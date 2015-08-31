/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// sys/mount.h has to come before linux/fs.h due to redefinition of MS_RDONLY, MS_BIND, etc
#include <sys/mount.h>
#include <linux/fs.h>

#include <grp.h>
#include <paths.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "cutils/fs.h"
#include "cutils/multiuser.h"
#include "cutils/sched_policy.h"
#include "debugger.h"
#include "jni_internal.h"
#include "JNIHelp.h"
#include "ScopedLocalRef.h"
#include "ScopedPrimitiveArray.h"
#include "ScopedUtfChars.h"
#include "thread.h"

#if defined(HAVE_PRCTL)
#include <sys/prctl.h>
#endif

#include <selinux/android.h>

#if defined(__linux__)
#include <sys/personality.h>
#include <sys/utsname.h>
#include <sys/capability.h>
#endif

namespace art {

static pid_t gSystemServerPid = 0;

// Must match values in dalvik.system.Zygote.
enum MountExternalKind {
  MOUNT_EXTERNAL_NONE = 0,
  MOUNT_EXTERNAL_SINGLEUSER = 1,
  MOUNT_EXTERNAL_MULTIUSER = 2,
  MOUNT_EXTERNAL_MULTIUSER_ALL = 3,
};

// This signal handler is for zygote mode, since the zygote must reap its children
static void SigChldHandler(int /*signal_number*/) {
  pid_t pid;
  int status;

  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
     // Log process-death status that we care about.  In general it is
     // not safe to call LOG(...) from a signal handler because of
     // possible reentrancy.  However, we know a priori that the
     // current implementation of LOG() is safe to call from a SIGCHLD
     // handler in the zygote process.  If the LOG() implementation
     // changes its locking strategy or its use of syscalls within the
     // lazy-init critical section, its use here may become unsafe.
    if (WIFEXITED(status)) {
      if (WEXITSTATUS(status)) {
        LOG(INFO) << "Process " << pid << " exited cleanly (" << WEXITSTATUS(status) << ")";
      } else if (false) {
        LOG(INFO) << "Process " << pid << " exited cleanly (" << WEXITSTATUS(status) << ")";
      }
    } else if (WIFSIGNALED(status)) {
      if (WTERMSIG(status) != SIGKILL) {
        LOG(INFO) << "Process " << pid << " terminated by signal (" << WTERMSIG(status) << ")";
      } else if (false) {
        LOG(INFO) << "Process " << pid << " terminated by signal (" << WTERMSIG(status) << ")";
      }
#ifdef WCOREDUMP
      if (WCOREDUMP(status)) {
        LOG(INFO) << "Process " << pid << " dumped core";
      }
#endif /* ifdef WCOREDUMP */
    }

    // If the just-crashed process is the system_server, bring down zygote
    // so that it is restarted by init and system server will be restarted
    // from there.
    if (pid == gSystemServerPid) {
      LOG(ERROR) << "Exit zygote because system server (" << pid << ") has terminated";
      kill(getpid(), SIGKILL);
    }
  }

  if (pid < 0) {
    PLOG(WARNING) << "Zygote SIGCHLD error in waitpid";
  }
}

// Configures the SIGCHLD handler for the zygote process. This is configured
// very late, because earlier in the runtime we may fork() and exec()
// other processes, and we want to waitpid() for those rather than
// have them be harvested immediately.
//
// This ends up being called repeatedly before each fork(), but there's
// no real harm in that.
static void SetSigChldHandler() {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SigChldHandler;

  int err = sigaction(SIGCHLD, &sa, NULL);
  if (err < 0) {
    PLOG(WARNING) << "Error setting SIGCHLD handler";
  }
}

// Sets the SIGCHLD handler back to default behavior in zygote children.
static void UnsetSigChldHandler() {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SIG_DFL;

  int err = sigaction(SIGCHLD, &sa, NULL);
  if (err < 0) {
    PLOG(WARNING) << "Error unsetting SIGCHLD handler";
  }
}

