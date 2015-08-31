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

#include "mark_sweep.h"

#include <functional>
#include <numeric>
#include <climits>
#include <vector>

#include "base/bounded_fifo.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/mutex-inl.h"
#include "base/timing_logger.h"
#include "gc/accounting/card_table-inl.h"
#include "gc/accounting/heap_bitmap.h"
#include "gc/accounting/space_bitmap-inl.h"
#include "gc/heap.h"
#include "gc/space/image_space.h"
#include "gc/space/large_object_space.h"
#include "gc/space/space-inl.h"
#include "indirect_reference_table.h"
#include "intern_table.h"
#include "jni_internal.h"
#include "monitor.h"
#include "mark_sweep-inl.h"
#include "mirror/art_field.h"
#include "mirror/art_field-inl.h"
#include "mirror/class-inl.h"
#include "mirror/class_loader.h"
#include "mirror/dex_cache.h"
#include "mirror/object-inl.h"
#include "mirror/object_array.h"
#include "mirror/object_array-inl.h"
#include "runtime.h"
#include "thread-inl.h"
#include "thread_list.h"
#include "verifier/method_verifier.h"

using ::art::mirror::ArtField;
using ::art::mirror::Class;
using ::art::mirror::Object;
using ::art::mirror::ObjectArray;

namespace art {
namespace gc {
namespace collector {

// Performance options.
constexpr bool kUseRecursiveMark = false;
constexpr bool kUseMarkStackPrefetch = true;
constexpr size_t kSweepArrayChunkFreeSize = 1024;

// Parallelism options.
constexpr bool kParallelCardScan = true;
constexpr bool kParallelRecursiveMark = true;
// Don't attempt to parallelize mark stack processing unless the mark stack is at least n
// elements. This is temporary until we reduce the overhead caused by allocating tasks, etc.. Not
// having this can add overhead in ProcessReferences since we may end up doing many calls of
// ProcessMarkStack with very small mark stacks.
constexpr size_t kMinimumParallelMarkStackSize = 128;
constexpr bool kParallelProcessMarkStack = true;

// Profiling and information flags.
constexpr bool kCountClassesMarked = false;
constexpr bool kProfileLargeObjects = false;
constexpr bool kMeasureOverhead = false;
constexpr bool kCountTasks = false;
constexpr bool kCountJavaLangRefs = false;

// Turn off kCheckLocks when profiling the GC since it slows the GC down by up to 40%.
constexpr bool kCheckLocks = kDebugLocking;

void MarkSweep::ImmuneSpace(space::ContinuousSpace* space) {
  // Bind live to mark bitmap if necessary.
  if (space->GetLiveBitmap() != space->GetMarkBitmap()) {
    BindLiveToMarkBitmap(space);
  }

  // Add the space to the immune region.
  if (immune_begin_ == NULL) {
    DCHECK(immune_end_ == NULL);
    SetImmuneRange(reinterpret_cast<Object*>(space->Begin()),
                   reinterpret_cast<Object*>(space->End()));
  } else {
    const space::ContinuousSpace* prev_space = nullptr;
    // Find out if the previous space is immune.
    for (space::ContinuousSpace* cur_space : GetHeap()->GetContinuousSpaces()) {
      if (cur_space == space) {
        break;
      }
      prev_space = cur_space;
    }
    // If previous space was immune, then extend the immune region. Relies on continuous spaces
    // being sorted by Heap::AddContinuousSpace.
    if (prev_space != NULL &&
        immune_begin_ <= reinterpret_cast<Object*>(prev_space->Begin()) &&
        immune_end_ >= reinterpret_cast<Object*>(prev_space->End())) {
      immune_begin_ = std::min(reinterpret_cast<Object*>(space->Begin()), immune_begin_);
      immune_end_ = std::max(reinterpret_cast<Object*>(space->End()), immune_end_);
    }
  }
}

void MarkSweep::BindBitmaps() {
  timings_.StartSplit("BindBitmaps");
  WriterMutexLock mu(Thread::Current(), *Locks::heap_bitmap_lock_);
  // Mark all of the spaces we never collect as immune.
  for (const auto& space : GetHeap()->GetContinuousSpaces()) {
    if (space->GetGcRetentionPolicy() == space::kGcRetentionPolicyNeverCollect) {
      ImmuneSpace(space);
    }
  }
  timings_.EndSplit();
}

MarkSweep::MarkSweep(Heap* heap, bool is_concurrent, const std::string& name_prefix)
    : GarbageCollector(heap,
                       name_prefix + (name_prefix.empty() ? "" : " ") +
                       (is_concurrent ? "concurrent mark sweep": "mark sweep")),
      current_mark_bitmap_(NULL),
      java_lang_Class_(NULL),
      mark_stack_(NULL),
      immune_begin_(NULL),
      immune_end_(NULL),
      soft_reference_list_(NULL),
      weak_reference_list_(NULL),
      finalizer_reference_list_(NULL),
      phantom_reference_list_(NULL),
      cleared_reference_list_(NULL),
      gc_barrier_(new Barrier(0)),
      large_object_lock_("mark sweep large object lock", kMarkSweepLargeObjectLock),
      mark_stack_lock_("mark sweep mark stack lock", kMarkSweepMarkStackLock),
      is_concurrent_(is_concurrent),
      clear_soft_references_(false) {
}

void MarkSweep::InitializePhase() {
  timings_.Reset();
  base::TimingLogger::ScopedSplit split("InitializePhase", &timings_);
  mark_stack_ = heap_->mark_stack_.get();
  DCHECK(mark_stack_ != nullptr);
  SetImmuneRange(nullptr, nullptr);
  soft_reference_list_ = nullptr;
  weak_reference_list_ = nullptr;
  finalizer_reference_list_ = nullptr;
  phantom_reference_list_ = nullptr;
  cleared_reference_list_ = nullptr;
  freed_bytes_ = 0;
  freed_large_object_bytes_ = 0;
  freed_objects_ = 0;
  freed_large_objects_ = 0;
  class_count_ = 0;
  array_count_ = 0;
  other_count_ = 0;
  large_object_test_ = 0;
  large_object_mark_ = 0;
  classes_marked_ = 0;
  overhead_time_ = 0;
  work_chunks_created_ = 0;
  work_chunks_deleted_ = 0;
  reference_count_ = 0;
  java_lang_Class_ = Class::GetJavaLangClass();
  CHECK(java_lang_Class_ != nullptr);

  FindDefaultMarkBitmap();

  // Do any pre GC verification.
  timings_.NewSplit("PreGcVerification");
  heap_->PreGcVerification(this);
}

void MarkSweep::ProcessReferences(Thread* self) {
  base::TimingLogger::ScopedSplit split("ProcessReferences", &timings_);
  WriterMutexLock mu(self, *Locks::heap_bitmap_lock_);
  ProcessReferences(&soft_reference_list_, clear_soft_references_, &weak_reference_list_,
                    &finalizer_reference_list_, &phantom_reference_list_);
}

bool MarkSweep::HandleDirtyObjectsPhase() {
  base::TimingLogger::ScopedSplit split("HandleDirtyObjectsPhase", &timings_);
  Thread* self = Thread::Current();
  Locks::mutator_lock_->AssertExclusiveHeld(self);

  {
    WriterMutexLock mu(self, *Locks::heap_bitmap_lock_);

    // Re-mark root set.
    ReMarkRoots();

    // Scan dirty objects, this is only required if we are not doing concurrent GC.
    RecursiveMarkDirtyObjects(true, accounting::CardTable::kCardDirty);
  }

  ProcessReferences(self);

  // Only need to do this if we have the card mark verification on, and only during concurrent GC.
  if (GetHeap()->verify_missing_card_marks_ || GetHeap()->verify_pre_gc_heap_||
      GetHeap()->verify_post_gc_heap_) {
    WriterMutexLock mu(self, *Locks::heap_bitmap_lock_);
    // This second sweep makes sure that we don't have any objects in the live stack which point to
    // freed objects. These cause problems since their references may be previously freed objects.
    SweepArray(GetHeap()->allocation_stack_.get(), false);
  }

  timings_.StartSplit("PreSweepingGcVerification");
  heap_->PreSweepingGcVerification(this);
  timings_.EndSplit();

  // Ensure that nobody inserted items in the live stack after we swapped the stacks.
  ReaderMutexLock mu(self, *Locks::heap_bitmap_lock_);
  CHECK_GE(live_stack_freeze_size_, GetHeap()->GetLiveStack()->Size());

  // Disallow new system weaks to prevent a race which occurs when someone adds a new system
  // weak before we sweep them. Since this new system weak may not be marked, the GC may
  // incorrectly sweep it. This also fixes a race where interning may attempt to return a strong
  // reference to a string that is about to be swept.
  Runtime::Current()->DisallowNewSystemWeaks();
  return true;
}

bool MarkSweep::IsConcurrent() const {
  return is_concurrent_;
}

void MarkSweep::MarkingPhase() {
  base::TimingLogger::ScopedSplit split("MarkingPhase", &timings_);
  Thread* self = Thread::Current();

  BindBitmaps();
  FindDefaultMarkBitmap();

  // Process dirty cards and add dirty cards to mod union tables.
  heap_->ProcessCards(timings_);

  // Need to do this before the checkpoint since we don't want any threads to add references to
  // the live stack during the recursive mark.
  timings_.NewSplit("SwapStacks");
  heap_->SwapStacks();

  WriterMutexLock mu(self, *Locks::heap_bitmap_lock_);
  if (Locks::mutator_lock_->IsExclusiveHeld(self)) {
    // If we exclusively hold the mutator lock, all threads must be suspended.
    MarkRoots();
  } else {
    MarkThreadRoots(self);
    // At this point the live stack should no longer have any mutators which push into it.
    MarkNonThreadRoots();
  }
  live_stack_freeze_size_ = heap_->GetLiveStack()->Size();
  MarkConcurrentRoots();

  heap_->UpdateAndMarkModUnion(this, timings_, GetGcType());
  MarkReachableObjects();
}

void MarkSweep::MarkThreadRoots(Thread* self) {
  MarkRootsCheckpoint(self);
}

void MarkSweep::MarkReachableObjects() {
  // Mark everything allocated since the last as GC live so that we can sweep concurrently,
  // knowing that new allocations won't be marked as live.
  timings_.StartSplit("MarkStackAsLive");
  accounting::ObjectStack* live_stack = heap_->GetLiveStack();
  heap_->MarkAllocStack(heap_->alloc_space_->GetLiveBitmap(),
                        heap_->large_object_space_->GetLiveObjects(), live_stack);
  live_stack->Reset();
  timings_.EndSplit();
  // Recursively mark all the non-image bits set in the mark bitmap.
  RecursiveMark();
}

void MarkSweep::ReclaimPhase() {
  base::TimingLogger::ScopedSplit split("ReclaimPhase", &timings_);
  Thread* self = Thread::Current();

  if (!IsConcurrent()) {
    ProcessReferences(self);
  }

  {
    WriterMutexLock mu(self, *Locks::heap_bitmap_lock_);
    SweepSystemWeaks();
  }

  if (IsConcurrent()) {
    Runtime::Current()->AllowNewSystemWeaks();

    base::TimingLogger::ScopedSplit split("UnMarkAllocStack", &timings_);
    WriterMutexLock mu(self, *Locks::heap_bitmap_lock_);
    accounting::ObjectStack* allocation_stack = GetHeap()->allocation_stack_.get();
    // The allocation stack contains things allocated since the start of the GC. These may have been
    // marked during this GC meaning they won't be eligible for reclaiming in the next sticky GC.
    // Remove these objects from the mark bitmaps so that they will be eligible for sticky
    // collection.
    // There is a race here which is safely handled. Another thread such as the hprof could
    // have flushed the alloc stack after we resumed the threads. This is safe however, since
    // reseting the allocation stack zeros it out with madvise. This means that we will either
    // read NULLs or attempt to unmark a newly allocated object which will not be marked in the
    // first place.
    mirror::Object** end = allocation_stack->End();
    for (mirror::Object** it = allocation_stack->Begin(); it != end; ++it) {
      const Object* obj = *it;
      if (obj != NULL) {
        UnMarkObjectNonNull(obj);
      }
    }
  }

  // Before freeing anything, lets verify the heap.
  if (kIsDebugBuild) {
    ReaderMutexLock mu(self, *Locks::heap_bitmap_lock_);
    VerifyImageRoots();
  }

  {
    WriterMutexLock mu(self, *Locks::heap_bitmap_lock_);

    // Reclaim unmarked objects.
    Sweep(false);

    // Swap the live and mark bitmaps for each space which we modified space. This is an
    // optimization that enables us to not clear live bits inside of the sweep. Only swaps unbound
    // bitmaps.
    timings_.StartSplit("SwapBitmaps");
    SwapBitmaps();
    timings_.EndSplit();

    // Unbind the live and mark bitmaps.
    UnBindBitmaps();
  }
}

void MarkSweep::SetImmuneRange(Object* begin, Object* end) {
  immune_begin_ = begin;
  immune_end_ = end;
}

void MarkSweep::FindDefaultMarkBitmap() {
  base::TimingLogger::ScopedSplit split("FindDefaultMarkBitmap", &timings_);
  for (const auto& space : GetHeap()->GetContinuousSpaces()) {
    if (space->GetGcRetentionPolicy() == space::kGcRetentionPolicyAlwaysCollect) {
      current_mark_bitmap_ = space->GetMarkBitmap();
      CHECK(current_mark_bitmap_ != NULL);
      return;
    }
  }
  GetHeap()->DumpSpaces();
  LOG(FATAL) << "Could not find a default mark bitmap";
}

void MarkSweep::ExpandMarkStack() {
  ResizeMarkStack(mark_stack_->Capacity() * 2);
}

void MarkSweep::ResizeMarkStack(size_t new_size) {
  // Rare case, no need to have Thread::Current be a parameter.
  if (UNLIKELY(mark_stack_->Size() < mark_stack_->Capacity())) {
    // Someone else acquired the lock and expanded the mark stack before us.
    return;
  }
  std::vector<Object*> temp(mark_stack_->Begin(), mark_stack_->End());
  CHECK_LE(mark_stack_->Size(), new_size);
  mark_stack_->Resize(new_size);
  for (const auto& obj : temp) {
    mark_stack_->PushBack(obj);
  }
}

inline void MarkSweep::MarkObjectNonNullParallel(const Object* obj) {
  DCHECK(obj != NULL);
  if (MarkObjectParallel(obj)) {
    MutexLock mu(Thread::Current(), mark_stack_lock_);
    if (UNLIKELY(mark_stack_->Size() >= mark_stack_->Capacity())) {
      ExpandMarkStack();
    }
    // The object must be pushed on to the mark stack.
    mark_stack_->PushBack(const_cast<Object*>(obj));
  }
}

inline void MarkSweep::UnMarkObjectNonNull(const Object* obj) {
  DCHECK(!IsImmune(obj));
  // Try to take advantage of locality of references within a space, failing this find the space
  // the hard way.
  accounting::SpaceBitmap* object_bitmap = current_mark_bitmap_;
  if (UNLIKELY(!object_bitmap->HasAddress(obj))) {
    accounting::SpaceBitmap* new_bitmap = heap_->GetMarkBitmap()->GetContinuousSpaceBitmap(obj);
    if (LIKELY(new_bitmap != NULL)) {
      object_bitmap = new_bitmap;
    } else {
      MarkLargeObject(obj, false);
      return;
    }
  }

  DCHECK(object_bitmap->HasAddress(obj));
  object_bitmap->Clear(obj);
}

inline void MarkSweep::MarkObjectNonNull(const Object* obj) {
  DCHECK(obj != NULL);

  if (IsImmune(obj)) {
    DCHECK(IsMarked(obj));
    return;
  }

  // Try to take advantage of locality of references within a space, failing this find the space
  // the hard way.
  accounting::SpaceBitmap* object_bitmap = current_mark_bitmap_;
  if (UNLIKELY(!object_bitmap->HasAddress(obj))) {
    accounting::SpaceBitmap* new_bitmap = heap_->GetMarkBitmap()->GetContinuousSpaceBitmap(obj);
    if (LIKELY(new_bitmap != NULL)) {
      object_bitmap = new_bitmap;
    } else {
      MarkLargeObject(obj, true);
      return;
    }
  }

  // This object was not previously marked.
  if (!object_bitmap->Test(obj)) {
    object_bitmap->Set(obj);
    if (UNLIKELY(mark_stack_->Size() >= mark_stack_->Capacity())) {
      // Lock is not needed but is here anyways to please annotalysis.
      MutexLock mu(Thread::Current(), mark_stack_lock_);
      ExpandMarkStack();
    }
    // The object must be pushed on to the mark stack.
    mark_stack_->PushBack(const_cast<Object*>(obj));
  }
}

// Rare case, probably not worth inlining since it will increase instruction cache miss rate.
bool MarkSweep::MarkLargeObject(const Object* obj, bool set) {
  // TODO: support >1 discontinuous space.
  space::LargeObjectSpace* large_object_space = GetHeap()->GetLargeObjectsSpace();
  accounting::SpaceSetMap* large_objects = large_object_space->GetMarkObjects();
  if (kProfileLargeObjects) {
    ++large_object_test_;
  }
  if (UNLIKELY(!large_objects->Test(obj))) {
    if (!large_object_space->Contains(obj)) {
      LOG(ERROR) << "Tried to mark " << obj << " not contained by any spaces";
      LOG(ERROR) << "Attempting see if it's a bad root";
      VerifyRoots();
      LOG(FATAL) << "Can't mark bad root";
    }
    if (kProfileLargeObjects) {
      ++large_object_mark_;
    }
    if (set) {
      large_objects->Set(obj);
    } else {
      large_objects->Clear(obj);
    }
    return true;
  }
  return false;
}

inline bool MarkSweep::MarkObjectParallel(const Object* obj) {
  DCHECK(obj != NULL);

  if (IsImmune(obj)) {
    DCHECK(IsMarked(obj));
    return false;
  }

  // Try to take advantage of locality of references within a space, failing this find the space
  // the hard way.
  accounting::SpaceBitmap* object_bitmap = current_mark_bitmap_;
  if (UNLIKELY(!object_bitmap->HasAddress(obj))) {
    accounting::SpaceBitmap* new_bitmap = heap_->GetMarkBitmap()->GetContinuousSpaceBitmap(obj);
    if (new_bitmap != NULL) {
      object_bitmap = new_bitmap;
    } else {
      // TODO: Remove the Thread::Current here?
      // TODO: Convert this to some kind of atomic marking?
      MutexLock mu(Thread::Current(), large_object_lock_);
      return MarkLargeObject(obj, true);
    }
  }

  // Return true if the object was not previously marked.
  return !object_bitmap->AtomicTestAndSet(obj);
}

// Used to mark objects when recursing.  Recursion is done by moving
// the finger across the bitmaps in address order and marking child
// objects.  Any newly-marked objects whose addresses are lower than
// the finger won't be visited by the bitmap scan, so those objects
// need to be added to the mark stack.
inline void MarkSweep::MarkObject(const Object* obj) {
  if (obj != NULL) {
    MarkObjectNonNull(obj);
  }
}

void MarkSweep::MarkRoot(const Object* obj) {
  if (obj != NULL) {
    MarkObjectNonNull(obj);
  }
}

void MarkSweep::MarkRootParallelCallback(const Object* root, void* arg) {
  DCHECK(root != NULL);
  DCHECK(arg != NULL);
  reinterpret_cast<MarkSweep*>(arg)->MarkObjectNonNullParallel(root);
}

void MarkSweep::MarkObjectCallback(const Object* root, void* arg) {
  DCHECK(root != NULL);
  DCHECK(arg != NULL);
  MarkSweep* mark_sweep = reinterpret_cast<MarkSweep*>(arg);
  mark_sweep->MarkObjectNonNull(root);
}

void MarkSweep::ReMarkObjectVisitor(const Object* root, void* arg) {
  DCHECK(root != NULL);
  DCHECK(arg != NULL);
  MarkSweep* mark_sweep = reinterpret_cast<MarkSweep*>(arg);
  mark_sweep->MarkObjectNonNull(root);
}

void MarkSweep::VerifyRootCallback(const Object* root, void* arg, size_t vreg,
                                   const StackVisitor* visitor) {
  reinterpret_cast<MarkSweep*>(arg)->VerifyRoot(root, vreg, visitor);
}

void MarkSweep::VerifyRoot(const Object* root, size_t vreg, const StackVisitor* visitor) {
  // See if the root is on any space bitmap.
  if (GetHeap()->GetLiveBitmap()->GetContinuousSpaceBitmap(root) == NULL) {
    space::LargeObjectSpace* large_object_space = GetHeap()->GetLargeObjectsSpace();
    if (!large_object_space->Contains(root)) {
      LOG(ERROR) << "Found invalid root: " << root;
      if (visitor != NULL) {
        LOG(ERROR) << visitor->DescribeLocation() << " in VReg: " << vreg;
      }
    }
  }
}

void MarkSweep::VerifyRoots() {
  Runtime::Current()->GetThreadList()->VerifyRoots(VerifyRootCallback, this);
}

// Marks all objects in the root set.
void MarkSweep::MarkRoots() {
  timings_.StartSplit("MarkRoots");
  Runtime::Current()->VisitNonConcurrentRoots(MarkObjectCallback, this);
  timings_.EndSplit();
}

void MarkSweep::MarkNonThreadRoots() {
  timings_.StartSplit("MarkNonThreadRoots");
  Runtime::Current()->VisitNonThreadRoots(MarkObjectCallback, this);
  timings_.EndSplit();
}

void MarkSweep::MarkConcurrentRoots() {
  timings_.StartSplit("MarkConcurrentRoots");
  // Visit all runtime roots and clear dirty flags.
  Runtime::Current()->VisitConcurrentRoots(MarkObjectCallback, this, false, true);
  timings_.EndSplit();
}

void MarkSweep::CheckObject(const Object* obj) {
  DCHECK(obj != NULL);
  VisitObjectReferences(obj, [this](const Object* obj, const Object* ref, MemberOffset offset,
      bool is_static) NO_THREAD_SAFETY_ANALYSIS {
    Locks::heap_bitmap_lock_->AssertSharedHeld(Thread::Current());
    CheckReference(obj, ref, offset, is_static);
  });
}

void MarkSweep::VerifyImageRootVisitor(Object* root, void* arg) {
  DCHECK(root != NULL);
  DCHECK(arg != NULL);
  MarkSweep* mark_sweep = reinterpret_cast<MarkSweep*>(arg);
  DCHECK(mark_sweep->heap_->GetMarkBitmap()->Test(root));
  mark_sweep->CheckObject(root);
}

void MarkSweep::BindLiveToMarkBitmap(space::ContinuousSpace* space) {
  CHECK(space->IsDlMallocSpace());
  space::DlMallocSpace* alloc_space = space->AsDlMallocSpace();
  accounting::SpaceBitmap* live_bitmap = space->GetLiveBitmap();
  accounting::SpaceBitmap* mark_bitmap = alloc_space->mark_bitmap_.release();
  GetHeap()->GetMarkBitmap()->ReplaceBitmap(mark_bitmap, live_bitmap);
  alloc_space->temp_bitmap_.reset(mark_bitmap);
  alloc_space->mark_bitmap_.reset(live_bitmap);
}

class ScanObjectVisitor {
 public:
  explicit ScanObjectVisitor(MarkSweep* const mark_sweep) ALWAYS_INLINE
      : mark_sweep_(mark_sweep) {}

