;;
;; Copyright (C) 2012 The Android Open Source Project
;;
;; Licensed under the Apache License, Version 2.0 (the "License");
;; you may not use this file except in compliance with the License.
;; You may obtain a copy of the License at
;;
;;      http://www.apache.org/licenses/LICENSE-2.0
;;
;; Unless required by applicable law or agreed to in writing, software
;; distributed under the License is distributed on an "AS IS" BASIS,
;; WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
;; See the License for the specific language governing permissions and
;; limitations under the License.
;;


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Type
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

%JavaObject = type opaque

%ShadowFrame = type { i32                  ; Number of VRegs
                    , %ShadowFrame*        ; Previous frame
                    , %JavaObject*         ; Method object pointer
                    , i32                  ; Line number for stack backtrace
                    ; [0 x i32]            ; VRegs
                    }

declare void @__art_type_list(%JavaObject*, %ShadowFrame*)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Thread
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

declare %JavaObject* @art_portable_get_current_thread_from_code()
declare %JavaObject* @art_portable_set_current_thread_from_code(%JavaObject*)

declare void @art_portable_lock_object_from_code(%JavaObject*, %JavaObject*)
declare void @art_portable_unlock_object_from_code(%JavaObject*, %JavaObject*)

declare void @art_portable_test_suspend_from_code(%JavaObject*)

declare %ShadowFrame* @art_portable_push_shadow_frame_from_code(%JavaObject*, %ShadowFrame*, %JavaObject*, i32)
declare void @art_portable_pop_shadow_frame_from_code(%ShadowFrame*)



;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Exception
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

declare %JavaObject* @art_portable_get_and_clear_exception(%JavaObject*)
declare void @art_portable_throw_div_zero_from_code()
declare void @art_portable_throw_array_bounds_from_code(i32, i32)
declare void @art_portable_throw_no_such_method_from_code(i32)
declare void @art_portable_throw_null_pointer_exception_from_code(i32)
declare void @art_portable_throw_stack_overflow_from_code()
declare void @art_portable_throw_exception_from_code(%JavaObject*)

declare i32 @art_portable_find_catch_block_from_code(%JavaObject*, i32)



;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Object Space
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

declare %JavaObject* @art_portable_alloc_object_from_code(i32, %JavaObject*, %JavaObject*)
declare %JavaObject* @art_portable_alloc_object_from_code_with_access_check(i32, %JavaObject*, %JavaObject*)

declare %JavaObject* @art_portable_alloc_array_from_code(i32, %JavaObject*, i32, %JavaObject*)
declare %JavaObject* @art_portable_alloc_array_from_code_with_access_check(i32, %JavaObject*, i32, %JavaObject*)
declare %JavaObject* @art_portable_check_and_alloc_array_from_code(i32, %JavaObject*, i32, %JavaObject*)
declare %JavaObject* @art_portable_check_and_alloc_array_from_code_with_access_check(i32, %JavaObject*, i32, %JavaObject*)

declare void @art_portable_find_instance_field_from_code(i32, %JavaObject*)
declare void @art_portable_find_static_field_from_code(i32, %JavaObject*)

declare %JavaObject* @art_portable_find_static_method_from_code_with_access_check(i32, %JavaObject*, %JavaObject*, %JavaObject*)
declare %JavaObject* @art_portable_find_direct_method_from_code_with_access_check(i32, %JavaObject*, %JavaObject*, %JavaObject*)
declare %JavaObject* @art_portable_find_virtual_method_from_code_with_access_check(i32, %JavaObject*, %JavaObject*, %JavaObject*)
declare %JavaObject* @art_portable_find_super_method_from_code_with_access_check(i32, %JavaObject*, %JavaObject*, %JavaObject*)
declare %JavaObject* @art_portable_find_interface_method_from_code_with_access_check(i32, %JavaObject*, %JavaObject*, %JavaObject*)
declare %JavaObject* @art_portable_find_interface_method_from_code(i32, %JavaObject*, %JavaObject*, %JavaObject*)

declare %JavaObject* @art_portable_initialize_static_storage_from_code(i32, %JavaObject*, %JavaObject*)
declare %JavaObject* @art_portable_initialize_type_from_code(i32, %JavaObject*, %JavaObject*)
declare %JavaObject* @art_portable_initialize_type_and_verify_access_from_code(i32, %JavaObject*, %JavaObject*)

declare %JavaObject* @art_portable_resolve_string_from_code(%JavaObject*, i32)

declare i32 @art_portable_set32_static_from_code(i32, %JavaObject*, i32)
declare i32 @art_portable_set64_static_from_code(i32, %JavaObject*, i64)
declare i32 @art_portable_set_obj_static_from_code(i32, %JavaObject*, %JavaObject*)

declare i32 @art_portable_get32_static_from_code(i32, %JavaObject*)
declare i64 @art_portable_get64_static_from_code(i32, %JavaObject*)
declare %JavaObject* @art_portable_get_obj_static_from_code(i32, %JavaObject*)

declare i32 @art_portable_set32_instance_from_code(i32, %JavaObject*, %JavaObject*, i32)
declare i32 @art_portable_set64_instance_from_code(i32, %JavaObject*, %JavaObject*, i64)
declare i32 @art_portable_set_obj_instance_from_code(i32, %JavaObject*, %JavaObject*, %JavaObject*)

declare i32 @art_portable_get32_instance_from_code(i32, %JavaObject*, %JavaObject*)
declare i64 @art_portable_get64_instance_from_code(i32, %JavaObject*, %JavaObject*)
declare %JavaObject* @art_portable_get_obj_instance_from_code(i32, %JavaObject*, %JavaObject*)

declare %JavaObject* @art_portable_decode_jobject_in_thread(%JavaObject*, %JavaObject*)

declare void @art_portable_fill_array_data_from_code(%JavaObject*, i32, %JavaObject*, i32)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Type Checking, in the nature of casting
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

declare i32 @art_portable_is_assignable_from_code(%JavaObject*, %JavaObject*)
declare void @art_portable_check_cast_from_code(%JavaObject*, %JavaObject*)
declare void @art_portable_check_put_array_element_from_code(%JavaObject*, %JavaObject*)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Math
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

declare i64 @art_d2l(double)
declare i32 @art_d2i(double)
declare i64 @art_f2l(float)
declare i32 @art_f2i(float)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; JNI
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

declare i32 @art_portable_jni_method_start(%JavaObject*)
declare i32 @art_portable_jni_method_start_synchronized(%JavaObject*, %JavaObject*)

declare void @art_portable_jni_method_end(i32, %JavaObject*)
declare void @art_portable_jni_method_end_synchronized(i32, %JavaObject*, %JavaObject*)
declare %JavaObject* @art_portable_jni_method_end_with_reference(%JavaObject*, i32, %JavaObject*)
declare %JavaObject* @art_portable_jni_method_end_with_reference_synchronized(%JavaObject*, i32, %JavaObject*, %JavaObject*)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Temporary runtime support, will be removed in the future
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

declare i1 @art_portable_is_exception_pending_from_code()

declare void @art_portable_mark_gc_card_from_code(%JavaObject*, %JavaObject*)

declare void @art_portable_proxy_invoke_handler_from_code(%JavaObject*, ...)
