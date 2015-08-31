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
# and assembly source files for the Dalvik interpreter.
#

import sys, string, re, time
from string import Template

interp_defs_file = "../../libdex/DexOpcodes.h" # need opcode list
kNumPackedOpcodes = 256 # TODO: Derive this from DexOpcodes.h.

splitops = False
verbose = False
handler_size_bits = -1000
handler_size_bytes = -1000
in_op_start = 0             # 0=not started, 1=started, 2=ended
in_alt_op_start = 0         # 0=not started, 1=started, 2=ended
default_op_dir = None
default_alt_stub = None
opcode_locations = {}
alt_opcode_locations = {}
asm_stub_text = []
label_prefix = ".L"         # use ".L" to hide labels from gdb
alt_label_prefix = ".L_ALT" # use ".L" to hide labels from gdb
style = None                # interpreter style
generate_alt_table = False

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
# Set interpreter style.
#
def setHandlerStyle(tokens):
    global style
    if len(tokens) != 2:
        raise DataParseError("handler-style requires one argument")
    style = tokens[1]
    if style != "computed-goto" and style != "jump-table" and style != "all-c":
        raise DataParseError("handler-style (%s) invalid" % style)

#
# Parse arch config file --
# Set handler_size_bytes to the value of tokens[1], and handler_size_bits to
# log2(handler_size_bytes).  Throws an exception if "bytes" is not 0 or
# a power of two.
#
def setHandlerSize(tokens):
    global handler_size_bits, handler_size_bytes
    if style != "computed-goto":
        print "Warning: handler-size valid only for computed-goto interpreters"
    if len(tokens) != 2:
        raise DataParseError("handler-size requires one argument")
    if handler_size_bits != -1000:
        raise DataParseError("handler-size may only be set once")

    # compute log2(n), and make sure n is 0 or a power of 2
    handler_size_bytes = bytes = int(tokens[1])
    bits = -1
    while bytes > 0:
        bytes //= 2     # halve with truncating division
        bits += 1

    if handler_size_bytes == 0 or handler_size_bytes != (1 << bits):
        raise DataParseError("handler-size (%d) must be power of 2" \
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
    if source.endswith(".cpp"):
        appendSourceFile(tokens[1], getGlobalSubDict(), c_fp, None)
    elif source.endswith(".S"):
        appendSourceFile(tokens[1], getGlobalSubDict(), asm_fp, None)
    else:
        raise DataParseError("don't know how to import %s (expecting .cpp/.S)"
                % source)

#
# Parse arch config file --
# Copy a file in to the C or asm output file.
#
def setAsmStub(tokens):
    global asm_stub_text
    if style == "all-c":
        print "Warning: asm-stub ignored for all-c interpreter"
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
# Record location of default alt stub
#
def setAsmAltStub(tokens):
    global default_alt_stub, generate_alt_table
    if style == "all-c":
        print "Warning: asm-alt-stub ingored for all-c interpreter"
    if len(tokens) != 2:
        raise DataParseError("import requires one argument")
    default_alt_stub = tokens[1]
    generate_alt_table = True

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
# Set location of a single alt opcode's source file.
#
def altEntry(tokens):
    global generate_alt_table
    if len(tokens) != 3:
        raise DataParseError("alt requires exactly two arguments")
    if in_op_start != 1:
        raise DataParseError("alt statements must be between opStart/opEnd")
    try:
        index = opcodes.index(tokens[1])
    except ValueError:
        raise DataParseError("unknown opcode %s" % tokens[1])
    if alt_opcode_locations.has_key(tokens[1]):
        print "Note: alt overrides earlier %s (%s -> %s)" \
                % (tokens[1], alt_opcode_locations[tokens[1]], tokens[2])
    alt_opcode_locations[tokens[1]] = tokens[2]
    generate_alt_table = True

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
    if opcode_locations.has_key(tokens[1]):
        print "Note: op overrides earlier %s (%s -> %s)" \
                % (tokens[1], opcode_locations[tokens[1]], tokens[2])
    opcode_locations[tokens[1]] = tokens[2]

#
# Emit jump table
#
def emitJmpTable(start_label, prefix):
    asm_fp.write("\n    .global %s\n" % start_label)
    asm_fp.write("    .text\n")
    asm_fp.write("%s:\n" % start_label)
    for i in xrange(kNumPackedOpcodes):
        op = opcodes[i]
        dict = getGlobalSubDict()
        dict.update({ "opcode":op, "opnum":i })
        asm_fp.write("    .long " + prefix + \
                     "_%(opcode)s /* 0x%(opnum)02x */\n" % dict)

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
    if splitops == False:
        if generate_alt_table:
            loadAndEmitAltOpcodes()
            if style == "jump-table":
                emitJmpTable("dvmAsmInstructionStart", label_prefix);
                emitJmpTable("dvmAsmAltInstructionStart", alt_label_prefix);

def genaltop(tokens):
    if in_op_start != 2:
       raise DataParseError("alt-op can be specified only after op-end")
    if len(tokens) != 1:
        raise DataParseError("opEnd takes no arguments")
    if generate_alt_table:
        loadAndEmitAltOpcodes()
        if style == "jump-table":
            emitJmpTable("dvmAsmInstructionStart", label_prefix);
            emitJmpTable("dvmAsmAltInstructionStart", alt_label_prefix);


#
# Extract an ordered list of instructions from the VM sources.  We use the
# "goto table" definition macro, which has exactly kNumPackedOpcodes
# entries.
#
def getOpcodeList():
    opcodes = []
    opcode_fp = open(interp_defs_file)
    opcode_re = re.compile(r"^\s*H\(OP_(\w+)\),.*", re.DOTALL)
    for line in opcode_fp:
        match = opcode_re.match(line)
        if not match:
            continue
        opcodes.append("OP_" + match.group(1))
    opcode_fp.close()

    if len(opcodes) != kNumPackedOpcodes:
        print "ERROR: found %d opcodes in Interp.h (expected %d)" \
                % (len(opcodes), kNumPackedOpcodes)
        raise SyntaxError, "bad opcode count"
    return opcodes

def emitAlign():
    if style == "computed-goto":
        asm_fp.write("    .balign %d\n" % handler_size_bytes)

#
# Load and emit opcodes for all kNumPackedOpcodes instructions.
#
def loadAndEmitOpcodes():
    sister_list = []
    assert len(opcodes) == kNumPackedOpcodes
    need_dummy_start = False
    if style == "jump-table":
        start_label = "dvmAsmInstructionStartCode"
        end_label = "dvmAsmInstructionEndCode"
    else:
        start_label = "dvmAsmInstructionStart"
        end_label = "dvmAsmInstructionEnd"

    # point dvmAsmInstructionStart at the first handler or stub
    asm_fp.write("\n    .global %s\n" % start_label)
    asm_fp.write("    .type   %s, %%function\n" % start_label)
    asm_fp.write("%s = " % start_label + label_prefix + "_OP_NOP\n")
    asm_fp.write("    .text\n\n")

    for i in xrange(kNumPackedOpcodes):
        op = opcodes[i]

        if opcode_locations.has_key(op):
            location = opcode_locations[op]
        else:
            location = default_op_dir

        if location == "c":
            loadAndEmitC(location, i)
            if len(asm_stub_text) == 0:
                need_dummy_start = True
        else:
            loadAndEmitAsm(location, i, sister_list)

    # For a 100% C implementation, there are no asm handlers or stubs.  We
    # need to have the dvmAsmInstructionStart label point at OP_NOP, and it's
    # too annoying to try to slide it in after the alignment psuedo-op, so
    # we take the low road and just emit a dummy OP_NOP here.
    if need_dummy_start:
        emitAlign()
        asm_fp.write(label_prefix + "_OP_NOP:   /* dummy */\n");

    emitAlign()
    asm_fp.write("    .size   %s, .-%s\n" % (start_label, start_label))
    asm_fp.write("    .global %s\n" % end_label)
    asm_fp.write("%s:\n" % end_label)

    if style == "computed-goto":
        emitSectionComment("Sister implementations", asm_fp)
        asm_fp.write("    .global dvmAsmSisterStart\n")
        asm_fp.write("    .type   dvmAsmSisterStart, %function\n")
        asm_fp.write("    .text\n")
        asm_fp.write("    .balign 4\n")
        asm_fp.write("dvmAsmSisterStart:\n")
        asm_fp.writelines(sister_list)
        asm_fp.write("\n    .size   dvmAsmSisterStart, .-dvmAsmSisterStart\n")
        asm_fp.write("    .global dvmAsmSisterEnd\n")
        asm_fp.write("dvmAsmSisterEnd:\n\n")

#
# Load an alternate entry stub
#
def loadAndEmitAltStub(source, opindex):
    op = opcodes[opindex]
    if verbose:
        print " alt emit %s --> stub" % source
    dict = getGlobalSubDict()
    dict.update({ "opcode":op, "opnum":opindex })

    emitAsmHeader(asm_fp, dict, alt_label_prefix)
    appendSourceFile(source, dict, asm_fp, None)

#
# Load and emit alternate opcodes for all kNumPackedOpcodes instructions.
#
def loadAndEmitAltOpcodes():
    assert len(opcodes) == kNumPackedOpcodes
    if style == "jump-table":
        start_label = "dvmAsmAltInstructionStartCode"
        end_label = "dvmAsmAltInstructionEndCode"
    else:
        start_label = "dvmAsmAltInstructionStart"
        end_label = "dvmAsmAltInstructionEnd"

    # point dvmAsmInstructionStart at the first handler or stub
    asm_fp.write("\n    .global %s\n" % start_label)
    asm_fp.write("    .type   %s, %%function\n" % start_label)
    asm_fp.write("    .text\n\n")
    asm_fp.write("%s = " % start_label + label_prefix + "_ALT_OP_NOP\n")

    for i in xrange(kNumPackedOpcodes):
        op = opcodes[i]
        if alt_opcode_locations.has_key(op):
            source = "%s/ALT_%s.S" % (alt_opcode_locations[op], op)
        else:
            source = default_alt_stub
        loadAndEmitAltStub(source, i)

    emitAlign()
    asm_fp.write("    .size   %s, .-%s\n" % (start_label, start_label))
    asm_fp.write("    .global %s\n" % end_label)
    asm_fp.write("%s:\n" % end_label)

#
# Load a C fragment and emit it, then output an assembly stub.
#
def loadAndEmitC(location, opindex):
    op = opcodes[opindex]
    source = "%s/%s.cpp" % (location, op)
    if verbose:
        print " emit %s --> C++" % source
    dict = getGlobalSubDict()
    dict.update({ "opcode":op, "opnum":opindex })

    appendSourceFile(source, dict, c_fp, None)

    if len(asm_stub_text) != 0:
        emitAsmStub(asm_fp, dict)

#
# Load an assembly fragment and emit it.
#
def loadAndEmitAsm(location, opindex, sister_list):
    op = opcodes[opindex]
    source = "%s/%s.S" % (location, op)
    dict = getGlobalSubDict()
    dict.update({ "opcode":op, "opnum":opindex })
    if verbose:
        print " emit %s --> asm" % source

    emitAsmHeader(asm_fp, dict, label_prefix)
    appendSourceFile(source, dict, asm_fp, sister_list)

#
# Output the alignment directive and label for an assembly piece.
#
def emitAsmHeader(outfp, dict, prefix):
    outfp.write("/* ------------------------------ */\n")
    # The alignment directive ensures that the handler occupies
    # at least the correct amount of space.  We don't try to deal
    # with overflow here.
    emitAlign()
    # Emit a label so that gdb will say the right thing.  We prepend an
    # underscore so the symbol name doesn't clash with the Opcode enum.
    outfp.write(prefix + "_%(opcode)s: /* 0x%(opnum)02x */\n" % dict)

#
# Output a generic instruction stub that updates the "glue" struct and
# calls the C implementation.
#
def emitAsmStub(outfp, dict):
    emitAsmHeader(outfp, dict, label_prefix)
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
            if style == "computed-goto" and not in_sister:
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
    c_fp = open("%s/InterpC-%s.cpp" % (output_dir, target_arch), "w")
    asm_fp = open("%s/InterpAsm-%s.S" % (output_dir, target_arch), "w")
except:
    print "Unable to open output files"
    print "Make sure directory '%s' exists and existing files are writable" \
            % output_dir
    # Ideally we'd remove the files to avoid confusing "make", but if they
    # failed to open we probably won't be able to remove them either.
    sys.exit(1)

print "Generating %s, %s" % (c_fp.name, asm_fp.name)

file_header = """/*
 * This file was generated automatically by gen-mterp.py for '%s'.
 *
 * --> DO NOT EDIT <--
 */

""" % (target_arch)

c_fp.write(file_header)
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
            elif tokens[0] == "asm-alt-stub":
                setAsmAltStub(tokens)
            elif tokens[0] == "op-start":
                opStart(tokens)
            elif tokens[0] == "op-end":
                opEnd(tokens)
            elif tokens[0] == "alt":
                altEntry(tokens)
            elif tokens[0] == "op":
                opEntry(tokens)
            elif tokens[0] == "handler-style":
                setHandlerStyle(tokens)
            elif tokens[0] == "alt-ops":
                genaltop(tokens)
            elif tokens[0] == "split-ops":
                splitops = True
            else:
                raise DataParseError, "unrecognized command '%s'" % tokens[0]
            if style == None:
                print "tokens[0] = %s" % tokens[0]
                raise DataParseError, "handler-style must be first command"
except DataParseError, err:
    print "Failed: " + str(err)
    # TODO: remove output files so "make" doesn't get confused
    failed = True
    c_fp.close()
    asm_fp.close()
    c_fp = asm_fp = None

config_fp.close()

#
# Done!
#
if c_fp:
    c_fp.close()
if asm_fp:
    asm_fp.close()

sys.exit(failed)