  // TODO: Fixme when anotatalysis works with visitors.
  void operator()(const Object* obj) const ALWAYS_INLINE NO_THREAD_SAFETY_ANALYSIS {
    if (kCheckLocks) {
      Locks::mutator_lock_->AssertSharedHeld(Thread::Current());
      Locks::heap_bitmap_lock_->AssertExclusiveHeld(Thread::Current());
    }
    mark_sweep_->ScanObject(obj);
  }

 private:
  MarkSweep* const mark_sweep_;
};

template <bool kUseFinger = false>
class MarkStackTask : public Task {
 public:
  MarkStackTask(ThreadPool* thread_pool, MarkSweep* mark_sweep, size_t mark_stack_size,
                const Object** mark_stack)
      : mark_sweep_(mark_sweep),
        thread_pool_(thread_pool),
        mark_stack_pos_(mark_stack_size) {
    // We may have to copy part of an existing mark stack when another mark stack overflows.
    if (mark_stack_size != 0) {
      DCHECK(mark_stack != NULL);
      // TODO: Check performance?
      std::copy(mark_stack, mark_stack + mark_stack_size, mark_stack_);
    }
    if (kCountTasks) {
      ++mark_sweep_->work_chunks_created_;
    }
  }

  static const size_t kMaxSize = 1 * KB;

