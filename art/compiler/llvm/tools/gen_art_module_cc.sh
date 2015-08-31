#!/bin/bash -e

# Copyright (C) 2012 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

SCRIPTDIR=`dirname "$0"`
cd "${SCRIPTDIR}/.."

mkdir -p generated

OUTPUT_FILE=generated/art_module.cc

echo "// Generated with ${0}" > ${OUTPUT_FILE}

echo '

#pragma GCC diagnostic ignored "-Wframe-larger-than="
// TODO: Remove this pragma after llc can generate makeLLVMModuleContents()
// with smaller frame size.

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>

#include <vector>

using namespace llvm;

namespace art {
namespace llvm {

' >> ${OUTPUT_FILE}

llc -march=cpp -cppgen=contents art_module.ll -o - >> ${OUTPUT_FILE}

echo '
} // namespace llvm
} // namespace art' >> ${OUTPUT_FILE}
