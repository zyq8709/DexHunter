/*
 * Copyright (C) 2011 The Android Open Source Project
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

struct Thread;

/*
 * Raises the scheduling priority of the current thread.  Returns the
 * original priority if successful, or INT_MAX on failure.
 * Use os_lowerThreadPriority to undo.
 *
 * TODO: does the GC really need this?
 */
int os_raiseThreadPriority();

/*
 * Sets the current thread scheduling priority. Used to undo the effects
 * of an earlier call to os_raiseThreadPriority.
 *
 * TODO: does the GC really need this?
 */
void os_lowerThreadPriority(int oldThreadPriority);

/*
 * Changes the priority of a system thread to match that of the Thread object.
 *
 * We map a priority value from 1-10 to Linux "nice" values, where lower
 * numbers indicate higher priority.
 */
void os_changeThreadPriority(Thread* thread, int newPriority);

/*
 * Returns the thread priority for the current thread by querying the system.
 * This is useful when attaching a thread through JNI.
 *
 * Returns a value from 1 to 10 (compatible with java.lang.Thread values).
 */
int os_getThreadPriorityFromSystem();