 protected:
  class ScanObjectParallelVisitor {
   public:
    explicit ScanObjectParallelVisitor(MarkStackTask<kUseFinger>* chunk_task) ALWAYS_INLINE
        : chunk_task_(chunk_task) {}

    void operator()(const Object* obj) const {
      MarkSweep* mark_sweep = chunk_task_->mark_sweep_;
      mark_sweep->ScanObjectVisit(obj,
          [mark_sweep, this](const Object* /* obj */, const Object* ref,
              const MemberOffset& /* offset */, bool /* is_static */) ALWAYS_INLINE {
        if (ref != nullptr && mark_sweep->MarkObjectParallel(ref)) {
          if (kUseFinger) {
            android_memory_barrier();
            if (reinterpret_cast<uintptr_t>(ref) >=
                static_cast<uintptr_t>(mark_sweep->atomic_finger_)) {
              return;
            }
          }
          chunk_task_->MarkStackPush(ref);
        }
      });
    }

   private:
    MarkStackTask<kUseFinger>* const chunk_task_;
  };

  virtual ~MarkStackTask() {
    // Make sure that we have cleared our mark stack.
    DCHECK_EQ(mark_stack_pos_, 0U);
    if (kCountTasks) {
      ++mark_sweep_->work_chunks_deleted_;
    }
  }

