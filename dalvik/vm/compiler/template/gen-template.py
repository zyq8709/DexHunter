#!/usr/bin/env python
#
# Copyright (C) 2007 The Android Open Source Project
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

#
# Using instructions from an architecture-specific config file, generate C
# and assembly source files for the Dalvik JIT.
#

import sys, string, re, time
from string import Template

interp_defs_file = "TemplateOpList.h" # need opcode list

handler_size_bits = -1000
handler_size_bytes = -1000
in_op_start = 0             # 0=not started, 1=started, 2=ended
default_op_dir = None
opcode_locations = {}
asm_stub_text = []
label_prefix = ".L"         # use ".L" to hide labels from gdb


# Exception class.
class DataParseError(SyntaxError):
    "Failure when parsing data file"

#
# Set any omnipresent substitution values.
#
def getGlobalSubDict():
    return { "handler_size_bits":handler_size_bits,
             "handler_size_bytes":handler_size_bytes }

#
# Parse arch config file --
# Set handler_size_bytes to the value of tokens[1], and handler_size_bits to
# log2(handler_size_bytes).  Throws an exception if "bytes" is not a power
# of two.
#
def setHandlerSize(tokens):
    global handler_size_bits, handler_size_bytes
    if len(tokens) != 2:
        raise DataParseError("handler-size requires one argument")
    if handler_size_bits != -1000:
        raise DataParseError("handler-size may only be set once")

    # compute log2(n), and make sure n is a power of 2
    handler_size_bytes = bytes = int(tokens[1])
    bits = -1
    while bytes > 0:
        bytes //= 2     # halve with truncating division
        bits += 1

    if handler_size_bytes == 0 or handler_size_bytes != (1 << bits):
        raise DataParseError("handler-size (%d) must be power of 2 and > 0" \
                % orig_bytes)
    handler_size_bits = bits

#
# Parse arch config file --
# Copy a file in to the C or asm output file.
#
def importFile(tokens):
    if len(tokens) != 2:
        raise DataParseError("import requires one argument")
    source = tokens[1]
    if source.endswith(".S"):
        appendSourceFile(tokens[1], getGlobalSubDict(), asm_fp, None)
    else:
        raise DataParseError("don't know how to import %s (expecting .c/.S)"
                % source)

#
# Parse arch config file --
# Copy a file in to the C or asm output file.
#
def setAsmStub(tokens):
    global asm_stub_text
    if len(tokens) != 2:
        raise DataParseError("import requires one argument")
    try:
        stub_fp = open(tokens[1])
        asm_stub_text = stub_fp.readlines()
    except IOError, err:
        stub_fp.close()
        raise DataParseError("unable to load asm-stub: %s" % str(err))
    stub_fp.close()

#
# Parse arch config file --
# Start of opcode list.
#
def opStart(tokens):
    global in_op_start
    global default_op_dir
    if len(tokens) != 2:
        raise DataParseError("opStart takes a directory name argument")
    if in_op_start != 0:
        raise DataParseError("opStart can only be specified once")
    default_op_dir = tokens[1]
    in_op_start = 1

#
# Parse arch config file --
# Set location of a single opcode's source file.
#
def opEntry(tokens):
    #global opcode_locations
    if len(tokens) != 3:
        raise DataParseError("op requires exactly two arguments")
    if in_op_start != 1:
        raise DataParseError("op statements must be between opStart/opEnd")
    try:
        index = opcodes.index(tokens[1])
    except ValueError:
        raise DataParseError("unknown opcode %s" % tokens[1])
    opcode_locations[tokens[1]] = tokens[2]

#
# Parse arch config file --
# End of opcode list; emit instruction blocks.
#
def opEnd(tokens):
    global in_op_start
    if len(tokens) != 1:
        raise DataParseError("opEnd takes no arguments")
    if in_op_start != 1:
        raise DataParseError("opEnd must follow opStart, and only appear once")
    in_op_start = 2

    loadAndEmitOpcodes()


