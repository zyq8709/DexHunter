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

#include "image_writer.h"

#include <sys/stat.h>

#include <vector>

#include "base/logging.h"
#include "base/unix_file/fd_file.h"
#include "class_linker.h"
#include "compiled_method.h"
#include "dex_file-inl.h"
#include "driver/compiler_driver.h"
#include "elf_writer.h"
#include "gc/accounting/card_table-inl.h"
#include "gc/accounting/heap_bitmap.h"
#include "gc/accounting/space_bitmap-inl.h"
#include "gc/heap.h"
#include "gc/space/large_object_space.h"
#include "gc/space/space-inl.h"
#include "globals.h"
#include "image.h"
#include "intern_table.h"
#include "mirror/art_field-inl.h"
#include "mirror/art_method-inl.h"
#include "mirror/array-inl.h"
#include "mirror/class-inl.h"
#include "mirror/class_loader.h"
#include "mirror/dex_cache-inl.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "oat.h"
#include "oat_file.h"
#include "object_utils.h"
#include "runtime.h"
#include "scoped_thread_state_change.h"
#include "sirt_ref.h"
#include "UniquePtr.h"
#include "utils.h"

using ::art::mirror::ArtField;
using ::art::mirror::ArtMethod;
using ::art::mirror::Class;
using ::art::mirror::DexCache;
using ::art::mirror::EntryPointFromInterpreter;
using ::art::mirror::Object;
using ::art::mirror::ObjectArray;
using ::art::mirror::String;