// Calls POSIX setgroups() using the int[] object as an argument.
// A NULL argument is tolerated.
static void SetGids(JNIEnv* env, jintArray javaGids) {
  if (javaGids == NULL) {
    return;
  }

  COMPILE_ASSERT(sizeof(gid_t) == sizeof(jint), sizeof_gid_and_jint_are_differerent);
  ScopedIntArrayRO gids(env, javaGids);
  CHECK(gids.get() != NULL);
  int rc = setgroups(gids.size(), reinterpret_cast<const gid_t*>(&gids[0]));
  if (rc == -1) {
    PLOG(FATAL) << "setgroups failed";
  }
}

// Sets the resource limits via setrlimit(2) for the values in the
// two-dimensional array of integers that's passed in. The second dimension
// contains a tuple of length 3: (resource, rlim_cur, rlim_max). NULL is
// treated as an empty array.
static void SetRLimits(JNIEnv* env, jobjectArray javaRlimits) {
  if (javaRlimits == NULL) {
    return;
  }

  rlimit rlim;
  memset(&rlim, 0, sizeof(rlim));

  for (int i = 0; i < env->GetArrayLength(javaRlimits); ++i) {
    ScopedLocalRef<jobject> javaRlimitObject(env, env->GetObjectArrayElement(javaRlimits, i));
    ScopedIntArrayRO javaRlimit(env, reinterpret_cast<jintArray>(javaRlimitObject.get()));
    if (javaRlimit.size() != 3) {
      LOG(FATAL) << "rlimits array must have a second dimension of size 3";
    }

    rlim.rlim_cur = javaRlimit[1];
    rlim.rlim_max = javaRlimit[2];

    int rc = setrlimit(javaRlimit[0], &rlim);
    if (rc == -1) {
      PLOG(FATAL) << "setrlimit(" << javaRlimit[0] << ", "
                  << "{" << rlim.rlim_cur << ", " << rlim.rlim_max << "}) failed";
    }
  }
}

#if defined(HAVE_ANDROID_OS)

// The debug malloc library needs to know whether it's the zygote or a child.
extern "C" int gMallocLeakZygoteChild;

static void EnableDebugger() {
  // To let a non-privileged gdbserver attach to this
  // process, we must set our dumpable flag.
  if (prctl(PR_SET_DUMPABLE, 1, 0, 0, 0) == -1) {
    PLOG(ERROR) << "prctl(PR_SET_DUMPABLE) failed for pid " << getpid();
  }
  // We don't want core dumps, though, so set the core dump size to 0.
  rlimit rl;
  rl.rlim_cur = 0;
  rl.rlim_max = RLIM_INFINITY;
  if (setrlimit(RLIMIT_CORE, &rl) == -1) {
    PLOG(ERROR) << "setrlimit(RLIMIT_CORE) failed for pid " << getpid();
  }
}

static void EnableKeepCapabilities() {
  int rc = prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0);
  if (rc == -1) {
    PLOG(FATAL) << "prctl(PR_SET_KEEPCAPS) failed";
  }
}

static void DropCapabilitiesBoundingSet() {
  for (int i = 0; prctl(PR_CAPBSET_READ, i, 0, 0, 0) >= 0; i++) {
    if (i == CAP_NET_RAW) {
      // Don't break /system/bin/ping
      continue;
    }
    int rc = prctl(PR_CAPBSET_DROP, i, 0, 0, 0);
    if (rc == -1) {
      if (errno == EINVAL) {
        PLOG(ERROR) << "prctl(PR_CAPBSET_DROP) failed with EINVAL. Please verify "
                    << "your kernel is compiled with file capabilities support";
      } else {
        PLOG(FATAL) << "prctl(PR_CAPBSET_DROP) failed";
      }
    }
  }
}

static void SetCapabilities(int64_t permitted, int64_t effective) {
  __user_cap_header_struct capheader;
  __user_cap_data_struct capdata;

  memset(&capheader, 0, sizeof(capheader));
  memset(&capdata, 0, sizeof(capdata));

  capheader.version = _LINUX_CAPABILITY_VERSION;
  capheader.pid = 0;

  capdata.effective = effective;
  capdata.permitted = permitted;

  if (capset(&capheader, &capdata) != 0) {
    PLOG(FATAL) << "capset(" << permitted << ", " << effective << ") failed";
  }
}

