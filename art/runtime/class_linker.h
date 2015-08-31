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

#ifndef ART_RUNTIME_CLASS_LINKER_H_
#define ART_RUNTIME_CLASS_LINKER_H_

#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/mutex.h"
#include "dex_file.h"
#include "gtest/gtest.h"
#include "root_visitor.h"
#include "oat_file.h"

namespace art {
namespace gc {
namespace space {
  class ImageSpace;
}  // namespace space
}  // namespace gc
namespace mirror {
  class ClassLoader;
  class DexCache;
  class DexCacheTest_Open_Test;
  class IfTable;
  template<class T> class ObjectArray;
  class StackTraceElement;
}  // namespace mirror

class InternTable;
class ObjectLock;
template<class T> class SirtRef;

typedef bool (ClassVisitor)(mirror::Class* c, void* arg);

class ClassLinker {
 public:
  // Creates the class linker by bootstrapping from dex files.
  static ClassLinker* CreateFromCompiler(const std::vector<const DexFile*>& boot_class_path,
                                         InternTable* intern_table)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Creates the class linker from an image.
  static ClassLinker* CreateFromImage(InternTable* intern_table)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ~ClassLinker();

  bool IsInBootClassPath(const char* descriptor);

  // Finds a class by its descriptor, loading it if necessary.
  // If class_loader is null, searches boot_class_path_.
  mirror::Class* FindClass(const char* descriptor, mirror::ClassLoader* class_loader)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  mirror::Class* FindSystemClass(const char* descriptor)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Define a new a class based on a ClassDef from a DexFile
  mirror::Class* DefineClass(const char* descriptor, mirror::ClassLoader* class_loader,
                             const DexFile& dex_file, const DexFile::ClassDef& dex_class_def)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Finds a class by its descriptor, returning NULL if it isn't wasn't loaded
  // by the given 'class_loader'.
  mirror::Class* LookupClass(const char* descriptor, const mirror::ClassLoader* class_loader)
      LOCKS_EXCLUDED(Locks::classlinker_classes_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Finds all the classes with the given descriptor, regardless of ClassLoader.
  void LookupClasses(const char* descriptor, std::vector<mirror::Class*>& classes)
      LOCKS_EXCLUDED(Locks::classlinker_classes_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  mirror::Class* FindPrimitiveClass(char type) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // General class unloading is not supported, this is used to prune
  // unwanted classes during image writing.
  bool RemoveClass(const char* descriptor, const mirror::ClassLoader* class_loader)
      LOCKS_EXCLUDED(Locks::classlinker_classes_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void DumpAllClasses(int flags)
      LOCKS_EXCLUDED(Locks::classlinker_classes_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void DumpForSigQuit(std::ostream& os)
      LOCKS_EXCLUDED(Locks::classlinker_classes_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  size_t NumLoadedClasses()
      LOCKS_EXCLUDED(Locks::classlinker_classes_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Resolve a String with the given index from the DexFile, storing the
  // result in the DexCache. The referrer is used to identify the
  // target DexCache and ClassLoader to use for resolution.
  mirror::String* ResolveString(uint32_t string_idx, const mirror::ArtMethod* referrer)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Resolve a String with the given index from the DexFile, storing the
  // result in the DexCache.
  mirror::String* ResolveString(const DexFile& dex_file, uint32_t string_idx,
                                mirror::DexCache* dex_cache)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Resolve a Type with the given index from the DexFile, storing the
  // result in the DexCache. The referrer is used to identity the
  // target DexCache and ClassLoader to use for resolution.
  mirror::Class* ResolveType(const DexFile& dex_file, uint16_t type_idx,
                             const mirror::Class* referrer)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return ResolveType(dex_file,
                       type_idx,
                       referrer->GetDexCache(),
                       referrer->GetClassLoader());
  }

  // Resolve a Type with the given index from the DexFile, storing the
  // result in the DexCache. The referrer is used to identify the
  // target DexCache and ClassLoader to use for resolution.
  mirror::Class* ResolveType(uint16_t type_idx, const mirror::ArtMethod* referrer)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  mirror::Class* ResolveType(uint16_t type_idx, const mirror::ArtField* referrer)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Resolve a type with the given ID from the DexFile, storing the
  // result in DexCache. The ClassLoader is used to search for the
  // type, since it may be referenced from but not contained within
  // the given DexFile.
  mirror::Class* ResolveType(const DexFile& dex_file,
                             uint16_t type_idx,
                             mirror::DexCache* dex_cache,
                             mirror::ClassLoader* class_loader)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Resolve a method with a given ID from the DexFile, storing the
  // result in DexCache. The ClassLinker and ClassLoader are used as
  // in ResolveType. What is unique is the method type argument which
  // is used to determine if this method is a direct, static, or
  // virtual method.
  mirror::ArtMethod* ResolveMethod(const DexFile& dex_file,
                                   uint32_t method_idx,
                                   mirror::DexCache* dex_cache,
                                   mirror::ClassLoader* class_loader,
                                   const mirror::ArtMethod* referrer,
                                   InvokeType type)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  mirror::ArtMethod* ResolveMethod(uint32_t method_idx, const mirror::ArtMethod* referrer,
                                   InvokeType type)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  mirror::ArtField* ResolveField(uint32_t field_idx, const mirror::ArtMethod* referrer,
                                 bool is_static)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Resolve a field with a given ID from the DexFile, storing the
  // result in DexCache. The ClassLinker and ClassLoader are used as
  // in ResolveType. What is unique is the is_static argument which is
  // used to determine if we are resolving a static or non-static
  // field.
  mirror::ArtField* ResolveField(const DexFile& dex_file,
                                 uint32_t field_idx,
                                 mirror::DexCache* dex_cache,
                                 mirror::ClassLoader* class_loader,
                                 bool is_static)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Resolve a field with a given ID from the DexFile, storing the
  // result in DexCache. The ClassLinker and ClassLoader are used as
  // in ResolveType. No is_static argument is provided so that Java
  // field resolution semantics are followed.
  mirror::ArtField* ResolveFieldJLS(const DexFile& dex_file,
                                    uint32_t field_idx,
                                    mirror::DexCache* dex_cache,
                                    mirror::ClassLoader* class_loader)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Get shorty from method index without resolution. Used to do handlerization.
  const char* MethodShorty(uint32_t method_idx, mirror::ArtMethod* referrer, uint32_t* length)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Returns true on success, false if there's an exception pending.
  // can_run_clinit=false allows the compiler to attempt to init a class,
  // given the restriction that no <clinit> execution is possible.
  bool EnsureInitialized(mirror::Class* c, bool can_run_clinit, bool can_init_fields)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Initializes classes that have instances in the image but that have
  // <clinit> methods so they could not be initialized by the compiler.
  void RunRootClinits() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void RegisterDexFile(const DexFile& dex_file)
      LOCKS_EXCLUDED(dex_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void RegisterDexFile(const DexFile& dex_file, SirtRef<mirror::DexCache>& dex_cache)
      LOCKS_EXCLUDED(dex_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void RegisterOatFile(const OatFile& oat_file)
      LOCKS_EXCLUDED(dex_lock_);

  const std::vector<const DexFile*>& GetBootClassPath() {
    return boot_class_path_;
  }

  void VisitClasses(ClassVisitor* visitor, void* arg)
      LOCKS_EXCLUDED(dex_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  // Less efficient variant of VisitClasses that doesn't hold the classlinker_classes_lock_
  // when calling the visitor.
  void VisitClassesWithoutClassesLock(ClassVisitor* visitor, void* arg)
      LOCKS_EXCLUDED(dex_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void VisitRoots(RootVisitor* visitor, void* arg, bool only_dirty, bool clean_dirty)
      LOCKS_EXCLUDED(Locks::classlinker_classes_lock_, dex_lock_);

  mirror::DexCache* FindDexCache(const DexFile& dex_file) const
      LOCKS_EXCLUDED(dex_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  bool IsDexFileRegistered(const DexFile& dex_file) const
      LOCKS_EXCLUDED(dex_lock_);
  void FixupDexCaches(mirror::ArtMethod* resolution_method) const
      LOCKS_EXCLUDED(dex_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Generate an oat file from a dex file
  bool GenerateOatFile(const std::string& dex_filename,
                       int oat_fd,
                       const std::string& oat_cache_filename);

  const OatFile* FindOatFileFromOatLocation(const std::string& location)
      LOCKS_EXCLUDED(dex_lock_);

  const OatFile* FindOatFileFromOatLocationLocked(const std::string& location)
      SHARED_LOCKS_REQUIRED(dex_lock_);

  // Finds the oat file for a dex location, generating the oat file if
  // it is missing or out of date. Returns the DexFile from within the
  // created oat file.
  const DexFile* FindOrCreateOatFileForDexLocation(const std::string& dex_location,
                                                   uint32_t dex_location_checksum,
                                                   const std::string& oat_location)
      LOCKS_EXCLUDED(dex_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  const DexFile* FindOrCreateOatFileForDexLocationLocked(const std::string& dex_location,
                                                         uint32_t dex_location_checksum,
                                                         const std::string& oat_location)
      EXCLUSIVE_LOCKS_REQUIRED(dex_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  // Find a DexFile within an OatFile given a DexFile location. Note
  // that this returns null if the location checksum of the DexFile
  // does not match the OatFile.
  const DexFile* FindDexFileInOatFileFromDexLocation(const std::string& location,
                                                     uint32_t location_checksum)
      LOCKS_EXCLUDED(dex_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);


  // Returns true if oat file contains the dex file with the given location and checksum.
  static bool VerifyOatFileChecksums(const OatFile* oat_file,
                                     const std::string& dex_location,
                                     uint32_t dex_location_checksum)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // TODO: replace this with multiple methods that allocate the correct managed type.
  template <class T>
  mirror::ObjectArray<T>* AllocObjectArray(Thread* self, size_t length)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  mirror::ObjectArray<mirror::Class>* AllocClassArray(Thread* self, size_t length)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  mirror::ObjectArray<mirror::String>* AllocStringArray(Thread* self, size_t length)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  mirror::ObjectArray<mirror::ArtMethod>* AllocArtMethodArray(Thread* self, size_t length)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  mirror::IfTable* AllocIfTable(Thread* self, size_t ifcount)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  mirror::ObjectArray<mirror::ArtField>* AllocArtFieldArray(Thread* self, size_t length)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  mirror::ObjectArray<mirror::StackTraceElement>* AllocStackTraceElementArray(Thread* self,
                                                                              size_t length)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void VerifyClass(mirror::Class* klass) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  bool VerifyClassUsingOatFile(const DexFile& dex_file, mirror::Class* klass,
                               mirror::Class::Status& oat_file_class_status)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void ResolveClassExceptionHandlerTypes(const DexFile& dex_file, mirror::Class* klass)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void ResolveMethodExceptionHandlerTypes(const DexFile& dex_file, mirror::ArtMethod* klass)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  mirror::Class* CreateProxyClass(mirror::String* name, mirror::ObjectArray<mirror::Class>* interfaces,
                                  mirror::ClassLoader* loader,
                                  mirror::ObjectArray<mirror::ArtMethod>* methods,
                                  mirror::ObjectArray<mirror::ObjectArray<mirror::Class> >* throws)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  std::string GetDescriptorForProxy(const mirror::Class* proxy_class)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  mirror::ArtMethod* FindMethodForProxy(const mirror::Class* proxy_class,
                                        const mirror::ArtMethod* proxy_method)
      LOCKS_EXCLUDED(dex_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Get the oat code for a method when its class isn't yet initialized
  const void* GetOatCodeFor(const mirror::ArtMethod* method)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Get the oat code for a method from a method index.
  const void* GetOatCodeFor(const DexFile& dex_file, uint16_t class_def_idx, uint32_t method_idx)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  pid_t GetClassesLockOwner();  // For SignalCatcher.
  pid_t GetDexLockOwner();  // For SignalCatcher.

  const void* GetPortableResolutionTrampoline() const {
    return portable_resolution_trampoline_;
  }

  const void* GetQuickResolutionTrampoline() const {
    return quick_resolution_trampoline_;
  }

  InternTable* GetInternTable() const {
    return intern_table_;
  }

  // Attempts to insert a class into a class table.  Returns NULL if
  // the class was inserted, otherwise returns an existing class with
  // the same descriptor and ClassLoader.
  mirror::Class* InsertClass(const char* descriptor, mirror::Class* klass, size_t hash)
      LOCKS_EXCLUDED(Locks::classlinker_classes_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 private:
  explicit ClassLinker(InternTable*);

  const OatFile::OatMethod GetOatMethodFor(const mirror::ArtMethod* method)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Initialize class linker by bootstraping from dex files
  void InitFromCompiler(const std::vector<const DexFile*>& boot_class_path)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Initialize class linker from one or more images.
  void InitFromImage() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  OatFile& GetImageOatFile(gc::space::ImageSpace* space)
      LOCKS_EXCLUDED(dex_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void FinishInit() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // For early bootstrapping by Init
  mirror::Class* AllocClass(Thread* self, mirror::Class* java_lang_Class, size_t class_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Alloc* convenience functions to avoid needing to pass in mirror::Class*
  // values that are known to the ClassLinker such as
  // kObjectArrayClass and kJavaLangString etc.
  mirror::Class* AllocClass(Thread* self, size_t class_size) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  mirror::DexCache* AllocDexCache(Thread* self, const DexFile& dex_file)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  mirror::ArtField* AllocArtField(Thread* self) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  mirror::ArtMethod* AllocArtMethod(Thread* self) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  mirror::Class* CreatePrimitiveClass(Thread* self, Primitive::Type type)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  mirror::Class* InitializePrimitiveClass(mirror::Class* primitive_class, Primitive::Type type)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);


  mirror::Class* CreateArrayClass(const char* descriptor, mirror::ClassLoader* class_loader)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void AppendToBootClassPath(const DexFile& dex_file)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void AppendToBootClassPath(const DexFile& dex_file, SirtRef<mirror::DexCache>& dex_cache)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void ConstructFieldMap(const DexFile& dex_file, const DexFile::ClassDef& dex_class_def,
                         mirror::Class* c, SafeMap<uint32_t, mirror::ArtField*>& field_map)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  size_t SizeOfClass(const DexFile& dex_file,
                     const DexFile::ClassDef& dex_class_def);

  void LoadClass(const DexFile& dex_file,
                 const DexFile::ClassDef& dex_class_def,
                 SirtRef<mirror::Class>& klass,
                 mirror::ClassLoader* class_loader)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void LoadField(const DexFile& dex_file, const ClassDataItemIterator& it,
                 SirtRef<mirror::Class>& klass, SirtRef<mirror::ArtField>& dst)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  mirror::ArtMethod* LoadMethod(Thread* self, const DexFile& dex_file,
                                const ClassDataItemIterator& dex_method,
                                SirtRef<mirror::Class>& klass)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void FixupStaticTrampolines(mirror::Class* klass) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Finds the associated oat class for a dex_file and descriptor
  const OatFile::OatClass* GetOatClass(const DexFile& dex_file, uint16_t class_def_idx)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void RegisterDexFileLocked(const DexFile& dex_file, SirtRef<mirror::DexCache>& dex_cache)
      EXCLUSIVE_LOCKS_REQUIRED(dex_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  bool IsDexFileRegisteredLocked(const DexFile& dex_file) const SHARED_LOCKS_REQUIRED(dex_lock_);
  void RegisterOatFileLocked(const OatFile& oat_file) EXCLUSIVE_LOCKS_REQUIRED(dex_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(dex_lock_);

  bool InitializeClass(mirror::Class* klass, bool can_run_clinit, bool can_init_parents)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  bool WaitForInitializeClass(mirror::Class* klass, Thread* self, ObjectLock& lock);
  bool ValidateSuperClassDescriptors(const mirror::Class* klass)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool IsSameDescriptorInDifferentClassContexts(const char* descriptor,
                                                const mirror::Class* klass1,
                                                const mirror::Class* klass2)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool IsSameMethodSignatureInDifferentClassContexts(const mirror::ArtMethod* method,
                                                     const mirror::Class* klass1,
                                                     const mirror::Class* klass2)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool LinkClass(SirtRef<mirror::Class>& klass, mirror::ObjectArray<mirror::Class>* interfaces,
                 Thread* self)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool LinkSuperClass(SirtRef<mirror::Class>& klass)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool LoadSuperAndInterfaces(SirtRef<mirror::Class>& klass, const DexFile& dex_file)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool LinkMethods(SirtRef<mirror::Class>& klass, mirror::ObjectArray<mirror::Class>* interfaces)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool LinkVirtualMethods(SirtRef<mirror::Class>& klass)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool LinkInterfaceMethods(SirtRef<mirror::Class>& klass,
                            mirror::ObjectArray<mirror::Class>* interfaces)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool LinkStaticFields(SirtRef<mirror::Class>& klass)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  bool LinkInstanceFields(SirtRef<mirror::Class>& klass)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  bool LinkFields(SirtRef<mirror::Class>& klass, bool is_static)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);


  void CreateReferenceInstanceOffsets(SirtRef<mirror::Class>& klass)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void CreateReferenceStaticOffsets(SirtRef<mirror::Class>& klass)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void CreateReferenceOffsets(SirtRef<mirror::Class>& klass, bool is_static,
                              uint32_t reference_offsets)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // For use by ImageWriter to find DexCaches for its roots
  const std::vector<mirror::DexCache*>& GetDexCaches() {
    return dex_caches_;
  }

  const OatFile* FindOpenedOatFileForDexFile(const DexFile& dex_file)
      LOCKS_EXCLUDED(dex_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  const OatFile* FindOpenedOatFileFromDexLocation(const std::string& dex_location,
                                                  uint32_t dex_location_checksum)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_, dex_lock_);
  const OatFile* FindOpenedOatFileFromOatLocation(const std::string& oat_location)
      SHARED_LOCKS_REQUIRED(dex_lock_);
  const DexFile* FindDexFileInOatLocation(const std::string& dex_location,
                                          uint32_t dex_location_checksum,
                                          const std::string& oat_location)
      EXCLUSIVE_LOCKS_REQUIRED(dex_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  const DexFile* VerifyAndOpenDexFileFromOatFile(const OatFile* oat_file,
                                                 const std::string& dex_location,
                                                 uint32_t dex_location_checksum)
      EXCLUSIVE_LOCKS_REQUIRED(dex_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  mirror::ArtMethod* CreateProxyConstructor(Thread* self, SirtRef<mirror::Class>& klass,
                                            mirror::Class* proxy_class)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  mirror::ArtMethod* CreateProxyMethod(Thread* self, SirtRef<mirror::Class>& klass,
                                       SirtRef<mirror::ArtMethod>& prototype)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  std::vector<const DexFile*> boot_class_path_;

  mutable ReaderWriterMutex dex_lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  std::vector<mirror::DexCache*> dex_caches_ GUARDED_BY(dex_lock_);
  std::vector<const OatFile*> oat_files_ GUARDED_BY(dex_lock_);


  // multimap from a string hash code of a class descriptor to
  // mirror::Class* instances. Results should be compared for a matching
  // Class::descriptor_ and Class::class_loader_.
  typedef std::multimap<size_t, mirror::Class*> Table;
  Table class_table_ GUARDED_BY(Locks::classlinker_classes_lock_);

  // Do we need to search dex caches to find image classes?
  bool dex_cache_image_class_lookup_required_;
  // Number of times we've searched dex caches for a class. After a certain number of misses we move
  // the classes into the class_table_ to avoid dex cache based searches.
  AtomicInteger failed_dex_cache_class_lookups_;

  mirror::Class* LookupClassFromTableLocked(const char* descriptor,
                                            const mirror::ClassLoader* class_loader,
                                            size_t hash)
      SHARED_LOCKS_REQUIRED(Locks::classlinker_classes_lock_, Locks::mutator_lock_);

  void MoveImageClassesToClassTable() LOCKS_EXCLUDED(Locks::classlinker_classes_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  mirror::Class* LookupClassFromImage(const char* descriptor)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // indexes into class_roots_.
  // needs to be kept in sync with class_roots_descriptors_.
  enum ClassRoot {
    kJavaLangClass,
    kJavaLangObject,
    kClassArrayClass,
    kObjectArrayClass,
    kJavaLangString,
    kJavaLangDexCache,
    kJavaLangRefReference,
    kJavaLangReflectArtField,
    kJavaLangReflectArtMethod,
    kJavaLangReflectProxy,
    kJavaLangStringArrayClass,
    kJavaLangReflectArtFieldArrayClass,
    kJavaLangReflectArtMethodArrayClass,
    kJavaLangClassLoader,
    kJavaLangThrowable,
    kJavaLangClassNotFoundException,
    kJavaLangStackTraceElement,
    kPrimitiveBoolean,
    kPrimitiveByte,
    kPrimitiveChar,
    kPrimitiveDouble,
    kPrimitiveFloat,
    kPrimitiveInt,
    kPrimitiveLong,
    kPrimitiveShort,
    kPrimitiveVoid,
    kBooleanArrayClass,
    kByteArrayClass,
    kCharArrayClass,
    kDoubleArrayClass,
    kFloatArrayClass,
    kIntArrayClass,
    kLongArrayClass,
    kShortArrayClass,
    kJavaLangStackTraceElementArrayClass,
    kClassRootsMax,
  };
  mirror::ObjectArray<mirror::Class>* class_roots_;

  mirror::Class* GetClassRoot(ClassRoot class_root) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void SetClassRoot(ClassRoot class_root, mirror::Class* klass)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  mirror::ObjectArray<mirror::Class>* GetClassRoots() {
    DCHECK(class_roots_ != NULL);
    return class_roots_;
  }

  static const char* class_roots_descriptors_[];

  const char* GetClassRootDescriptor(ClassRoot class_root) {
    const char* descriptor = class_roots_descriptors_[class_root];
    CHECK(descriptor != NULL);
    return descriptor;
  }

  mirror::IfTable* array_iftable_;

  bool init_done_;
  bool dex_caches_dirty_ GUARDED_BY(dex_lock_);
  bool class_table_dirty_ GUARDED_BY(Locks::classlinker_classes_lock_);

  InternTable* intern_table_;

  const void* portable_resolution_trampoline_;
  const void* quick_resolution_trampoline_;

  friend class ImageWriter;  // for GetClassRoots
  FRIEND_TEST(ClassLinkerTest, ClassRootDescriptors);
  FRIEND_TEST(mirror::DexCacheTest, Open);
  FRIEND_TEST(ExceptionTest, FindExceptionHandler);
  FRIEND_TEST(ObjectTest, AllocObjectArray);
  DISALLOW_COPY_AND_ASSIGN(ClassLinker);
};

}  // namespace art

#endif  // ART_RUNTIME_CLASS_LINKER_H_