namespace art {

bool ImageWriter::Write(const std::string& image_filename,
                        uintptr_t image_begin,
                        const std::string& oat_filename,
                        const std::string& oat_location) {
  CHECK(!image_filename.empty());

  CHECK_NE(image_begin, 0U);
  image_begin_ = reinterpret_cast<byte*>(image_begin);

  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  const std::vector<DexCache*>& all_dex_caches = class_linker->GetDexCaches();
  dex_caches_.insert(all_dex_caches.begin(), all_dex_caches.end());

  UniquePtr<File> oat_file(OS::OpenFileReadWrite(oat_filename.c_str()));
  if (oat_file.get() == NULL) {
    LOG(ERROR) << "Failed to open oat file " << oat_filename << " for " << oat_location;
    return false;
  }
  oat_file_ = OatFile::OpenWritable(oat_file.get(), oat_location);
  if (oat_file_ == NULL) {
    LOG(ERROR) << "Failed to open writable oat file " << oat_filename << " for " << oat_location;
    return false;
  }
  class_linker->RegisterOatFile(*oat_file_);

  interpreter_to_interpreter_bridge_offset_ =
      oat_file_->GetOatHeader().GetInterpreterToInterpreterBridgeOffset();
  interpreter_to_compiled_code_bridge_offset_ =
      oat_file_->GetOatHeader().GetInterpreterToCompiledCodeBridgeOffset();

  jni_dlsym_lookup_offset_ = oat_file_->GetOatHeader().GetJniDlsymLookupOffset();

  portable_resolution_trampoline_offset_ =
      oat_file_->GetOatHeader().GetPortableResolutionTrampolineOffset();
  portable_to_interpreter_bridge_offset_ =
      oat_file_->GetOatHeader().GetPortableToInterpreterBridgeOffset();

  quick_resolution_trampoline_offset_ =
      oat_file_->GetOatHeader().GetQuickResolutionTrampolineOffset();
  quick_to_interpreter_bridge_offset_ =
      oat_file_->GetOatHeader().GetQuickToInterpreterBridgeOffset();
  {
    Thread::Current()->TransitionFromSuspendedToRunnable();
    PruneNonImageClasses();  // Remove junk
    ComputeLazyFieldsForImageClasses();  // Add useful information
    ComputeEagerResolvedStrings();
    Thread::Current()->TransitionFromRunnableToSuspended(kNative);
  }
  gc::Heap* heap = Runtime::Current()->GetHeap();
  heap->CollectGarbage(false);  // Remove garbage.
  // Trim size of alloc spaces.
  for (const auto& space : heap->GetContinuousSpaces()) {
    if (space->IsDlMallocSpace()) {
      space->AsDlMallocSpace()->Trim();
    }
  }

  if (!AllocMemory()) {
    return false;
  }
#ifndef NDEBUG
  {  // NOLINT(whitespace/braces)
    ScopedObjectAccess soa(Thread::Current());
    CheckNonImageClassesRemoved();
  }
#endif
  Thread::Current()->TransitionFromSuspendedToRunnable();
  size_t oat_loaded_size = 0;
  size_t oat_data_offset = 0;
  ElfWriter::GetOatElfInformation(oat_file.get(), oat_loaded_size, oat_data_offset);
  CalculateNewObjectOffsets(oat_loaded_size, oat_data_offset);
  CopyAndFixupObjects();
  PatchOatCodeAndMethods();
  // Record allocations into the image bitmap.
  RecordImageAllocations();
  Thread::Current()->TransitionFromRunnableToSuspended(kNative);

  UniquePtr<File> image_file(OS::CreateEmptyFile(image_filename.c_str()));
  ImageHeader* image_header = reinterpret_cast<ImageHeader*>(image_->Begin());
  if (image_file.get() == NULL) {
    LOG(ERROR) << "Failed to open image file " << image_filename;
    return false;
  }
  if (fchmod(image_file->Fd(), 0644) != 0) {
    PLOG(ERROR) << "Failed to make image file world readable: " << image_filename;
    return EXIT_FAILURE;
  }

  // Write out the image.
  CHECK_EQ(image_end_, image_header->GetImageSize());
  if (!image_file->WriteFully(image_->Begin(), image_end_)) {
    PLOG(ERROR) << "Failed to write image file " << image_filename;
    return false;
  }

  // Write out the image bitmap at the page aligned start of the image end.
  CHECK_ALIGNED(image_header->GetImageBitmapOffset(), kPageSize);
  if (!image_file->Write(reinterpret_cast<char*>(image_bitmap_->Begin()),
                         image_header->GetImageBitmapSize(),
                         image_header->GetImageBitmapOffset())) {
    PLOG(ERROR) << "Failed to write image file " << image_filename;
    return false;
  }

  return true;
}

void ImageWriter::RecordImageAllocations() {
  uint64_t start_time = NanoTime();
  CHECK(image_bitmap_.get() != nullptr);
  for (const auto& it : offsets_) {
    mirror::Object* obj = reinterpret_cast<mirror::Object*>(image_->Begin() + it.second);
    DCHECK_ALIGNED(obj, kObjectAlignment);
    image_bitmap_->Set(obj);
  }
  LOG(INFO) << "RecordImageAllocations took " << PrettyDuration(NanoTime() - start_time);
}

bool ImageWriter::AllocMemory() {
  size_t size = 0;
  for (const auto& space : Runtime::Current()->GetHeap()->GetContinuousSpaces()) {
    if (space->IsDlMallocSpace()) {
      size += space->Size();
    }
  }

  int prot = PROT_READ | PROT_WRITE;
  size_t length = RoundUp(size, kPageSize);
  image_.reset(MemMap::MapAnonymous("image writer image", NULL, length, prot));
  if (image_.get() == NULL) {
    LOG(ERROR) << "Failed to allocate memory for image file generation";
    return false;
  }
  return true;
}

void ImageWriter::ComputeLazyFieldsForImageClasses() {
  Runtime* runtime = Runtime::Current();
  ClassLinker* class_linker = runtime->GetClassLinker();
  class_linker->VisitClassesWithoutClassesLock(ComputeLazyFieldsForClassesVisitor, NULL);
}

bool ImageWriter::ComputeLazyFieldsForClassesVisitor(Class* c, void* /*arg*/) {
  c->ComputeName();
  return true;
}

void ImageWriter::ComputeEagerResolvedStringsCallback(Object* obj, void* arg) {
  if (!obj->GetClass()->IsStringClass()) {
    return;
  }
  String* string = obj->AsString();
  const uint16_t* utf16_string = string->GetCharArray()->GetData() + string->GetOffset();
  ImageWriter* writer = reinterpret_cast<ImageWriter*>(arg);
  for (DexCache* dex_cache : writer->dex_caches_) {
    const DexFile& dex_file = *dex_cache->GetDexFile();
    const DexFile::StringId* string_id = dex_file.FindStringId(utf16_string);
    if (string_id != NULL) {
      // This string occurs in this dex file, assign the dex cache entry.
      uint32_t string_idx = dex_file.GetIndexForStringId(*string_id);
      if (dex_cache->GetResolvedString(string_idx) == NULL) {
        dex_cache->SetResolvedString(string_idx, string);
      }
    }
  }
}

void ImageWriter::ComputeEagerResolvedStrings()
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  // TODO: Check image spaces only?
  gc::Heap* heap = Runtime::Current()->GetHeap();
  WriterMutexLock mu(Thread::Current(), *Locks::heap_bitmap_lock_);
  heap->FlushAllocStack();
  heap->GetLiveBitmap()->Walk(ComputeEagerResolvedStringsCallback, this);
}