#
# Extract an ordered list of instructions from the VM sources.  We use the
# "goto table" definition macro, which has exactly kNumPackedOpcodes
# entries.
#
def getOpcodeList():
    opcodes = []
    opcode_fp = open("%s/%s" % (target_arch, interp_defs_file))
    opcode_re = re.compile(r"^JIT_TEMPLATE\((\w+)\)", re.DOTALL)
    for line in opcode_fp:
        match = opcode_re.match(line)
        if not match:
            continue
        opcodes.append("TEMPLATE_" + match.group(1))
    opcode_fp.close()

    return opcodes


#
# Load and emit opcodes for all kNumPackedOpcodes instructions.
#
def loadAndEmitOpcodes():
    sister_list = []

    # point dvmAsmInstructionStart at the first handler or stub
    asm_fp.write("\n    .global dvmCompilerTemplateStart\n")
    asm_fp.write("    .type   dvmCompilerTemplateStart, %function\n")
    asm_fp.write("    .section .data.rel.ro\n\n")
    asm_fp.write("dvmCompilerTemplateStart:\n\n")

    for i in xrange(len(opcodes)):
        op = opcodes[i]

        if opcode_locations.has_key(op):
            location = opcode_locations[op]
        else:
            location = default_op_dir

        loadAndEmitAsm(location, i, sister_list)

    # Use variable sized handlers now
    # asm_fp.write("\n    .balign %d\n" % handler_size_bytes)
    asm_fp.write("    .size   dvmCompilerTemplateStart, .-dvmCompilerTemplateStart\n")

#
# Load an assembly fragment and emit it.
#
def loadAndEmitAsm(location, opindex, sister_list):
    op = opcodes[opindex]
    source = "%s/%s.S" % (location, op)
    dict = getGlobalSubDict()
    dict.update({ "opcode":op, "opnum":opindex })
    print " emit %s --> asm" % source

    emitAsmHeader(asm_fp, dict)
    appendSourceFile(source, dict, asm_fp, sister_list)

#
# Output the alignment directive and label for an assembly piece.
#
def emitAsmHeader(outfp, dict):
    outfp.write("/* ------------------------------ */\n")
    # The alignment directive ensures that the handler occupies
    # at least the correct amount of space.  We don't try to deal
    # with overflow here.
    outfp.write("    .balign 4\n")
    # Emit a label so that gdb will say the right thing.  We prepend an
    # underscore so the symbol name doesn't clash with the Opcode enum.
    template_name = "dvmCompiler_%(opcode)s" % dict
    outfp.write("    .global %s\n" % template_name);
    outfp.write("%s:\n" % template_name);

#
# Output a generic instruction stub that updates the "glue" struct and
# calls the C implementation.
#
def emitAsmStub(outfp, dict):
    emitAsmHeader(outfp, dict)
    for line in asm_stub_text:
        templ = Template(line)
        outfp.write(templ.substitute(dict))