  MarkSweep* const mark_sweep_;
  ThreadPool* const thread_pool_;
  // Thread local mark stack for this task.
  const Object* mark_stack_[kMaxSize];
  // Mark stack position.
  size_t mark_stack_pos_;

  void MarkStackPush(const Object* obj) ALWAYS_INLINE {
    if (UNLIKELY(mark_stack_pos_ == kMaxSize)) {
      // Mark stack overflow, give 1/2 the stack to the thread pool as a new work task.
      mark_stack_pos_ /= 2;
      auto* task = new MarkStackTask(thread_pool_, mark_sweep_, kMaxSize - mark_stack_pos_,
                                     mark_stack_ + mark_stack_pos_);
      thread_pool_->AddTask(Thread::Current(), task);
    }
    DCHECK(obj != nullptr);
    DCHECK(mark_stack_pos_ < kMaxSize);
    mark_stack_[mark_stack_pos_++] = obj;
  }

  virtual void Finalize() {
    delete this;
  }

  // Scans all of the objects
  virtual void Run(Thread* self) {
    ScanObjectParallelVisitor visitor(this);
    // TODO: Tune this.
    static const size_t kFifoSize = 4;
    BoundedFifoPowerOfTwo<const Object*, kFifoSize> prefetch_fifo;
    for (;;) {
      const Object* obj = NULL;
      if (kUseMarkStackPrefetch) {
        while (mark_stack_pos_ != 0 && prefetch_fifo.size() < kFifoSize) {
          const Object* obj = mark_stack_[--mark_stack_pos_];
          DCHECK(obj != NULL);
          __builtin_prefetch(obj);
          prefetch_fifo.push_back(obj);
        }
        if (UNLIKELY(prefetch_fifo.empty())) {
          break;
        }
        obj = prefetch_fifo.front();
        prefetch_fifo.pop_front();
      } else {
        if (UNLIKELY(mark_stack_pos_ == 0)) {
          break;
        }
        obj = mark_stack_[--mark_stack_pos_];
      }
      DCHECK(obj != NULL);
      visitor(obj);
    }
  }
};

class CardScanTask : public MarkStackTask<false> {
 public:
  CardScanTask(ThreadPool* thread_pool, MarkSweep* mark_sweep, accounting::SpaceBitmap* bitmap,
               byte* begin, byte* end, byte minimum_age, size_t mark_stack_size,
               const Object** mark_stack_obj)
      : MarkStackTask<false>(thread_pool, mark_sweep, mark_stack_size, mark_stack_obj),
        bitmap_(bitmap),
        begin_(begin),
        end_(end),
        minimum_age_(minimum_age) {
  }

 protected:
  accounting::SpaceBitmap* const bitmap_;
  byte* const begin_;
  byte* const end_;
  const byte minimum_age_;

  virtual void Finalize() {
    delete this;
  }

  virtual void Run(Thread* self) NO_THREAD_SAFETY_ANALYSIS {
    ScanObjectParallelVisitor visitor(this);
    accounting::CardTable* card_table = mark_sweep_->GetHeap()->GetCardTable();
    size_t cards_scanned = card_table->Scan(bitmap_, begin_, end_, visitor, minimum_age_);
    mark_sweep_->cards_scanned_.fetch_add(cards_scanned);
    VLOG(heap) << "Parallel scanning cards " << reinterpret_cast<void*>(begin_) << " - "
        << reinterpret_cast<void*>(end_) << " = " << cards_scanned;
    // Finish by emptying our local mark stack.
    MarkStackTask::Run(self);
  }
};

size_t MarkSweep::GetThreadCount(bool paused) const {
  if (heap_->GetThreadPool() == nullptr || !heap_->CareAboutPauseTimes()) {
    return 0;
  }
  if (paused) {
    return heap_->GetParallelGCThreadCount() + 1;
  } else {
    return heap_->GetConcGCThreadCount() + 1;
  }
}

void MarkSweep::ScanGrayObjects(bool paused, byte minimum_age) {
  accounting::CardTable* card_table = GetHeap()->GetCardTable();
  ThreadPool* thread_pool = GetHeap()->GetThreadPool();
  size_t thread_count = GetThreadCount(paused);
  // The parallel version with only one thread is faster for card scanning, TODO: fix.
  if (kParallelCardScan && thread_count > 0) {
    Thread* self = Thread::Current();
    // Can't have a different split for each space since multiple spaces can have their cards being
    // scanned at the same time.
    timings_.StartSplit(paused ? "(Paused)ScanGrayObjects" : "ScanGrayObjects");
    // Try to take some of the mark stack since we can pass this off to the worker tasks.
    const Object** mark_stack_begin = const_cast<const Object**>(mark_stack_->Begin());
    const Object** mark_stack_end = const_cast<const Object**>(mark_stack_->End());
    const size_t mark_stack_size = mark_stack_end - mark_stack_begin;
    // Estimated number of work tasks we will create.
    const size_t mark_stack_tasks = GetHeap()->GetContinuousSpaces().size() * thread_count;
    DCHECK_NE(mark_stack_tasks, 0U);
    const size_t mark_stack_delta = std::min(CardScanTask::kMaxSize / 2,
                                             mark_stack_size / mark_stack_tasks + 1);
    size_t ref_card_count = 0;
    cards_scanned_ = 0;
    for (const auto& space : GetHeap()->GetContinuousSpaces()) {
      byte* card_begin = space->Begin();
      byte* card_end = space->End();
      // Calculate how many bytes of heap we will scan,
      const size_t address_range = card_end - card_begin;
      // Calculate how much address range each task gets.
      const size_t card_delta = RoundUp(address_range / thread_count + 1,
                                        accounting::CardTable::kCardSize);
      // Create the worker tasks for this space.
      while (card_begin != card_end) {
        // Add a range of cards.
        size_t addr_remaining = card_end - card_begin;
        size_t card_increment = std::min(card_delta, addr_remaining);
        // Take from the back of the mark stack.
        size_t mark_stack_remaining = mark_stack_end - mark_stack_begin;
        size_t mark_stack_increment = std::min(mark_stack_delta, mark_stack_remaining);
        mark_stack_end -= mark_stack_increment;
        mark_stack_->PopBackCount(static_cast<int32_t>(mark_stack_increment));
        DCHECK_EQ(mark_stack_end, mark_stack_->End());
        // Add the new task to the thread pool.
        auto* task = new CardScanTask(thread_pool, this, space->GetMarkBitmap(), card_begin,
                                      card_begin + card_increment, minimum_age,
                                      mark_stack_increment, mark_stack_end);
        thread_pool->AddTask(self, task);
        card_begin += card_increment;
      }

      if (paused && kIsDebugBuild) {
        // Make sure we don't miss scanning any cards.
        size_t scanned_cards = card_table->Scan(space->GetMarkBitmap(), space->Begin(),
                                                space->End(), VoidFunctor(), minimum_age);
        VLOG(heap) << "Scanning space cards " << reinterpret_cast<void*>(space->Begin()) << " - "
            << reinterpret_cast<void*>(space->End()) << " = " << scanned_cards;
        ref_card_count += scanned_cards;
      }
    }

    thread_pool->SetMaxActiveWorkers(thread_count - 1);
    thread_pool->StartWorkers(self);
    thread_pool->Wait(self, true, true);
    thread_pool->StopWorkers(self);
    if (paused) {
      DCHECK_EQ(ref_card_count, static_cast<size_t>(cards_scanned_.load()));
    }
    timings_.EndSplit();
  } else {
    for (const auto& space : GetHeap()->GetContinuousSpaces()) {
      // Image spaces are handled properly since live == marked for them.
      switch (space->GetGcRetentionPolicy()) {
        case space::kGcRetentionPolicyNeverCollect:
          timings_.StartSplit(paused ? "(Paused)ScanGrayImageSpaceObjects" :
              "ScanGrayImageSpaceObjects");
          break;
        case space::kGcRetentionPolicyFullCollect:
          timings_.StartSplit(paused ? "(Paused)ScanGrayZygoteSpaceObjects" :
              "ScanGrayZygoteSpaceObjects");
          break;
        case space::kGcRetentionPolicyAlwaysCollect:
          timings_.StartSplit(paused ? "(Paused)ScanGrayAllocSpaceObjects" :
              "ScanGrayAllocSpaceObjects");
          break;
        }
      ScanObjectVisitor visitor(this);
      card_table->Scan(space->GetMarkBitmap(), space->Begin(), space->End(), visitor, minimum_age);
      timings_.EndSplit();
    }
  }
}

void MarkSweep::VerifyImageRoots() {
  // Verify roots ensures that all the references inside the image space point
  // objects which are either in the image space or marked objects in the alloc
  // space
  timings_.StartSplit("VerifyImageRoots");
  for (const auto& space : GetHeap()->GetContinuousSpaces()) {
    if (space->IsImageSpace()) {
      space::ImageSpace* image_space = space->AsImageSpace();
      uintptr_t begin = reinterpret_cast<uintptr_t>(image_space->Begin());
      uintptr_t end = reinterpret_cast<uintptr_t>(image_space->End());
      accounting::SpaceBitmap* live_bitmap = image_space->GetLiveBitmap();
      DCHECK(live_bitmap != NULL);
      live_bitmap->VisitMarkedRange(begin, end, [this](const Object* obj) {
        if (kCheckLocks) {
          Locks::heap_bitmap_lock_->AssertSharedHeld(Thread::Current());
        }
        DCHECK(obj != NULL);
        CheckObject(obj);
      });
    }
  }
  timings_.EndSplit();
}

class RecursiveMarkTask : public MarkStackTask<false> {
 public:
  RecursiveMarkTask(ThreadPool* thread_pool, MarkSweep* mark_sweep,
                    accounting::SpaceBitmap* bitmap, uintptr_t begin, uintptr_t end)
      : MarkStackTask<false>(thread_pool, mark_sweep, 0, NULL),
        bitmap_(bitmap),
        begin_(begin),
        end_(end) {
  }