bool ImageWriter::IsImageClass(const Class* klass) {
  return compiler_driver_.IsImageClass(ClassHelper(klass).GetDescriptor());
}

struct NonImageClasses {
  ImageWriter* image_writer;
  std::set<std::string>* non_image_classes;
};

void ImageWriter::PruneNonImageClasses() {
  if (compiler_driver_.GetImageClasses() == NULL) {
    return;
  }
  Runtime* runtime = Runtime::Current();
  ClassLinker* class_linker = runtime->GetClassLinker();

  // Make a list of classes we would like to prune.
  std::set<std::string> non_image_classes;
  NonImageClasses context;
  context.image_writer = this;
  context.non_image_classes = &non_image_classes;
  class_linker->VisitClasses(NonImageClassesVisitor, &context);

  // Remove the undesired classes from the class roots.
  for (const std::string& it : non_image_classes) {
    class_linker->RemoveClass(it.c_str(), NULL);
  }

  // Clear references to removed classes from the DexCaches.
  ArtMethod* resolution_method = runtime->GetResolutionMethod();
  for (DexCache* dex_cache : dex_caches_) {
    for (size_t i = 0; i < dex_cache->NumResolvedTypes(); i++) {
      Class* klass = dex_cache->GetResolvedType(i);
      if (klass != NULL && !IsImageClass(klass)) {
        dex_cache->SetResolvedType(i, NULL);
        dex_cache->GetInitializedStaticStorage()->Set(i, NULL);
      }
    }
    for (size_t i = 0; i < dex_cache->NumResolvedMethods(); i++) {
      ArtMethod* method = dex_cache->GetResolvedMethod(i);
      if (method != NULL && !IsImageClass(method->GetDeclaringClass())) {
        dex_cache->SetResolvedMethod(i, resolution_method);
      }
    }
    for (size_t i = 0; i < dex_cache->NumResolvedFields(); i++) {
      ArtField* field = dex_cache->GetResolvedField(i);
      if (field != NULL && !IsImageClass(field->GetDeclaringClass())) {
        dex_cache->SetResolvedField(i, NULL);
      }
    }
  }
}

bool ImageWriter::NonImageClassesVisitor(Class* klass, void* arg) {
  NonImageClasses* context = reinterpret_cast<NonImageClasses*>(arg);
  if (!context->image_writer->IsImageClass(klass)) {
    context->non_image_classes->insert(ClassHelper(klass).GetDescriptor());
  }
  return true;
}

void ImageWriter::CheckNonImageClassesRemoved()
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  if (compiler_driver_.GetImageClasses() == NULL) {
    return;
  }

  gc::Heap* heap = Runtime::Current()->GetHeap();
  Thread* self = Thread::Current();
  {
    WriterMutexLock mu(self, *Locks::heap_bitmap_lock_);
    heap->FlushAllocStack();
  }

  ReaderMutexLock mu(self, *Locks::heap_bitmap_lock_);
  heap->GetLiveBitmap()->Walk(CheckNonImageClassesRemovedCallback, this);
}