#
# Append the file specified by "source" to the open "outfp".  Each line will
# be template-replaced using the substitution dictionary "dict".
#
# If the first line of the file starts with "%" it is taken as a directive.
# A "%include" line contains a filename and, optionally, a Python-style
# dictionary declaration with substitution strings.  (This is implemented
# with recursion.)
#
# If "sister_list" is provided, and we find a line that contains only "&",
# all subsequent lines from the file will be appended to sister_list instead
# of copied to the output.
#
# This may modify "dict".
#
def appendSourceFile(source, dict, outfp, sister_list):
    outfp.write("/* File: %s */\n" % source)
    infp = open(source, "r")
    in_sister = False
    for line in infp:
        if line.startswith("%include"):
            # Parse the "include" line
            tokens = line.strip().split(' ', 2)
            if len(tokens) < 2:
                raise DataParseError("malformed %%include in %s" % source)

            alt_source = tokens[1].strip("\"")
            if alt_source == source:
                raise DataParseError("self-referential %%include in %s"
                        % source)

            new_dict = dict.copy()
            if len(tokens) == 3:
                new_dict.update(eval(tokens[2]))
            #print " including src=%s dict=%s" % (alt_source, new_dict)
            appendSourceFile(alt_source, new_dict, outfp, sister_list)
            continue

        elif line.startswith("%default"):
            # copy keywords into dictionary
            tokens = line.strip().split(' ', 1)
            if len(tokens) < 2:
                raise DataParseError("malformed %%default in %s" % source)
            defaultValues = eval(tokens[1])
            for entry in defaultValues:
                dict.setdefault(entry, defaultValues[entry])
            continue

        elif line.startswith("%verify"):
            # more to come, someday
            continue

        elif line.startswith("%break") and sister_list != None:
            # allow more than one %break, ignoring all following the first
            if not in_sister:
                in_sister = True
                sister_list.append("\n/* continuation for %(opcode)s */\n"%dict)
            continue

        # perform keyword substitution if a dictionary was provided
        if dict != None:
            templ = Template(line)
            try:
                subline = templ.substitute(dict)
            except KeyError, err:
                raise DataParseError("keyword substitution failed in %s: %s"
                        % (source, str(err)))
            except:
                print "ERROR: substitution failed: " + line
                raise
        else:
            subline = line

        # write output to appropriate file
        if in_sister:
            sister_list.append(subline)
        else:
            outfp.write(subline)
    outfp.write("\n")
    infp.close()

#
# Emit a C-style section header comment.
#
def emitSectionComment(str, fp):
    equals = "========================================" \
             "==================================="

    fp.write("\n/*\n * %s\n *  %s\n * %s\n */\n" %
        (equals, str, equals))


#
# ===========================================================================
# "main" code
#

#
# Check args.
#
if len(sys.argv) != 3:
    print "Usage: %s target-arch output-dir" % sys.argv[0]
    sys.exit(2)

target_arch = sys.argv[1]
output_dir = sys.argv[2]

#
# Extract opcode list.
#
opcodes = getOpcodeList()
#for op in opcodes:
#    print "  %s" % op

#
# Open config file.
#
try:
    config_fp = open("config-%s" % target_arch)
except:
    print "Unable to open config file 'config-%s'" % target_arch
    sys.exit(1)

#
# Open and prepare output files.
#
try:
    asm_fp = open("%s/CompilerTemplateAsm-%s.S" % (output_dir, target_arch), "w")
except:
    print "Unable to open output files"
    print "Make sure directory '%s' exists and existing files are writable" \
            % output_dir
    # Ideally we'd remove the files to avoid confusing "make", but if they
    # failed to open we probably won't be able to remove them either.
    sys.exit(1)

print "Generating %s" % (asm_fp.name)

file_header = """/*
 * This file was generated automatically by gen-template.py for '%s'.
 *
 * --> DO NOT EDIT <--
 */

""" % (target_arch)

asm_fp.write(file_header)

#
# Process the config file.
#
failed = False
try:
    for line in config_fp:
        line = line.strip()         # remove CRLF, leading spaces
        tokens = line.split(' ')    # tokenize
        #print "%d: %s" % (len(tokens), tokens)
        if len(tokens[0]) == 0:
            #print "  blank"
            pass
        elif tokens[0][0] == '#':
            #print "  comment"
            pass
        else:
            if tokens[0] == "handler-size":
                setHandlerSize(tokens)
            elif tokens[0] == "import":
                importFile(tokens)
            elif tokens[0] == "asm-stub":
                setAsmStub(tokens)
            elif tokens[0] == "op-start":
                opStart(tokens)
            elif tokens[0] == "op-end":
                opEnd(tokens)
            elif tokens[0] == "op":
                opEntry(tokens)
            else:
                raise DataParseError, "unrecognized command '%s'" % tokens[0]
except DataParseError, err:
    print "Failed: " + str(err)
    # TODO: remove output files so "make" doesn't get confused
    failed = True
    asm_fp.close()
    c_fp = asm_fp = None

config_fp.close()

#
# Done!
#
if asm_fp:
    asm_fp.close()

sys.exit(failed)