 protected:
  accounting::SpaceBitmap* const bitmap_;
  const uintptr_t begin_;
  const uintptr_t end_;

  virtual void Finalize() {
    delete this;
  }

  // Scans all of the objects
  virtual void Run(Thread* self) NO_THREAD_SAFETY_ANALYSIS {
    ScanObjectParallelVisitor visitor(this);
    bitmap_->VisitMarkedRange(begin_, end_, visitor);
    // Finish by emptying our local mark stack.
    MarkStackTask::Run(self);
  }
};

// Populates the mark stack based on the set of marked objects and
// recursively marks until the mark stack is emptied.
void MarkSweep::RecursiveMark() {
  base::TimingLogger::ScopedSplit split("RecursiveMark", &timings_);
  // RecursiveMark will build the lists of known instances of the Reference classes.
  // See DelayReferenceReferent for details.
  CHECK(soft_reference_list_ == NULL);
  CHECK(weak_reference_list_ == NULL);
  CHECK(finalizer_reference_list_ == NULL);
  CHECK(phantom_reference_list_ == NULL);
  CHECK(cleared_reference_list_ == NULL);

  if (kUseRecursiveMark) {
    const bool partial = GetGcType() == kGcTypePartial;
    ScanObjectVisitor scan_visitor(this);
    auto* self = Thread::Current();
    ThreadPool* thread_pool = heap_->GetThreadPool();
    size_t thread_count = GetThreadCount(false);
    const bool parallel = kParallelRecursiveMark && thread_count > 1;
    mark_stack_->Reset();
    for (const auto& space : GetHeap()->GetContinuousSpaces()) {
      if ((space->GetGcRetentionPolicy() == space::kGcRetentionPolicyAlwaysCollect) ||
          (!partial && space->GetGcRetentionPolicy() == space::kGcRetentionPolicyFullCollect)) {
        current_mark_bitmap_ = space->GetMarkBitmap();
        if (current_mark_bitmap_ == NULL) {
          GetHeap()->DumpSpaces();
          LOG(FATAL) << "invalid bitmap";
        }
        if (parallel) {
          // We will use the mark stack the future.
          // CHECK(mark_stack_->IsEmpty());
          // This function does not handle heap end increasing, so we must use the space end.
          uintptr_t begin = reinterpret_cast<uintptr_t>(space->Begin());
          uintptr_t end = reinterpret_cast<uintptr_t>(space->End());
          atomic_finger_ = static_cast<int32_t>(0xFFFFFFFF);

          // Create a few worker tasks.
          const size_t n = thread_count * 2;
          while (begin != end) {
            uintptr_t start = begin;
            uintptr_t delta = (end - begin) / n;
            delta = RoundUp(delta, KB);
            if (delta < 16 * KB) delta = end - begin;
            begin += delta;
            auto* task = new RecursiveMarkTask(thread_pool, this, current_mark_bitmap_, start,
                                               begin);
            thread_pool->AddTask(self, task);
          }
          thread_pool->SetMaxActiveWorkers(thread_count - 1);
          thread_pool->StartWorkers(self);
          thread_pool->Wait(self, true, true);
          thread_pool->StopWorkers(self);
        } else {
          // This function does not handle heap end increasing, so we must use the space end.
          uintptr_t begin = reinterpret_cast<uintptr_t>(space->Begin());
          uintptr_t end = reinterpret_cast<uintptr_t>(space->End());
          current_mark_bitmap_->VisitMarkedRange(begin, end, scan_visitor);
        }
      }
    }
  }
  ProcessMarkStack(false);
}

bool MarkSweep::IsMarkedCallback(const Object* object, void* arg) {
  return reinterpret_cast<MarkSweep*>(arg)->IsMarked(object);
}

void MarkSweep::RecursiveMarkDirtyObjects(bool paused, byte minimum_age) {
  ScanGrayObjects(paused, minimum_age);
  ProcessMarkStack(paused);
}

void MarkSweep::ReMarkRoots() {
  timings_.StartSplit("ReMarkRoots");
  Runtime::Current()->VisitRoots(ReMarkObjectVisitor, this, true, true);
  timings_.EndSplit();
}

void MarkSweep::SweepJniWeakGlobals(IsMarkedTester is_marked, void* arg) {
  Runtime::Current()->GetJavaVM()->SweepWeakGlobals(is_marked, arg);
}

struct ArrayMarkedCheck {
  accounting::ObjectStack* live_stack;
  MarkSweep* mark_sweep;
};

// Either marked or not live.
bool MarkSweep::IsMarkedArrayCallback(const Object* object, void* arg) {
  ArrayMarkedCheck* array_check = reinterpret_cast<ArrayMarkedCheck*>(arg);
  if (array_check->mark_sweep->IsMarked(object)) {
    return true;
  }
  accounting::ObjectStack* live_stack = array_check->live_stack;
  if (std::find(live_stack->Begin(), live_stack->End(), object) == live_stack->End()) {
    return true;
  }
  return false;
}

void MarkSweep::SweepSystemWeaks() {
  Runtime* runtime = Runtime::Current();
  timings_.StartSplit("SweepSystemWeaks");
  runtime->GetInternTable()->SweepInternTableWeaks(IsMarkedCallback, this);
  runtime->GetMonitorList()->SweepMonitorList(IsMarkedCallback, this);
  SweepJniWeakGlobals(IsMarkedCallback, this);
  timings_.EndSplit();
}

bool MarkSweep::VerifyIsLiveCallback(const Object* obj, void* arg) {
  reinterpret_cast<MarkSweep*>(arg)->VerifyIsLive(obj);
  // We don't actually want to sweep the object, so lets return "marked"
  return true;
}

void MarkSweep::VerifyIsLive(const Object* obj) {
  Heap* heap = GetHeap();
  if (!heap->GetLiveBitmap()->Test(obj)) {
    space::LargeObjectSpace* large_object_space = GetHeap()->GetLargeObjectsSpace();
    if (!large_object_space->GetLiveObjects()->Test(obj)) {
      if (std::find(heap->allocation_stack_->Begin(), heap->allocation_stack_->End(), obj) ==
          heap->allocation_stack_->End()) {
        // Object not found!
        heap->DumpSpaces();
        LOG(FATAL) << "Found dead object " << obj;
      }
    }
  }
}

void MarkSweep::VerifySystemWeaks() {
  Runtime* runtime = Runtime::Current();
  // Verify system weaks, uses a special IsMarked callback which always returns true.
  runtime->GetInternTable()->SweepInternTableWeaks(VerifyIsLiveCallback, this);
  runtime->GetMonitorList()->SweepMonitorList(VerifyIsLiveCallback, this);
  runtime->GetJavaVM()->SweepWeakGlobals(VerifyIsLiveCallback, this);
}

struct SweepCallbackContext {
  MarkSweep* mark_sweep;
  space::AllocSpace* space;
  Thread* self;
};

class CheckpointMarkThreadRoots : public Closure {
 public:
  explicit CheckpointMarkThreadRoots(MarkSweep* mark_sweep) : mark_sweep_(mark_sweep) {}

  virtual void Run(Thread* thread) NO_THREAD_SAFETY_ANALYSIS {
    ATRACE_BEGIN("Marking thread roots");
    // Note: self is not necessarily equal to thread since thread may be suspended.
    Thread* self = Thread::Current();
    CHECK(thread == self || thread->IsSuspended() || thread->GetState() == kWaitingPerformingGc)
        << thread->GetState() << " thread " << thread << " self " << self;
    thread->VisitRoots(MarkSweep::MarkRootParallelCallback, mark_sweep_);
    ATRACE_END();
    mark_sweep_->GetBarrier().Pass(self);
  }