void ImageWriter::CheckNonImageClassesRemovedCallback(Object* obj, void* arg) {
  ImageWriter* image_writer = reinterpret_cast<ImageWriter*>(arg);
  if (!obj->IsClass()) {
    return;
  }
  Class* klass = obj->AsClass();
  if (!image_writer->IsImageClass(klass)) {
    image_writer->DumpImageClasses();
    CHECK(image_writer->IsImageClass(klass)) << ClassHelper(klass).GetDescriptor()
                                             << " " << PrettyDescriptor(klass);
  }
}

void ImageWriter::DumpImageClasses() {
  CompilerDriver::DescriptorSet* image_classes = compiler_driver_.GetImageClasses();
  CHECK(image_classes != NULL);
  for (const std::string& image_class : *image_classes) {
    LOG(INFO) << " " << image_class;
  }
}

void ImageWriter::CalculateNewObjectOffsetsCallback(Object* obj, void* arg) {
  DCHECK(obj != NULL);
  DCHECK(arg != NULL);
  ImageWriter* image_writer = reinterpret_cast<ImageWriter*>(arg);

  // if it is a string, we want to intern it if its not interned.
  if (obj->GetClass()->IsStringClass()) {
    // we must be an interned string that was forward referenced and already assigned
    if (image_writer->IsImageOffsetAssigned(obj)) {
      DCHECK_EQ(obj, obj->AsString()->Intern());
      return;
    }
    SirtRef<String> interned(Thread::Current(), obj->AsString()->Intern());
    if (obj != interned.get()) {
      if (!image_writer->IsImageOffsetAssigned(interned.get())) {
        // interned obj is after us, allocate its location early
        image_writer->AssignImageOffset(interned.get());
      }
      // point those looking for this object to the interned version.
      image_writer->SetImageOffset(obj, image_writer->GetImageOffset(interned.get()));
      return;
    }
    // else (obj == interned), nothing to do but fall through to the normal case
  }

  image_writer->AssignImageOffset(obj);
}

ObjectArray<Object>* ImageWriter::CreateImageRoots() const {
  Runtime* runtime = Runtime::Current();
  ClassLinker* class_linker = runtime->GetClassLinker();
  Class* object_array_class = class_linker->FindSystemClass("[Ljava/lang/Object;");
  Thread* self = Thread::Current();

  // build an Object[] of all the DexCaches used in the source_space_
  ObjectArray<Object>* dex_caches = ObjectArray<Object>::Alloc(self, object_array_class,
                                                               dex_caches_.size());
  int i = 0;
  for (DexCache* dex_cache : dex_caches_) {
    dex_caches->Set(i++, dex_cache);
  }

  // build an Object[] of the roots needed to restore the runtime
  SirtRef<ObjectArray<Object> >
      image_roots(self,
                  ObjectArray<Object>::Alloc(self, object_array_class,
                                             ImageHeader::kImageRootsMax));
  image_roots->Set(ImageHeader::kResolutionMethod, runtime->GetResolutionMethod());
  image_roots->Set(ImageHeader::kCalleeSaveMethod,
                   runtime->GetCalleeSaveMethod(Runtime::kSaveAll));
  image_roots->Set(ImageHeader::kRefsOnlySaveMethod,
                   runtime->GetCalleeSaveMethod(Runtime::kRefsOnly));
  image_roots->Set(ImageHeader::kRefsAndArgsSaveMethod,
                   runtime->GetCalleeSaveMethod(Runtime::kRefsAndArgs));
  image_roots->Set(ImageHeader::kOatLocation,
                   String::AllocFromModifiedUtf8(self, oat_file_->GetLocation().c_str()));
  image_roots->Set(ImageHeader::kDexCaches,
                   dex_caches);
  image_roots->Set(ImageHeader::kClassRoots,
                   class_linker->GetClassRoots());
  for (int i = 0; i < ImageHeader::kImageRootsMax; i++) {
    CHECK(image_roots->Get(i) != NULL);
  }
  return image_roots.get();
}

