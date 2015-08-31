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

#include "jni_compiler.h"

#include "base/logging.h"
#include "class_linker.h"
#include "compiled_method.h"
#include "dex_file-inl.h"
#include "driver/compiler_driver.h"
#include "driver/dex_compilation_unit.h"
#include "llvm/compiler_llvm.h"
#include "llvm/ir_builder.h"
#include "llvm/llvm_compilation_unit.h"
#include "llvm/runtime_support_llvm_func.h"
#include "llvm/utils_llvm.h"
#include "mirror/art_method.h"
#include "runtime.h"
#include "stack.h"
#include "thread.h"

#include <llvm/ADT/SmallVector.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>

namespace art {
namespace llvm {

using ::art::llvm::runtime_support::JniMethodEnd;
using ::art::llvm::runtime_support::JniMethodEndSynchronized;
using ::art::llvm::runtime_support::JniMethodEndWithReference;
using ::art::llvm::runtime_support::JniMethodEndWithReferenceSynchronized;
using ::art::llvm::runtime_support::JniMethodStart;
using ::art::llvm::runtime_support::JniMethodStartSynchronized;
using ::art::llvm::runtime_support::RuntimeId;

JniCompiler::JniCompiler(LlvmCompilationUnit* cunit,
                         CompilerDriver& driver,
                         const DexCompilationUnit* dex_compilation_unit)
    : cunit_(cunit), driver_(&driver), module_(cunit_->GetModule()),
      context_(cunit_->GetLLVMContext()), irb_(*cunit_->GetIRBuilder()),
      dex_compilation_unit_(dex_compilation_unit),
      func_(NULL), elf_func_idx_(0) {
  // Check: Ensure that JNI compiler will only get "native" method
  CHECK(dex_compilation_unit->IsNative());
}

CompiledMethod* JniCompiler::Compile() {
  const bool is_static = dex_compilation_unit_->IsStatic();
  const bool is_synchronized = dex_compilation_unit_->IsSynchronized();
  const DexFile* dex_file = dex_compilation_unit_->GetDexFile();
  DexFile::MethodId const& method_id =
      dex_file->GetMethodId(dex_compilation_unit_->GetDexMethodIndex());
  char const return_shorty = dex_file->GetMethodShorty(method_id)[0];
  ::llvm::Value* this_object_or_class_object;

  uint32_t method_idx = dex_compilation_unit_->GetDexMethodIndex();
  std::string func_name(StringPrintf("jni_%s",
                                     MangleForJni(PrettyMethod(method_idx, *dex_file)).c_str()));
  CreateFunction(func_name);

  // Set argument name
  ::llvm::Function::arg_iterator arg_begin(func_->arg_begin());
  ::llvm::Function::arg_iterator arg_end(func_->arg_end());
  ::llvm::Function::arg_iterator arg_iter(arg_begin);

  DCHECK_NE(arg_iter, arg_end);
  arg_iter->setName("method");
  ::llvm::Value* method_object_addr = arg_iter++;

  if (!is_static) {
    // Non-static, the second argument is "this object"
    this_object_or_class_object = arg_iter++;
  } else {
    // Load class object
    this_object_or_class_object =
        irb_.LoadFromObjectOffset(method_object_addr,
                                  mirror::ArtMethod::DeclaringClassOffset().Int32Value(),
                                  irb_.getJObjectTy(),
                                  kTBAAConstJObject);
  }
  // Actual argument (ignore method and this object)
  arg_begin = arg_iter;

  // Count the number of Object* arguments
  uint32_t sirt_size = 1;
  // "this" object pointer for non-static
  // "class" object pointer for static
  for (unsigned i = 0; arg_iter != arg_end; ++i, ++arg_iter) {
#if !defined(NDEBUG)
    arg_iter->setName(StringPrintf("a%u", i));
#endif
    if (arg_iter->getType() == irb_.getJObjectTy()) {
      ++sirt_size;
    }
  }

  // Shadow stack
  ::llvm::StructType* shadow_frame_type = irb_.getShadowFrameTy(sirt_size);
  ::llvm::AllocaInst* shadow_frame_ = irb_.CreateAlloca(shadow_frame_type);

  // Store the dex pc
  irb_.StoreToObjectOffset(shadow_frame_,
                           ShadowFrame::DexPCOffset(),
                           irb_.getInt32(DexFile::kDexNoIndex),
                           kTBAAShadowFrame);

  // Push the shadow frame
  ::llvm::Value* shadow_frame_upcast = irb_.CreateConstGEP2_32(shadow_frame_, 0, 0);
  ::llvm::Value* old_shadow_frame =
      irb_.Runtime().EmitPushShadowFrame(shadow_frame_upcast, method_object_addr, sirt_size);

  // Get JNIEnv
  ::llvm::Value* jni_env_object_addr =
      irb_.Runtime().EmitLoadFromThreadOffset(Thread::JniEnvOffset().Int32Value(),
                                              irb_.getJObjectTy(),
                                              kTBAARuntimeInfo);

  // Get callee code_addr
  ::llvm::Value* code_addr =
      irb_.LoadFromObjectOffset(method_object_addr,
                                mirror::ArtMethod::NativeMethodOffset().Int32Value(),
                                GetFunctionType(dex_compilation_unit_->GetDexMethodIndex(),
                                                is_static, true)->getPointerTo(),
                                kTBAARuntimeInfo);

  // Load actual parameters
  std::vector< ::llvm::Value*> args;

  // The 1st parameter: JNIEnv*
  args.push_back(jni_env_object_addr);

  // Variables for GetElementPtr
  ::llvm::Value* gep_index[] = {
    irb_.getInt32(0),  // No displacement for shadow frame pointer
    irb_.getInt32(1),  // SIRT
    NULL,
  };

  size_t sirt_member_index = 0;

  // Store the "this object or class object" to SIRT
  gep_index[2] = irb_.getInt32(sirt_member_index++);
  ::llvm::Value* sirt_field_addr = irb_.CreateBitCast(irb_.CreateGEP(shadow_frame_, gep_index),
                                                    irb_.getJObjectTy()->getPointerTo());
  irb_.CreateStore(this_object_or_class_object, sirt_field_addr, kTBAAShadowFrame);
  // Push the "this object or class object" to out args
  this_object_or_class_object = irb_.CreateBitCast(sirt_field_addr, irb_.getJObjectTy());
  args.push_back(this_object_or_class_object);
  // Store arguments to SIRT, and push back to args
  for (arg_iter = arg_begin; arg_iter != arg_end; ++arg_iter) {
    if (arg_iter->getType() == irb_.getJObjectTy()) {
      // Store the reference type arguments to SIRT
      gep_index[2] = irb_.getInt32(sirt_member_index++);
      ::llvm::Value* sirt_field_addr = irb_.CreateBitCast(irb_.CreateGEP(shadow_frame_, gep_index),
                                                        irb_.getJObjectTy()->getPointerTo());
      irb_.CreateStore(arg_iter, sirt_field_addr, kTBAAShadowFrame);
      // Note null is placed in the SIRT but the jobject passed to the native code must be null
      // (not a pointer into the SIRT as with regular references).
      ::llvm::Value* equal_null = irb_.CreateICmpEQ(arg_iter, irb_.getJNull());
      ::llvm::Value* arg =
          irb_.CreateSelect(equal_null,
                            irb_.getJNull(),
                            irb_.CreateBitCast(sirt_field_addr, irb_.getJObjectTy()));
      args.push_back(arg);
    } else {
      args.push_back(arg_iter);
    }
  }

  ::llvm::Value* saved_local_ref_cookie;
  {  // JniMethodStart
    RuntimeId func_id = is_synchronized ? JniMethodStartSynchronized
                                        : JniMethodStart;
    ::llvm::SmallVector< ::llvm::Value*, 2> args;
    if (is_synchronized) {
      args.push_back(this_object_or_class_object);
    }
    args.push_back(irb_.Runtime().EmitGetCurrentThread());
    saved_local_ref_cookie =
        irb_.CreateCall(irb_.GetRuntime(func_id), args);
  }

  // Call!!!
  ::llvm::Value* retval = irb_.CreateCall(code_addr, args);

  {  // JniMethodEnd
    bool is_return_ref = return_shorty == 'L';
    RuntimeId func_id =
        is_return_ref ? (is_synchronized ? JniMethodEndWithReferenceSynchronized
                                         : JniMethodEndWithReference)
                      : (is_synchronized ? JniMethodEndSynchronized
                                         : JniMethodEnd);
    ::llvm::SmallVector< ::llvm::Value*, 4> args;
    if (is_return_ref) {
      args.push_back(retval);
    }
    args.push_back(saved_local_ref_cookie);
    if (is_synchronized) {
      args.push_back(this_object_or_class_object);
    }
    args.push_back(irb_.Runtime().EmitGetCurrentThread());

    ::llvm::Value* decoded_jobject =
        irb_.CreateCall(irb_.GetRuntime(func_id), args);

    // Return decoded jobject if return reference.
    if (is_return_ref) {
      retval = decoded_jobject;
    }
  }

  // Pop the shadow frame
  irb_.Runtime().EmitPopShadowFrame(old_shadow_frame);

  // Return!
  switch (return_shorty) {
    case 'V':
      irb_.CreateRetVoid();
      break;
    case 'Z':
    case 'C':
      irb_.CreateRet(irb_.CreateZExt(retval, irb_.getInt32Ty()));
      break;
    case 'B':
    case 'S':
      irb_.CreateRet(irb_.CreateSExt(retval, irb_.getInt32Ty()));
      break;
    default:
      irb_.CreateRet(retval);
      break;
  }

  // Verify the generated bitcode
  VERIFY_LLVM_FUNCTION(*func_);

  cunit_->Materialize();

  return new CompiledMethod(*driver_, cunit_->GetInstructionSet(), cunit_->GetElfObject(),
                            func_name);
}


void JniCompiler::CreateFunction(const std::string& func_name) {
  CHECK_NE(0U, func_name.size());

  const bool is_static = dex_compilation_unit_->IsStatic();

  // Get function type
  ::llvm::FunctionType* func_type =
    GetFunctionType(dex_compilation_unit_->GetDexMethodIndex(), is_static, false);

  // Create function
  func_ = ::llvm::Function::Create(func_type, ::llvm::Function::InternalLinkage,
                                   func_name, module_);

  // Create basic block
  ::llvm::BasicBlock* basic_block = ::llvm::BasicBlock::Create(*context_, "B0", func_);

  // Set insert point
  irb_.SetInsertPoint(basic_block);
}


::llvm::FunctionType* JniCompiler::GetFunctionType(uint32_t method_idx,
                                                   bool is_static, bool is_native_function) {
  // Get method signature
  uint32_t shorty_size;
  const char* shorty = dex_compilation_unit_->GetShorty(&shorty_size);
  CHECK_GE(shorty_size, 1u);

  // Get return type
  ::llvm::Type* ret_type = NULL;
  switch (shorty[0]) {
    case 'V': ret_type =  irb_.getJVoidTy(); break;
    case 'Z':
    case 'B':
    case 'C':
    case 'S':
    case 'I': ret_type =  irb_.getJIntTy(); break;
    case 'F': ret_type =  irb_.getJFloatTy(); break;
    case 'J': ret_type =  irb_.getJLongTy(); break;
    case 'D': ret_type =  irb_.getJDoubleTy(); break;
    case 'L': ret_type =  irb_.getJObjectTy(); break;
    default: LOG(FATAL)  << "Unreachable: unexpected return type in shorty " << shorty;
  }
  // Get argument type
  std::vector< ::llvm::Type*> args_type;

  args_type.push_back(irb_.getJObjectTy());  // method object pointer

  if (!is_static || is_native_function) {
    // "this" object pointer for non-static
    // "class" object pointer for static naitve
    args_type.push_back(irb_.getJType('L'));
  }

  for (uint32_t i = 1; i < shorty_size; ++i) {
    args_type.push_back(irb_.getJType(shorty[i]));
  }

  return ::llvm::FunctionType::get(ret_type, args_type, false);
}

}  // namespace llvm
}  // namespace art
