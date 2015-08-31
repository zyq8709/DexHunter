/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ART_COMPILER_LLVM_RUNTIME_SUPPORT_LLVM_FUNC_LIST_H_
#define ART_COMPILER_LLVM_RUNTIME_SUPPORT_LLVM_FUNC_LIST_H_

#define RUNTIME_SUPPORT_FUNC_LIST(V) \
  V(LockObject, art_portable_lock_object_from_code) \
  V(UnlockObject, art_portable_unlock_object_from_code) \
  V(GetCurrentThread, art_portable_get_current_thread_from_code) \
  V(SetCurrentThread, art_portable_set_current_thread_from_code) \
  V(PushShadowFrame, art_portable_push_shadow_frame_from_code) \
  V(PopShadowFrame, art_portable_pop_shadow_frame_from_code) \
  V(TestSuspend, art_portable_test_suspend_from_code) \
  V(ThrowException, art_portable_throw_exception_from_code) \
  V(ThrowStackOverflowException, art_portable_throw_stack_overflow_from_code) \
  V(ThrowNullPointerException, art_portable_throw_null_pointer_exception_from_code) \
  V(ThrowDivZeroException, art_portable_throw_div_zero_from_code) \
  V(ThrowIndexOutOfBounds, art_portable_throw_array_bounds_from_code) \
  V(InitializeTypeAndVerifyAccess, art_portable_initialize_type_and_verify_access_from_code) \
  V(InitializeType, art_portable_initialize_type_from_code) \
  V(IsAssignable, art_portable_is_assignable_from_code) \
  V(CheckCast, art_portable_check_cast_from_code) \
  V(CheckPutArrayElement, art_portable_check_put_array_element_from_code) \
  V(AllocObject, art_portable_alloc_object_from_code) \
  V(AllocObjectWithAccessCheck, art_portable_alloc_object_from_code_with_access_check) \
  V(AllocArray, art_portable_alloc_array_from_code) \
  V(AllocArrayWithAccessCheck, art_portable_alloc_array_from_code_with_access_check) \
  V(CheckAndAllocArray, art_portable_check_and_alloc_array_from_code) \
  V(CheckAndAllocArrayWithAccessCheck, art_portable_check_and_alloc_array_from_code_with_access_check) \
  V(FindStaticMethodWithAccessCheck, art_portable_find_static_method_from_code_with_access_check) \
  V(FindDirectMethodWithAccessCheck, art_portable_find_direct_method_from_code_with_access_check) \
  V(FindVirtualMethodWithAccessCheck, art_portable_find_virtual_method_from_code_with_access_check) \
  V(FindSuperMethodWithAccessCheck, art_portable_find_super_method_from_code_with_access_check) \
  V(FindInterfaceMethodWithAccessCheck, art_portable_find_interface_method_from_code_with_access_check) \
  V(FindInterfaceMethod, art_portable_find_interface_method_from_code) \
  V(ResolveString, art_portable_resolve_string_from_code) \
  V(Set32Static, art_portable_set32_static_from_code) \
  V(Set64Static, art_portable_set64_static_from_code) \
  V(SetObjectStatic, art_portable_set_obj_static_from_code) \
  V(Get32Static, art_portable_get32_static_from_code) \
  V(Get64Static, art_portable_get64_static_from_code) \
  V(GetObjectStatic, art_portable_get_obj_static_from_code) \
  V(Set32Instance, art_portable_set32_instance_from_code) \
  V(Set64Instance, art_portable_set64_instance_from_code) \
  V(SetObjectInstance, art_portable_set_obj_instance_from_code) \
  V(Get32Instance, art_portable_get32_instance_from_code) \
  V(Get64Instance, art_portable_get64_instance_from_code) \
  V(GetObjectInstance, art_portable_get_obj_instance_from_code) \
  V(InitializeStaticStorage, art_portable_initialize_static_storage_from_code) \
  V(FillArrayData, art_portable_fill_array_data_from_code) \
  V(GetAndClearException, art_portable_get_and_clear_exception) \
  V(IsExceptionPending, art_portable_is_exception_pending_from_code) \
  V(FindCatchBlock, art_portable_find_catch_block_from_code) \
  V(MarkGCCard, art_portable_mark_gc_card_from_code) \
  V(ProxyInvokeHandler, art_portable_proxy_invoke_handler_from_code) \
  V(art_d2l, art_d2l) \
  V(art_d2i, art_d2i) \
  V(art_f2l, art_f2l) \
  V(art_f2i, art_f2i) \
  V(JniMethodStart,                        art_portable_jni_method_start) \
  V(JniMethodStartSynchronized,            art_portable_jni_method_start_synchronized) \
  V(JniMethodEnd,                          art_portable_jni_method_end) \
  V(JniMethodEndSynchronized,              art_portable_jni_method_end_synchronized) \
  V(JniMethodEndWithReference,             art_portable_jni_method_end_with_reference) \
  V(JniMethodEndWithReferenceSynchronized, art_portable_jni_method_end_with_reference_synchronized)

#endif  // ART_COMPILER_LLVM_RUNTIME_SUPPORT_LLVM_FUNC_LIST_H_