void ImageWriter::CalculateNewObjectOffsets(size_t oat_loaded_size, size_t oat_data_offset) {
  CHECK_NE(0U, oat_loaded_size);
  Thread* self = Thread::Current();
  SirtRef<ObjectArray<Object> > image_roots(self, CreateImageRoots());

  gc::Heap* heap = Runtime::Current()->GetHeap();
  const auto& spaces = heap->GetContinuousSpaces();
  DCHECK(!spaces.empty());
  DCHECK_EQ(0U, image_end_);

  // Leave space for the header, but do not write it yet, we need to
  // know where image_roots is going to end up
  image_end_ += RoundUp(sizeof(ImageHeader), 8);  // 64-bit-alignment

  {
    WriterMutexLock mu(self, *Locks::heap_bitmap_lock_);
    heap->FlushAllocStack();
    // TODO: Image spaces only?
    // TODO: Add InOrderWalk to heap bitmap.
    const char* old = self->StartAssertNoThreadSuspension("ImageWriter");
    DCHECK(heap->GetLargeObjectsSpace()->GetLiveObjects()->IsEmpty());
    for (const auto& space : spaces) {
      space->GetLiveBitmap()->InOrderWalk(CalculateNewObjectOffsetsCallback, this);
      DCHECK_LT(image_end_, image_->Size());
    }
    self->EndAssertNoThreadSuspension(old);
  }

  // Create the image bitmap.
  image_bitmap_.reset(gc::accounting::SpaceBitmap::Create("image bitmap", image_->Begin(),
                                                          image_end_));
  const byte* oat_file_begin = image_begin_ + RoundUp(image_end_, kPageSize);
  const byte* oat_file_end = oat_file_begin + oat_loaded_size;
  oat_data_begin_ = oat_file_begin + oat_data_offset;
  const byte* oat_data_end = oat_data_begin_ + oat_file_->Size();

  // Return to write header at start of image with future location of image_roots. At this point,
  // image_end_ is the size of the image (excluding bitmaps).
  ImageHeader image_header(reinterpret_cast<uint32_t>(image_begin_),
                           static_cast<uint32_t>(image_end_),
                           RoundUp(image_end_, kPageSize),
                           image_bitmap_->Size(),
                           reinterpret_cast<uint32_t>(GetImageAddress(image_roots.get())),
                           oat_file_->GetOatHeader().GetChecksum(),
                           reinterpret_cast<uint32_t>(oat_file_begin),
                           reinterpret_cast<uint32_t>(oat_data_begin_),
                           reinterpret_cast<uint32_t>(oat_data_end),
                           reinterpret_cast<uint32_t>(oat_file_end));
  memcpy(image_->Begin(), &image_header, sizeof(image_header));

  // Note that image_end_ is left at end of used space
}

void ImageWriter::CopyAndFixupObjects()
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  Thread* self = Thread::Current();
  const char* old_cause = self->StartAssertNoThreadSuspension("ImageWriter");
  gc::Heap* heap = Runtime::Current()->GetHeap();
  // TODO: heap validation can't handle this fix up pass
  heap->DisableObjectValidation();
  // TODO: Image spaces only?
  WriterMutexLock mu(self, *Locks::heap_bitmap_lock_);
  heap->FlushAllocStack();
  heap->GetLiveBitmap()->Walk(CopyAndFixupObjectsCallback, this);
  self->EndAssertNoThreadSuspension(old_cause);
}