 private:
  MarkSweep* mark_sweep_;
};

void MarkSweep::MarkRootsCheckpoint(Thread* self) {
  CheckpointMarkThreadRoots check_point(this);
  timings_.StartSplit("MarkRootsCheckpoint");
  ThreadList* thread_list = Runtime::Current()->GetThreadList();
  // Request the check point is run on all threads returning a count of the threads that must
  // run through the barrier including self.
  size_t barrier_count = thread_list->RunCheckpoint(&check_point);
  // Release locks then wait for all mutator threads to pass the barrier.
  // TODO: optimize to not release locks when there are no threads to wait for.
  Locks::heap_bitmap_lock_->ExclusiveUnlock(self);
  Locks::mutator_lock_->SharedUnlock(self);
  ThreadState old_state = self->SetState(kWaitingForCheckPointsToRun);
  CHECK_EQ(old_state, kWaitingPerformingGc);
  gc_barrier_->Increment(self, barrier_count);
  self->SetState(kWaitingPerformingGc);
  Locks::mutator_lock_->SharedLock(self);
  Locks::heap_bitmap_lock_->ExclusiveLock(self);
  timings_.EndSplit();
}

void MarkSweep::SweepCallback(size_t num_ptrs, Object** ptrs, void* arg) {
  SweepCallbackContext* context = static_cast<SweepCallbackContext*>(arg);
  MarkSweep* mark_sweep = context->mark_sweep;
  Heap* heap = mark_sweep->GetHeap();
  space::AllocSpace* space = context->space;
  Thread* self = context->self;
  Locks::heap_bitmap_lock_->AssertExclusiveHeld(self);
  // Use a bulk free, that merges consecutive objects before freeing or free per object?
  // Documentation suggests better free performance with merging, but this may be at the expensive
  // of allocation.
  size_t freed_objects = num_ptrs;
  // AllocSpace::FreeList clears the value in ptrs, so perform after clearing the live bit
  size_t freed_bytes = space->FreeList(self, num_ptrs, ptrs);
  heap->RecordFree(freed_objects, freed_bytes);
  mark_sweep->freed_objects_.fetch_add(freed_objects);
  mark_sweep->freed_bytes_.fetch_add(freed_bytes);
}

void MarkSweep::ZygoteSweepCallback(size_t num_ptrs, Object** ptrs, void* arg) {
  SweepCallbackContext* context = static_cast<SweepCallbackContext*>(arg);
  Locks::heap_bitmap_lock_->AssertExclusiveHeld(context->self);
  Heap* heap = context->mark_sweep->GetHeap();
  // We don't free any actual memory to avoid dirtying the shared zygote pages.
  for (size_t i = 0; i < num_ptrs; ++i) {
    Object* obj = static_cast<Object*>(ptrs[i]);
    heap->GetLiveBitmap()->Clear(obj);
    heap->GetCardTable()->MarkCard(obj);
  }
}

void MarkSweep::SweepArray(accounting::ObjectStack* allocations, bool swap_bitmaps) {
  space::DlMallocSpace* space = heap_->GetAllocSpace();
  timings_.StartSplit("SweepArray");
  // Newly allocated objects MUST be in the alloc space and those are the only objects which we are
  // going to free.
  accounting::SpaceBitmap* live_bitmap = space->GetLiveBitmap();
  accounting::SpaceBitmap* mark_bitmap = space->GetMarkBitmap();
  space::LargeObjectSpace* large_object_space = GetHeap()->GetLargeObjectsSpace();
  accounting::SpaceSetMap* large_live_objects = large_object_space->GetLiveObjects();
  accounting::SpaceSetMap* large_mark_objects = large_object_space->GetMarkObjects();
  if (swap_bitmaps) {
    std::swap(live_bitmap, mark_bitmap);
    std::swap(large_live_objects, large_mark_objects);
  }

  size_t freed_bytes = 0;
  size_t freed_large_object_bytes = 0;
  size_t freed_objects = 0;
  size_t freed_large_objects = 0;
  size_t count = allocations->Size();
  Object** objects = const_cast<Object**>(allocations->Begin());
  Object** out = objects;
  Object** objects_to_chunk_free = out;

  // Empty the allocation stack.
  Thread* self = Thread::Current();
  for (size_t i = 0; i < count; ++i) {
    Object* obj = objects[i];
    // There should only be objects in the AllocSpace/LargeObjectSpace in the allocation stack.
    if (LIKELY(mark_bitmap->HasAddress(obj))) {
      if (!mark_bitmap->Test(obj)) {
        // Don't bother un-marking since we clear the mark bitmap anyways.
        *(out++) = obj;
        // Free objects in chunks.
        DCHECK_GE(out, objects_to_chunk_free);
        DCHECK_LE(static_cast<size_t>(out - objects_to_chunk_free), kSweepArrayChunkFreeSize);
        if (static_cast<size_t>(out - objects_to_chunk_free) == kSweepArrayChunkFreeSize) {
          timings_.StartSplit("FreeList");
          size_t chunk_freed_objects = out - objects_to_chunk_free;
          freed_objects += chunk_freed_objects;
          freed_bytes += space->FreeList(self, chunk_freed_objects, objects_to_chunk_free);
          objects_to_chunk_free = out;
          timings_.EndSplit();
        }
      }
    } else if (!large_mark_objects->Test(obj)) {
      ++freed_large_objects;
      freed_large_object_bytes += large_object_space->Free(self, obj);
    }
  }
  // Free the remaining objects in chunks.
  DCHECK_GE(out, objects_to_chunk_free);
  DCHECK_LE(static_cast<size_t>(out - objects_to_chunk_free), kSweepArrayChunkFreeSize);
  if (out - objects_to_chunk_free > 0) {
    timings_.StartSplit("FreeList");
    size_t chunk_freed_objects = out - objects_to_chunk_free;
    freed_objects += chunk_freed_objects;
    freed_bytes += space->FreeList(self, chunk_freed_objects, objects_to_chunk_free);
    timings_.EndSplit();
  }
  CHECK_EQ(count, allocations->Size());
  timings_.EndSplit();

  timings_.StartSplit("RecordFree");
  VLOG(heap) << "Freed " << freed_objects << "/" << count
             << " objects with size " << PrettySize(freed_bytes);
  heap_->RecordFree(freed_objects + freed_large_objects, freed_bytes + freed_large_object_bytes);
  freed_objects_.fetch_add(freed_objects);
  freed_large_objects_.fetch_add(freed_large_objects);
  freed_bytes_.fetch_add(freed_bytes);
  freed_large_object_bytes_.fetch_add(freed_large_object_bytes);
  timings_.EndSplit();

  timings_.StartSplit("ResetStack");
  allocations->Reset();
  timings_.EndSplit();
}

void MarkSweep::Sweep(bool swap_bitmaps) {
  DCHECK(mark_stack_->IsEmpty());
  base::TimingLogger::ScopedSplit("Sweep", &timings_);

  const bool partial = (GetGcType() == kGcTypePartial);
  SweepCallbackContext scc;
  scc.mark_sweep = this;
  scc.self = Thread::Current();
  for (const auto& space : GetHeap()->GetContinuousSpaces()) {
    // We always sweep always collect spaces.
    bool sweep_space = (space->GetGcRetentionPolicy() == space::kGcRetentionPolicyAlwaysCollect);
    if (!partial && !sweep_space) {
      // We sweep full collect spaces when the GC isn't a partial GC (ie its full).
      sweep_space = (space->GetGcRetentionPolicy() == space::kGcRetentionPolicyFullCollect);
    }
    if (sweep_space) {
      uintptr_t begin = reinterpret_cast<uintptr_t>(space->Begin());
      uintptr_t end = reinterpret_cast<uintptr_t>(space->End());
      scc.space = space->AsDlMallocSpace();
      accounting::SpaceBitmap* live_bitmap = space->GetLiveBitmap();
      accounting::SpaceBitmap* mark_bitmap = space->GetMarkBitmap();
      if (swap_bitmaps) {
        std::swap(live_bitmap, mark_bitmap);
      }
      if (!space->IsZygoteSpace()) {
        base::TimingLogger::ScopedSplit split("SweepAllocSpace", &timings_);
        // Bitmaps are pre-swapped for optimization which enables sweeping with the heap unlocked.
        accounting::SpaceBitmap::SweepWalk(*live_bitmap, *mark_bitmap, begin, end,
                                           &SweepCallback, reinterpret_cast<void*>(&scc));
      } else {
        base::TimingLogger::ScopedSplit split("SweepZygote", &timings_);
        // Zygote sweep takes care of dirtying cards and clearing live bits, does not free actual
        // memory.
        accounting::SpaceBitmap::SweepWalk(*live_bitmap, *mark_bitmap, begin, end,
                                           &ZygoteSweepCallback, reinterpret_cast<void*>(&scc));
      }
    }
  }

  SweepLargeObjects(swap_bitmaps);
}

void MarkSweep::SweepLargeObjects(bool swap_bitmaps) {
  base::TimingLogger::ScopedSplit("SweepLargeObjects", &timings_);
  // Sweep large objects
  space::LargeObjectSpace* large_object_space = GetHeap()->GetLargeObjectsSpace();
  accounting::SpaceSetMap* large_live_objects = large_object_space->GetLiveObjects();
  accounting::SpaceSetMap* large_mark_objects = large_object_space->GetMarkObjects();
  if (swap_bitmaps) {
    std::swap(large_live_objects, large_mark_objects);
  }
  // O(n*log(n)) but hopefully there are not too many large objects.
  size_t freed_objects = 0;
  size_t freed_bytes = 0;
  Thread* self = Thread::Current();
  for (const Object* obj : large_live_objects->GetObjects()) {
    if (!large_mark_objects->Test(obj)) {
      freed_bytes += large_object_space->Free(self, const_cast<Object*>(obj));
      ++freed_objects;
    }
  }
  freed_large_objects_.fetch_add(freed_objects);
  freed_large_object_bytes_.fetch_add(freed_bytes);
  GetHeap()->RecordFree(freed_objects, freed_bytes);
}

void MarkSweep::CheckReference(const Object* obj, const Object* ref, MemberOffset offset, bool is_static) {
  for (const auto& space : GetHeap()->GetContinuousSpaces()) {
    if (space->IsDlMallocSpace() && space->Contains(ref)) {
      DCHECK(IsMarked(obj));

      bool is_marked = IsMarked(ref);
      if (!is_marked) {
        LOG(INFO) << *space;
        LOG(WARNING) << (is_static ? "Static ref'" : "Instance ref'") << PrettyTypeOf(ref)
                     << "' (" << reinterpret_cast<const void*>(ref) << ") in '" << PrettyTypeOf(obj)
                     << "' (" << reinterpret_cast<const void*>(obj) << ") at offset "
                     << reinterpret_cast<void*>(offset.Int32Value()) << " wasn't marked";

        const Class* klass = is_static ? obj->AsClass() : obj->GetClass();
        DCHECK(klass != NULL);
        const ObjectArray<ArtField>* fields = is_static ? klass->GetSFields() : klass->GetIFields();
        DCHECK(fields != NULL);
        bool found = false;
        for (int32_t i = 0; i < fields->GetLength(); ++i) {
          const ArtField* cur = fields->Get(i);
          if (cur->GetOffset().Int32Value() == offset.Int32Value()) {
            LOG(WARNING) << "Field referencing the alloc space was " << PrettyField(cur);
            found = true;
            break;
          }
        }
        if (!found) {
          LOG(WARNING) << "Could not find field in object alloc space with offset " << offset.Int32Value();
        }

        bool obj_marked = heap_->GetCardTable()->IsDirty(obj);
        if (!obj_marked) {
          LOG(WARNING) << "Object '" << PrettyTypeOf(obj) << "' "
                       << "(" << reinterpret_cast<const void*>(obj) << ") contains references to "
                       << "the alloc space, but wasn't card marked";
        }
      }
    }
    break;
  }
}

// Process the "referent" field in a java.lang.ref.Reference.  If the
// referent has not yet been marked, put it on the appropriate list in
// the heap for later processing.
void MarkSweep::DelayReferenceReferent(mirror::Class* klass, Object* obj) {
  DCHECK(klass != nullptr);
  DCHECK(klass->IsReferenceClass());
  DCHECK(obj != NULL);
  Object* referent = heap_->GetReferenceReferent(obj);
  if (referent != NULL && !IsMarked(referent)) {
    if (kCountJavaLangRefs) {
      ++reference_count_;
    }
    Thread* self = Thread::Current();
    // TODO: Remove these locks, and use atomic stacks for storing references?
    // We need to check that the references haven't already been enqueued since we can end up
    // scanning the same reference multiple times due to dirty cards.
    if (klass->IsSoftReferenceClass()) {
      MutexLock mu(self, *heap_->GetSoftRefQueueLock());
      if (!heap_->IsEnqueued(obj)) {
        heap_->EnqueuePendingReference(obj, &soft_reference_list_);
      }
    } else if (klass->IsWeakReferenceClass()) {
      MutexLock mu(self, *heap_->GetWeakRefQueueLock());
      if (!heap_->IsEnqueued(obj)) {
        heap_->EnqueuePendingReference(obj, &weak_reference_list_);
      }
    } else if (klass->IsFinalizerReferenceClass()) {
      MutexLock mu(self, *heap_->GetFinalizerRefQueueLock());
      if (!heap_->IsEnqueued(obj)) {
        heap_->EnqueuePendingReference(obj, &finalizer_reference_list_);
      }
    } else if (klass->IsPhantomReferenceClass()) {
      MutexLock mu(self, *heap_->GetPhantomRefQueueLock());
      if (!heap_->IsEnqueued(obj)) {
        heap_->EnqueuePendingReference(obj, &phantom_reference_list_);
      }
    } else {
      LOG(FATAL) << "Invalid reference type " << PrettyClass(klass)
                 << " " << std::hex << klass->GetAccessFlags();
    }
  }
}

void MarkSweep::ScanRoot(const Object* obj) {
  ScanObject(obj);
}

class MarkObjectVisitor {
 public:
  explicit MarkObjectVisitor(MarkSweep* const mark_sweep) ALWAYS_INLINE : mark_sweep_(mark_sweep) {}