static void SetSchedulerPolicy() {
  errno = -set_sched_policy(0, SP_DEFAULT);
  if (errno != 0) {
    PLOG(FATAL) << "set_sched_policy(0, SP_DEFAULT) failed";
  }
}

#else

static int gMallocLeakZygoteChild = 0;

static void EnableDebugger() {}
static void EnableKeepCapabilities() {}
static void DropCapabilitiesBoundingSet() {}
static void SetCapabilities(int64_t, int64_t) {}
static void SetSchedulerPolicy() {}

#endif

static void EnableDebugFeatures(uint32_t debug_flags) {
  // Must match values in dalvik.system.Zygote.
  enum {
    DEBUG_ENABLE_DEBUGGER           = 1,
    DEBUG_ENABLE_CHECKJNI           = 1 << 1,
    DEBUG_ENABLE_ASSERT             = 1 << 2,
    DEBUG_ENABLE_SAFEMODE           = 1 << 3,
    DEBUG_ENABLE_JNI_LOGGING        = 1 << 4,
  };

  if ((debug_flags & DEBUG_ENABLE_CHECKJNI) != 0) {
    Runtime* runtime = Runtime::Current();
    JavaVMExt* vm = runtime->GetJavaVM();
    if (!vm->check_jni) {
      LOG(DEBUG) << "Late-enabling -Xcheck:jni";
      vm->SetCheckJniEnabled(true);
      // There's only one thread running at this point, so only one JNIEnv to fix up.
      Thread::Current()->GetJniEnv()->SetCheckJniEnabled(true);
    } else {
      LOG(DEBUG) << "Not late-enabling -Xcheck:jni (already on)";
    }
    debug_flags &= ~DEBUG_ENABLE_CHECKJNI;
  }

  if ((debug_flags & DEBUG_ENABLE_JNI_LOGGING) != 0) {
    gLogVerbosity.third_party_jni = true;
    debug_flags &= ~DEBUG_ENABLE_JNI_LOGGING;
  }

  Dbg::SetJdwpAllowed((debug_flags & DEBUG_ENABLE_DEBUGGER) != 0);
  if ((debug_flags & DEBUG_ENABLE_DEBUGGER) != 0) {
    EnableDebugger();
  }
  debug_flags &= ~DEBUG_ENABLE_DEBUGGER;

  // These two are for backwards compatibility with Dalvik.
  debug_flags &= ~DEBUG_ENABLE_ASSERT;
  debug_flags &= ~DEBUG_ENABLE_SAFEMODE;

  if (debug_flags != 0) {
    LOG(ERROR) << StringPrintf("Unknown bits set in debug_flags: %#x", debug_flags);
  }
}