void ImageWriter::CopyAndFixupObjectsCallback(Object* object, void* arg) {
  DCHECK(object != NULL);
  DCHECK(arg != NULL);
  const Object* obj = object;
  ImageWriter* image_writer = reinterpret_cast<ImageWriter*>(arg);

  // see GetLocalAddress for similar computation
  size_t offset = image_writer->GetImageOffset(obj);
  byte* dst = image_writer->image_->Begin() + offset;
  const byte* src = reinterpret_cast<const byte*>(obj);
  size_t n = obj->SizeOf();
  DCHECK_LT(offset + n, image_writer->image_->Size());
  memcpy(dst, src, n);
  Object* copy = reinterpret_cast<Object*>(dst);
  copy->SetField32(Object::MonitorOffset(), 0, false);  // We may have inflated the lock during compilation.
  image_writer->FixupObject(obj, copy);
}

void ImageWriter::FixupObject(const Object* orig, Object* copy) {
  DCHECK(orig != NULL);
  DCHECK(copy != NULL);
  copy->SetClass(down_cast<Class*>(GetImageAddress(orig->GetClass())));
  // TODO: special case init of pointers to malloc data (or removal of these pointers)
  if (orig->IsClass()) {
    FixupClass(orig->AsClass(), down_cast<Class*>(copy));
  } else if (orig->IsObjectArray()) {
    FixupObjectArray(orig->AsObjectArray<Object>(), down_cast<ObjectArray<Object>*>(copy));
  } else if (orig->IsArtMethod()) {
    FixupMethod(orig->AsArtMethod(), down_cast<ArtMethod*>(copy));
  } else {
    FixupInstanceFields(orig, copy);
  }
}

void ImageWriter::FixupClass(const Class* orig, Class* copy) {
  FixupInstanceFields(orig, copy);
  FixupStaticFields(orig, copy);
}

void ImageWriter::FixupMethod(const ArtMethod* orig, ArtMethod* copy) {
  FixupInstanceFields(orig, copy);

  // OatWriter replaces the code_ with an offset value. Here we re-adjust to a pointer relative to
  // oat_begin_

  // The resolution method has a special trampoline to call.
  if (UNLIKELY(orig == Runtime::Current()->GetResolutionMethod())) {
#if defined(ART_USE_PORTABLE_COMPILER)
    copy->SetEntryPointFromCompiledCode(GetOatAddress(portable_resolution_trampoline_offset_));
#else
    copy->SetEntryPointFromCompiledCode(GetOatAddress(quick_resolution_trampoline_offset_));
#endif
  } else {
    // We assume all methods have code. If they don't currently then we set them to the use the
    // resolution trampoline. Abstract methods never have code and so we need to make sure their
    // use results in an AbstractMethodError. We use the interpreter to achieve this.
    if (UNLIKELY(orig->IsAbstract())) {
#if defined(ART_USE_PORTABLE_COMPILER)
      copy->SetEntryPointFromCompiledCode(GetOatAddress(portable_to_interpreter_bridge_offset_));
#else
      copy->SetEntryPointFromCompiledCode(GetOatAddress(quick_to_interpreter_bridge_offset_));
#endif
      copy->SetEntryPointFromInterpreter(reinterpret_cast<EntryPointFromInterpreter*>
      (GetOatAddress(interpreter_to_interpreter_bridge_offset_)));
    } else {
      copy->SetEntryPointFromInterpreter(reinterpret_cast<EntryPointFromInterpreter*>
      (GetOatAddress(interpreter_to_compiled_code_bridge_offset_)));
      // Use original code if it exists. Otherwise, set the code pointer to the resolution
      // trampoline.
      const byte* code = GetOatAddress(orig->GetOatCodeOffset());
      if (code != NULL) {
        copy->SetEntryPointFromCompiledCode(code);
      } else {
#if defined(ART_USE_PORTABLE_COMPILER)
        copy->SetEntryPointFromCompiledCode(GetOatAddress(portable_resolution_trampoline_offset_));
#else
        copy->SetEntryPointFromCompiledCode(GetOatAddress(quick_resolution_trampoline_offset_));
#endif
      }
      if (orig->IsNative()) {
        // The native method's pointer is set to a stub to lookup via dlsym.
        // Note this is not the code_ pointer, that is handled above.
        copy->SetNativeMethod(GetOatAddress(jni_dlsym_lookup_offset_));
      } else {
        // Normal (non-abstract non-native) methods have various tables to relocate.
        uint32_t mapping_table_off = orig->GetOatMappingTableOffset();
        const byte* mapping_table = GetOatAddress(mapping_table_off);
        copy->SetMappingTable(mapping_table);

        uint32_t vmap_table_offset = orig->GetOatVmapTableOffset();
        const byte* vmap_table = GetOatAddress(vmap_table_offset);
        copy->SetVmapTable(vmap_table);

        uint32_t native_gc_map_offset = orig->GetOatNativeGcMapOffset();
        const byte* native_gc_map = GetOatAddress(native_gc_map_offset);
        copy->SetNativeGcMap(reinterpret_cast<const uint8_t*>(native_gc_map));
      }
    }
  }
}