  // TODO: Fixme when anotatalysis works with visitors.
  void operator()(const Object* /* obj */, const Object* ref, const MemberOffset& /* offset */,
                  bool /* is_static */) const ALWAYS_INLINE
      NO_THREAD_SAFETY_ANALYSIS {
    if (kCheckLocks) {
      Locks::mutator_lock_->AssertSharedHeld(Thread::Current());
      Locks::heap_bitmap_lock_->AssertExclusiveHeld(Thread::Current());
    }
    mark_sweep_->MarkObject(ref);
  }

 private:
  MarkSweep* const mark_sweep_;
};

// Scans an object reference.  Determines the type of the reference
// and dispatches to a specialized scanning routine.
void MarkSweep::ScanObject(const Object* obj) {
  MarkObjectVisitor visitor(this);
  ScanObjectVisit(obj, visitor);
}

void MarkSweep::ProcessMarkStackParallel(size_t thread_count) {
  Thread* self = Thread::Current();
  ThreadPool* thread_pool = GetHeap()->GetThreadPool();
  const size_t chunk_size = std::min(mark_stack_->Size() / thread_count + 1,
                                     static_cast<size_t>(MarkStackTask<false>::kMaxSize));
  CHECK_GT(chunk_size, 0U);
  // Split the current mark stack up into work tasks.
  for (mirror::Object **it = mark_stack_->Begin(), **end = mark_stack_->End(); it < end; ) {
    const size_t delta = std::min(static_cast<size_t>(end - it), chunk_size);
    thread_pool->AddTask(self, new MarkStackTask<false>(thread_pool, this, delta,
                                                        const_cast<const mirror::Object**>(it)));
    it += delta;
  }
  thread_pool->SetMaxActiveWorkers(thread_count - 1);
  thread_pool->StartWorkers(self);
  thread_pool->Wait(self, true, true);
  thread_pool->StopWorkers(self);
  mark_stack_->Reset();
  CHECK_EQ(work_chunks_created_, work_chunks_deleted_) << " some of the work chunks were leaked";
}

// Scan anything that's on the mark stack.
void MarkSweep::ProcessMarkStack(bool paused) {
  timings_.StartSplit("ProcessMarkStack");
  size_t thread_count = GetThreadCount(paused);
  if (kParallelProcessMarkStack && thread_count > 1 &&
      mark_stack_->Size() >= kMinimumParallelMarkStackSize) {
    ProcessMarkStackParallel(thread_count);
  } else {
    // TODO: Tune this.
    static const size_t kFifoSize = 4;
    BoundedFifoPowerOfTwo<const Object*, kFifoSize> prefetch_fifo;
    for (;;) {
      const Object* obj = NULL;
      if (kUseMarkStackPrefetch) {
        while (!mark_stack_->IsEmpty() && prefetch_fifo.size() < kFifoSize) {
          const Object* obj = mark_stack_->PopBack();
          DCHECK(obj != NULL);
          __builtin_prefetch(obj);
          prefetch_fifo.push_back(obj);
        }
        if (prefetch_fifo.empty()) {
          break;
        }
        obj = prefetch_fifo.front();
        prefetch_fifo.pop_front();
      } else {
        if (mark_stack_->IsEmpty()) {
          break;
        }
        obj = mark_stack_->PopBack();
      }
      DCHECK(obj != NULL);
      ScanObject(obj);
    }
  }
  timings_.EndSplit();
}

// Walks the reference list marking any references subject to the
// reference clearing policy.  References with a black referent are
// removed from the list.  References with white referents biased
// toward saving are blackened and also removed from the list.
void MarkSweep::PreserveSomeSoftReferences(Object** list) {
  DCHECK(list != NULL);
  Object* clear = NULL;
  size_t counter = 0;

  DCHECK(mark_stack_->IsEmpty());

  timings_.StartSplit("PreserveSomeSoftReferences");
  while (*list != NULL) {
    Object* ref = heap_->DequeuePendingReference(list);
    Object* referent = heap_->GetReferenceReferent(ref);
    if (referent == NULL) {
      // Referent was cleared by the user during marking.
      continue;
    }
    bool is_marked = IsMarked(referent);
    if (!is_marked && ((++counter) & 1)) {
      // Referent is white and biased toward saving, mark it.
      MarkObject(referent);
      is_marked = true;
    }
    if (!is_marked) {
      // Referent is white, queue it for clearing.
      heap_->EnqueuePendingReference(ref, &clear);
    }
  }
  *list = clear;
  timings_.EndSplit();

  // Restart the mark with the newly black references added to the root set.
  ProcessMarkStack(true);
}

inline bool MarkSweep::IsMarked(const Object* object) const
    SHARED_LOCKS_REQUIRED(Locks::heap_bitmap_lock_) {
  if (IsImmune(object)) {
    return true;
  }
  DCHECK(current_mark_bitmap_ != NULL);
  if (current_mark_bitmap_->HasAddress(object)) {
    return current_mark_bitmap_->Test(object);
  }
  return heap_->GetMarkBitmap()->Test(object);
}

// Unlink the reference list clearing references objects with white
// referents.  Cleared references registered to a reference queue are
// scheduled for appending by the heap worker thread.
void MarkSweep::ClearWhiteReferences(Object** list) {
  DCHECK(list != NULL);
  while (*list != NULL) {
    Object* ref = heap_->DequeuePendingReference(list);
    Object* referent = heap_->GetReferenceReferent(ref);
    if (referent != NULL && !IsMarked(referent)) {
      // Referent is white, clear it.
      heap_->ClearReferenceReferent(ref);
      if (heap_->IsEnqueuable(ref)) {
        heap_->EnqueueReference(ref, &cleared_reference_list_);
      }
    }
  }
  DCHECK(*list == NULL);
}

// Enqueues finalizer references with white referents.  White
// referents are blackened, moved to the zombie field, and the
// referent field is cleared.
void MarkSweep::EnqueueFinalizerReferences(Object** list) {
  DCHECK(list != NULL);
  timings_.StartSplit("EnqueueFinalizerReferences");
  MemberOffset zombie_offset = heap_->GetFinalizerReferenceZombieOffset();
  bool has_enqueued = false;
  while (*list != NULL) {
    Object* ref = heap_->DequeuePendingReference(list);
    Object* referent = heap_->GetReferenceReferent(ref);
    if (referent != NULL && !IsMarked(referent)) {
      MarkObject(referent);
      // If the referent is non-null the reference must queuable.
      DCHECK(heap_->IsEnqueuable(ref));
      ref->SetFieldObject(zombie_offset, referent, false);
      heap_->ClearReferenceReferent(ref);
      heap_->EnqueueReference(ref, &cleared_reference_list_);
      has_enqueued = true;
    }
  }
  timings_.EndSplit();
  if (has_enqueued) {
    ProcessMarkStack(true);
  }
  DCHECK(*list == NULL);
}

// Process reference class instances and schedule finalizations.
void MarkSweep::ProcessReferences(Object** soft_references, bool clear_soft,
                                  Object** weak_references,
                                  Object** finalizer_references,
                                  Object** phantom_references) {
  CHECK(soft_references != NULL);
  CHECK(weak_references != NULL);
  CHECK(finalizer_references != NULL);
  CHECK(phantom_references != NULL);
  CHECK(mark_stack_->IsEmpty());

  // Unless we are in the zygote or required to clear soft references
  // with white references, preserve some white referents.
  if (!clear_soft && !Runtime::Current()->IsZygote()) {
    PreserveSomeSoftReferences(soft_references);
  }

  timings_.StartSplit("ProcessReferences");
  // Clear all remaining soft and weak references with white
  // referents.
  ClearWhiteReferences(soft_references);
  ClearWhiteReferences(weak_references);
  timings_.EndSplit();

  // Preserve all white objects with finalize methods and schedule
  // them for finalization.
  EnqueueFinalizerReferences(finalizer_references);

  timings_.StartSplit("ProcessReferences");
  // Clear all f-reachable soft and weak references with white
  // referents.
  ClearWhiteReferences(soft_references);
  ClearWhiteReferences(weak_references);

  // Clear all phantom references with white referents.
  ClearWhiteReferences(phantom_references);

  // At this point all reference lists should be empty.
  DCHECK(*soft_references == NULL);
  DCHECK(*weak_references == NULL);
  DCHECK(*finalizer_references == NULL);
  DCHECK(*phantom_references == NULL);
  timings_.EndSplit();
}

void MarkSweep::UnBindBitmaps() {
  base::TimingLogger::ScopedSplit split("UnBindBitmaps", &timings_);
  for (const auto& space : GetHeap()->GetContinuousSpaces()) {
    if (space->IsDlMallocSpace()) {
      space::DlMallocSpace* alloc_space = space->AsDlMallocSpace();
      if (alloc_space->temp_bitmap_.get() != NULL) {
        // At this point, the temp_bitmap holds our old mark bitmap.
        accounting::SpaceBitmap* new_bitmap = alloc_space->temp_bitmap_.release();
        GetHeap()->GetMarkBitmap()->ReplaceBitmap(alloc_space->mark_bitmap_.get(), new_bitmap);
        CHECK_EQ(alloc_space->mark_bitmap_.release(), alloc_space->live_bitmap_.get());
        alloc_space->mark_bitmap_.reset(new_bitmap);
        DCHECK(alloc_space->temp_bitmap_.get() == NULL);
      }
    }
  }
}

void MarkSweep::FinishPhase() {
  base::TimingLogger::ScopedSplit split("FinishPhase", &timings_);
  // Can't enqueue references if we hold the mutator lock.
  Object* cleared_references = GetClearedReferences();
  Heap* heap = GetHeap();
  timings_.NewSplit("EnqueueClearedReferences");
  heap->EnqueueClearedReferences(&cleared_references);

  timings_.NewSplit("PostGcVerification");
  heap->PostGcVerification(this);

  timings_.NewSplit("GrowForUtilization");
  heap->GrowForUtilization(GetGcType(), GetDurationNs());

  timings_.NewSplit("RequestHeapTrim");
  heap->RequestHeapTrim();

  // Update the cumulative statistics
  total_time_ns_ += GetDurationNs();
  total_paused_time_ns_ += std::accumulate(GetPauseTimes().begin(), GetPauseTimes().end(), 0,
                                           std::plus<uint64_t>());
  total_freed_objects_ += GetFreedObjects() + GetFreedLargeObjects();
  total_freed_bytes_ += GetFreedBytes() + GetFreedLargeObjectBytes();

  // Ensure that the mark stack is empty.
  CHECK(mark_stack_->IsEmpty());

  if (kCountScannedTypes) {
    VLOG(gc) << "MarkSweep scanned classes=" << class_count_ << " arrays=" << array_count_
             << " other=" << other_count_;
  }

  if (kCountTasks) {
    VLOG(gc) << "Total number of work chunks allocated: " << work_chunks_created_;
  }

  if (kMeasureOverhead) {
    VLOG(gc) << "Overhead time " << PrettyDuration(overhead_time_);
  }

  if (kProfileLargeObjects) {
    VLOG(gc) << "Large objects tested " << large_object_test_ << " marked " << large_object_mark_;
  }

  if (kCountClassesMarked) {
    VLOG(gc) << "Classes marked " << classes_marked_;
  }

  if (kCountJavaLangRefs) {
    VLOG(gc) << "References scanned " << reference_count_;
  }

  // Update the cumulative loggers.
  cumulative_timings_.Start();
  cumulative_timings_.AddLogger(timings_);
  cumulative_timings_.End();

  // Clear all of the spaces' mark bitmaps.
  for (const auto& space : GetHeap()->GetContinuousSpaces()) {
    if (space->GetGcRetentionPolicy() != space::kGcRetentionPolicyNeverCollect) {
      space->GetMarkBitmap()->Clear();
    }
  }
  mark_stack_->Reset();

  // Reset the marked large objects.
  space::LargeObjectSpace* large_objects = GetHeap()->GetLargeObjectsSpace();
  large_objects->GetMarkObjects()->Clear();
}

}  // namespace collector
}  // namespace gc
}  // namespace art