// Create a private mount namespace and bind mount appropriate emulated
// storage for the given user.
static bool MountEmulatedStorage(uid_t uid, jint mount_mode) {
  if (mount_mode == MOUNT_EXTERNAL_NONE) {
    return true;
  }

  // See storage config details at http://source.android.com/tech/storage/
  userid_t user_id = multiuser_get_user_id(uid);

  // Create a second private mount namespace for our process
  if (unshare(CLONE_NEWNS) == -1) {
      PLOG(WARNING) << "Failed to unshare()";
      return false;
  }

  // Create bind mounts to expose external storage
  if (mount_mode == MOUNT_EXTERNAL_MULTIUSER || mount_mode == MOUNT_EXTERNAL_MULTIUSER_ALL) {
    // These paths must already be created by init.rc
    const char* source = getenv("EMULATED_STORAGE_SOURCE");
    const char* target = getenv("EMULATED_STORAGE_TARGET");
    const char* legacy = getenv("EXTERNAL_STORAGE");
    if (source == NULL || target == NULL || legacy == NULL) {
      LOG(WARNING) << "Storage environment undefined; unable to provide external storage";
      return false;
    }

    // Prepare source paths

    // /mnt/shell/emulated/0
    std::string source_user(StringPrintf("%s/%d", source, user_id));
    // /storage/emulated/0
    std::string target_user(StringPrintf("%s/%d", target, user_id));

    if (fs_prepare_dir(source_user.c_str(), 0000, 0, 0) == -1
        || fs_prepare_dir(target_user.c_str(), 0000, 0, 0) == -1) {
      return false;
    }

    if (mount_mode == MOUNT_EXTERNAL_MULTIUSER_ALL) {
      // Mount entire external storage tree for all users
      if (mount(source, target, NULL, MS_BIND, NULL) == -1) {
        PLOG(WARNING) << "Failed to mount " << source << " to " << target;
        return false;
      }
    } else {
      // Only mount user-specific external storage
      if (mount(source_user.c_str(), target_user.c_str(), NULL, MS_BIND, NULL) == -1) {
        PLOG(WARNING) << "Failed to mount " << source_user << " to " << target_user;
        return false;
      }
    }

    if (fs_prepare_dir(legacy, 0000, 0, 0) == -1) {
        return false;
    }

    // Finally, mount user-specific path into place for legacy users
    if (mount(target_user.c_str(), legacy, NULL, MS_BIND | MS_REC, NULL) == -1) {
      PLOG(WARNING) << "Failed to mount " << target_user << " to " << legacy;
      return false;
    }
  } else {
    LOG(WARNING) << "Mount mode " << mount_mode << " unsupported";
    return false;
  }

  return true;
}

#if defined(__linux__)
static bool NeedsNoRandomizeWorkaround() {
#if !defined(__arm__)
    return false;
#else
    int major;
    int minor;
    struct utsname uts;
    if (uname(&uts) == -1) {
        return false;
    }

    if (sscanf(uts.release, "%d.%d", &major, &minor) != 2) {
        return false;
    }

    // Kernels before 3.4.* need the workaround.
    return (major < 3) || ((major == 3) && (minor < 4));
#endif
}
#endif