void ImageWriter::FixupObjectArray(const ObjectArray<Object>* orig, ObjectArray<Object>* copy) {
  for (int32_t i = 0; i < orig->GetLength(); ++i) {
    const Object* element = orig->Get(i);
    copy->SetPtrWithoutChecks(i, GetImageAddress(element));
  }
}

void ImageWriter::FixupInstanceFields(const Object* orig, Object* copy) {
  DCHECK(orig != NULL);
  DCHECK(copy != NULL);
  Class* klass = orig->GetClass();
  DCHECK(klass != NULL);
  FixupFields(orig,
              copy,
              klass->GetReferenceInstanceOffsets(),
              false);
}

void ImageWriter::FixupStaticFields(const Class* orig, Class* copy) {
  DCHECK(orig != NULL);
  DCHECK(copy != NULL);
  FixupFields(orig,
              copy,
              orig->GetReferenceStaticOffsets(),
              true);
}

void ImageWriter::FixupFields(const Object* orig,
                              Object* copy,
                              uint32_t ref_offsets,
                              bool is_static) {
  if (ref_offsets != CLASS_WALK_SUPER) {
    // Found a reference offset bitmap.  Fixup the specified offsets.
    while (ref_offsets != 0) {
      size_t right_shift = CLZ(ref_offsets);
      MemberOffset byte_offset = CLASS_OFFSET_FROM_CLZ(right_shift);
      const Object* ref = orig->GetFieldObject<const Object*>(byte_offset, false);
      // Use SetFieldPtr to avoid card marking since we are writing to the image.
      copy->SetFieldPtr(byte_offset, GetImageAddress(ref), false);
      ref_offsets &= ~(CLASS_HIGH_BIT >> right_shift);
    }
  } else {
    // There is no reference offset bitmap.  In the non-static case,
    // walk up the class inheritance hierarchy and find reference
    // offsets the hard way. In the static case, just consider this
    // class.
    for (const Class *klass = is_static ? orig->AsClass() : orig->GetClass();
         klass != NULL;
         klass = is_static ? NULL : klass->GetSuperClass()) {
      size_t num_reference_fields = (is_static
                                     ? klass->NumReferenceStaticFields()
                                     : klass->NumReferenceInstanceFields());
      for (size_t i = 0; i < num_reference_fields; ++i) {
        ArtField* field = (is_static
                           ? klass->GetStaticField(i)
                           : klass->GetInstanceField(i));
        MemberOffset field_offset = field->GetOffset();
        const Object* ref = orig->GetFieldObject<const Object*>(field_offset, false);
        // Use SetFieldPtr to avoid card marking since we are writing to the image.
        copy->SetFieldPtr(field_offset, GetImageAddress(ref), false);
      }
    }
  }
  if (!is_static && orig->IsReferenceInstance()) {
    // Fix-up referent, that isn't marked as an object field, for References.
    ArtField* field = orig->GetClass()->FindInstanceField("referent", "Ljava/lang/Object;");
    MemberOffset field_offset = field->GetOffset();
    const Object* ref = orig->GetFieldObject<const Object*>(field_offset, false);
    // Use SetFieldPtr to avoid card marking since we are writing to the image.
    copy->SetFieldPtr(field_offset, GetImageAddress(ref), false);
  }
}

static ArtMethod* GetTargetMethod(const CompilerDriver::PatchInformation* patch)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  DexCache* dex_cache = class_linker->FindDexCache(patch->GetDexFile());
  ArtMethod* method = class_linker->ResolveMethod(patch->GetDexFile(),
                                                  patch->GetTargetMethodIdx(),
                                                  dex_cache,
                                                  NULL,
                                                  NULL,
                                                  patch->GetTargetInvokeType());
  CHECK(method != NULL)
    << patch->GetDexFile().GetLocation() << " " << patch->GetTargetMethodIdx();
  CHECK(!method->IsRuntimeMethod())
    << patch->GetDexFile().GetLocation() << " " << patch->GetTargetMethodIdx();
  CHECK(dex_cache->GetResolvedMethods()->Get(patch->GetTargetMethodIdx()) == method)
    << patch->GetDexFile().GetLocation() << " " << patch->GetReferrerMethodIdx() << " "
    << PrettyMethod(dex_cache->GetResolvedMethods()->Get(patch->GetTargetMethodIdx())) << " "
    << PrettyMethod(method);
  return method;
}

void ImageWriter::PatchOatCodeAndMethods() {
  Thread* self = Thread::Current();
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  const char* old_cause = self->StartAssertNoThreadSuspension("ImageWriter");

  typedef std::vector<const CompilerDriver::PatchInformation*> Patches;
  const Patches& code_to_patch = compiler_driver_.GetCodeToPatch();
  for (size_t i = 0; i < code_to_patch.size(); i++) {
    const CompilerDriver::PatchInformation* patch = code_to_patch[i];
    ArtMethod* target = GetTargetMethod(patch);
    uint32_t code = reinterpret_cast<uint32_t>(class_linker->GetOatCodeFor(target));
    uint32_t code_base = reinterpret_cast<uint32_t>(&oat_file_->GetOatHeader());
    uint32_t code_offset = code - code_base;
    SetPatchLocation(patch, reinterpret_cast<uint32_t>(GetOatAddress(code_offset)));
  }

  const Patches& methods_to_patch = compiler_driver_.GetMethodsToPatch();
  for (size_t i = 0; i < methods_to_patch.size(); i++) {
    const CompilerDriver::PatchInformation* patch = methods_to_patch[i];
    ArtMethod* target = GetTargetMethod(patch);
    SetPatchLocation(patch, reinterpret_cast<uint32_t>(GetImageAddress(target)));
  }

  // Update the image header with the new checksum after patching
  ImageHeader* image_header = reinterpret_cast<ImageHeader*>(image_->Begin());
  image_header->SetOatChecksum(oat_file_->GetOatHeader().GetChecksum());
  self->EndAssertNoThreadSuspension(old_cause);
}

void ImageWriter::SetPatchLocation(const CompilerDriver::PatchInformation* patch, uint32_t value) {
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  const void* oat_code = class_linker->GetOatCodeFor(patch->GetDexFile(),
                                                     patch->GetReferrerClassDefIdx(),
                                                     patch->GetReferrerMethodIdx());
  OatHeader& oat_header = const_cast<OatHeader&>(oat_file_->GetOatHeader());
  // TODO: make this Thumb2 specific
  uint8_t* base = reinterpret_cast<uint8_t*>(reinterpret_cast<uint32_t>(oat_code) & ~0x1);
  uint32_t* patch_location = reinterpret_cast<uint32_t*>(base + patch->GetLiteralOffset());
#ifndef NDEBUG
  const DexFile::MethodId& id = patch->GetDexFile().GetMethodId(patch->GetTargetMethodIdx());
  uint32_t expected = reinterpret_cast<uint32_t>(&id);
  uint32_t actual = *patch_location;
  CHECK(actual == expected || actual == value) << std::hex
    << "actual=" << actual
    << "expected=" << expected
    << "value=" << value;
#endif
  *patch_location = value;
  oat_header.UpdateChecksum(patch_location, sizeof(value));
}

}  // namespace art