// Utility routine to fork zygote and specialize the child process.
static pid_t ForkAndSpecializeCommon(JNIEnv* env, uid_t uid, gid_t gid, jintArray javaGids,
                                     jint debug_flags, jobjectArray javaRlimits,
                                     jlong permittedCapabilities, jlong effectiveCapabilities,
                                     jint mount_external,
                                     jstring java_se_info, jstring java_se_name, bool is_system_server) {
  Runtime* runtime = Runtime::Current();
  CHECK(runtime->IsZygote()) << "runtime instance not started with -Xzygote";
  if (!runtime->PreZygoteFork()) {
    LOG(FATAL) << "pre-fork heap failed";
  }

  SetSigChldHandler();

  // Grab thread before fork potentially makes Thread::pthread_key_self_ unusable.
  Thread* self = Thread::Current();

  // dvmDumpLoaderStats("zygote");  // TODO: ?
  pid_t pid = fork();

  if (pid == 0) {
    // The child process.
    gMallocLeakZygoteChild = 1;

    // Keep capabilities across UID change, unless we're staying root.
    if (uid != 0) {
      EnableKeepCapabilities();
    }

    DropCapabilitiesBoundingSet();

    if (!MountEmulatedStorage(uid, mount_external)) {
      PLOG(WARNING) << "Failed to mount emulated storage";
      if (errno == ENOTCONN || errno == EROFS) {
        // When device is actively encrypting, we get ENOTCONN here
        // since FUSE was mounted before the framework restarted.
        // When encrypted device is booting, we get EROFS since
        // FUSE hasn't been created yet by init.
        // In either case, continue without external storage.
      } else {
        LOG(FATAL) << "Cannot continue without emulated storage";
      }
    }

    SetGids(env, javaGids);

    SetRLimits(env, javaRlimits);

    int rc = setresgid(gid, gid, gid);
    if (rc == -1) {
      PLOG(FATAL) << "setresgid(" << gid << ") failed";
    }

    rc = setresuid(uid, uid, uid);
    if (rc == -1) {
      PLOG(FATAL) << "setresuid(" << uid << ") failed";
    }

#if defined(__linux__)
    if (NeedsNoRandomizeWorkaround()) {
        // Work around ARM kernel ASLR lossage (http://b/5817320).
        int old_personality = personality(0xffffffff);
        int new_personality = personality(old_personality | ADDR_NO_RANDOMIZE);
        if (new_personality == -1) {
            PLOG(WARNING) << "personality(" << new_personality << ") failed";
        }
    }
#endif

    SetCapabilities(permittedCapabilities, effectiveCapabilities);

    SetSchedulerPolicy();

#if defined(HAVE_ANDROID_OS)
    {  // NOLINT(whitespace/braces)
      const char* se_info_c_str = NULL;
      UniquePtr<ScopedUtfChars> se_info;
      if (java_se_info != NULL) {
          se_info.reset(new ScopedUtfChars(env, java_se_info));
          se_info_c_str = se_info->c_str();
          CHECK(se_info_c_str != NULL);
      }
      const char* se_name_c_str = NULL;
      UniquePtr<ScopedUtfChars> se_name;
      if (java_se_name != NULL) {
          se_name.reset(new ScopedUtfChars(env, java_se_name));
          se_name_c_str = se_name->c_str();
          CHECK(se_name_c_str != NULL);
      }
      rc = selinux_android_setcontext(uid, is_system_server, se_info_c_str, se_name_c_str);
      if (rc == -1) {
        PLOG(FATAL) << "selinux_android_setcontext(" << uid << ", "
                    << (is_system_server ? "true" : "false") << ", "
                    << "\"" << se_info_c_str << "\", \"" << se_name_c_str << "\") failed";
      }
    }
#else
    UNUSED(is_system_server);
    UNUSED(java_se_info);
    UNUSED(java_se_name);
#endif

    // Our system thread ID, etc, has changed so reset Thread state.
    self->InitAfterFork();

    EnableDebugFeatures(debug_flags);

    UnsetSigChldHandler();
    runtime->DidForkFromZygote();
  } else if (pid > 0) {
    // the parent process
  }
  return pid;
}

static jint Zygote_nativeForkAndSpecialize(JNIEnv* env, jclass, jint uid, jint gid, jintArray gids,
                                           jint debug_flags, jobjectArray rlimits, jint mount_external,
                                           jstring se_info, jstring se_name) {
  return ForkAndSpecializeCommon(env, uid, gid, gids, debug_flags, rlimits, 0, 0, mount_external, se_info, se_name, false);
}

static jint Zygote_nativeForkSystemServer(JNIEnv* env, jclass, uid_t uid, gid_t gid, jintArray gids,
                                          jint debug_flags, jobjectArray rlimits,
                                          jlong permittedCapabilities, jlong effectiveCapabilities) {
  pid_t pid = ForkAndSpecializeCommon(env, uid, gid, gids,
                                      debug_flags, rlimits,
                                      permittedCapabilities, effectiveCapabilities,
                                      MOUNT_EXTERNAL_NONE, NULL, NULL, true);
  if (pid > 0) {
      // The zygote process checks whether the child process has died or not.
      LOG(INFO) << "System server process " << pid << " has been created";
      gSystemServerPid = pid;
      // There is a slight window that the system server process has crashed
      // but it went unnoticed because we haven't published its pid yet. So
      // we recheck here just to make sure that all is well.
      int status;
      if (waitpid(pid, &status, WNOHANG) == pid) {
          LOG(FATAL) << "System server process " << pid << " has died. Restarting Zygote!";
      }
  }
  return pid;
}

static JNINativeMethod gMethods[] = {
  NATIVE_METHOD(Zygote, nativeForkAndSpecialize, "(II[II[[IILjava/lang/String;Ljava/lang/String;)I"),
  NATIVE_METHOD(Zygote, nativeForkSystemServer, "(II[II[[IJJ)I"),
};

void register_dalvik_system_Zygote(JNIEnv* env) {
  REGISTER_NATIVE_METHODS("dalvik/system/Zygote");
}

}  // namespace art
