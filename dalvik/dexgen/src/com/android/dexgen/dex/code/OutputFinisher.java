/*
 * Copyright (C) 2007 The Android Open Source Project
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

package com.android.dexgen.dex.code;

import com.android.dexgen.rop.code.LocalItem;
import com.android.dexgen.rop.code.RegisterSpec;
import com.android.dexgen.rop.code.RegisterSpecList;
import com.android.dexgen.rop.code.RegisterSpecSet;
import com.android.dexgen.rop.code.SourcePosition;
import com.android.dexgen.rop.cst.Constant;
import com.android.dexgen.rop.cst.CstMemberRef;
import com.android.dexgen.rop.cst.CstType;
import com.android.dexgen.rop.cst.CstUtf8;
import com.android.dexgen.rop.type.Type;

import java.util.ArrayList;
import java.util.HashSet;

/**
 * Processor for instruction lists, which takes a "first cut" of
 * instruction selection as a basis and produces a "final cut" in the
 * form of a {@link DalvInsnList} instance.
 */
public final class OutputFinisher {
    /**
     * {@code >= 0;} register count for the method, not including any extra
     * "reserved" registers needed to translate "difficult" instructions
     */
    private final int unreservedRegCount;

    /** {@code non-null;} the list of instructions, per se */
    private ArrayList<DalvInsn> insns;

    /** whether any instruction has position info */
    private boolean hasAnyPositionInfo;

    /** whether any instruction has local variable info */
    private boolean hasAnyLocalInfo;

    /**
     * {@code >= 0;} the count of reserved registers (low-numbered
     * registers used when expanding instructions that can't be
     * represented simply); becomes valid after a call to {@link
     * #massageInstructions}
     */
    private int reservedCount;

    /**
     * Constructs an instance. It initially contains no instructions.
     *
     * @param regCount {@code >= 0;} register count for the method
     * @param initialCapacity {@code >= 0;} initial capacity of the instructions
     * list
     */
    public OutputFinisher(int initialCapacity, int regCount) {
        this.unreservedRegCount = regCount;
        this.insns = new ArrayList<DalvInsn>(initialCapacity);
        this.reservedCount = -1;
        this.hasAnyPositionInfo = false;
        this.hasAnyLocalInfo = false;
    }

    /**
     * Returns whether any of the instructions added to this instance
     * come with position info.
     *
     * @return whether any of the instructions added to this instance
     * come with position info
     */
    public boolean hasAnyPositionInfo() {
        return hasAnyPositionInfo;
    }

    /**
     * Returns whether this instance has any local variable information.
     *
     * @return whether this instance has any local variable information
     */
    public boolean hasAnyLocalInfo() {
        return hasAnyLocalInfo;
    }

    /**
     * Helper for {@link #add} which scrutinizes a single
     * instruction for local variable information.
     *
     * @param insn {@code non-null;} instruction to scrutinize
     * @return {@code true} iff the instruction refers to any
     * named locals
     */
    private static boolean hasLocalInfo(DalvInsn insn) {
        if (insn instanceof LocalSnapshot) {
            RegisterSpecSet specs = ((LocalSnapshot) insn).getLocals();
            int size = specs.size();
            for (int i = 0; i < size; i++) {
                if (hasLocalInfo(specs.get(i))) {
                    return true;
                }
            }
        } else if (insn instanceof LocalStart) {
            RegisterSpec spec = ((LocalStart) insn).getLocal();
            if (hasLocalInfo(spec)) {
                return true;
            }
        }

        return false;
    }

    /**
     * Helper for {@link #hasAnyLocalInfo} which scrutinizes a single
     * register spec.
     *
     * @param spec {@code non-null;} spec to scrutinize
     * @return {@code true} iff the spec refers to any
     * named locals
     */
    private static boolean hasLocalInfo(RegisterSpec spec) {
        return (spec != null)
            && (spec.getLocalItem().getName() != null);
    }

    /**
     * Returns the set of all constants referred to by instructions added
     * to this instance.
     *
     * @return {@code non-null;} the set of constants
     */
    public HashSet<Constant> getAllConstants() {
        HashSet<Constant> result = new HashSet<Constant>(20);

        for (DalvInsn insn : insns) {
            addConstants(result, insn);
        }

        return result;
    }

    /**
     * Helper for {@link #getAllConstants} which adds all the info for
     * a single instruction.
     *
     * @param result {@code non-null;} result set to add to
     * @param insn {@code non-null;} instruction to scrutinize
     */
    private static void addConstants(HashSet<Constant> result,
            DalvInsn insn) {
        if (insn instanceof CstInsn) {
            Constant cst = ((CstInsn) insn).getConstant();
            result.add(cst);
        } else if (insn instanceof LocalSnapshot) {
            RegisterSpecSet specs = ((LocalSnapshot) insn).getLocals();
            int size = specs.size();
            for (int i = 0; i < size; i++) {
                addConstants(result, specs.get(i));
            }
        } else if (insn instanceof LocalStart) {
            RegisterSpec spec = ((LocalStart) insn).getLocal();
            addConstants(result, spec);
        }
    }

    /**
     * Helper for {@link #getAllConstants} which adds all the info for
     * a single {@code RegisterSpec}.
     *
     * @param result {@code non-null;} result set to add to
     * @param spec {@code null-ok;} register spec to add
     */
    private static void addConstants(HashSet<Constant> result,
            RegisterSpec spec) {
        if (spec == null) {
            return;
        }

        LocalItem local = spec.getLocalItem();
        CstUtf8 name = local.getName();
        CstUtf8 signature = local.getSignature();
        Type type = spec.getType();

        if (type != Type.KNOWN_NULL) {
            result.add(CstType.intern(type));
        }

        if (name != null) {
            result.add(name);
        }

        if (signature != null) {
            result.add(signature);
        }
    }

    /**
     * Adds an instruction to the output.
     *
     * @param insn {@code non-null;} the instruction to add
     */
    public void add(DalvInsn insn) {
        insns.add(insn);
        updateInfo(insn);
    }

    /**
     * Inserts an instruction in the output at the given offset.
     *
     * @param at {@code >= 0;} what index to insert at
     * @param insn {@code non-null;} the instruction to insert
     */
    public void insert(int at, DalvInsn insn) {
        insns.add(at, insn);
        updateInfo(insn);
    }

    /**
     * Helper for {@link #add} and {@link #insert},
     * which updates the position and local info flags.
     *
     * @param insn {@code non-null;} an instruction that was just introduced
     */
    private void updateInfo(DalvInsn insn) {
        if (! hasAnyPositionInfo) {
            SourcePosition pos = insn.getPosition();
            if (pos.getLine() >= 0) {
                hasAnyPositionInfo = true;
            }
        }

        if (! hasAnyLocalInfo) {
            if (hasLocalInfo(insn)) {
                hasAnyLocalInfo = true;
            }
        }
    }

    /**
     * Reverses a branch which is buried a given number of instructions
     * backward in the output. It is illegal to call this unless the
     * indicated instruction really is a reversible branch.
     *
     * @param which how many instructions back to find the branch;
     * {@code 0} is the most recently added instruction,
     * {@code 1} is the instruction before that, etc.
     * @param newTarget {@code non-null;} the new target for the reversed branch
     */
    public void reverseBranch(int which, CodeAddress newTarget) {
        int size = insns.size();
        int index = size - which - 1;
        TargetInsn targetInsn;

        try {
            targetInsn = (TargetInsn) insns.get(index);
        } catch (IndexOutOfBoundsException ex) {
            // Translate the exception.
            throw new IllegalArgumentException("too few instructions");
        } catch (ClassCastException ex) {
            // Translate the exception.
            throw new IllegalArgumentException("non-reversible instruction");
        }

        /*
         * No need to call this.set(), since the format and other info
         * are the same.
         */
        insns.set(index, targetInsn.withNewTargetAndReversed(newTarget));
    }

    /**
     * Assigns indices in all instructions that need them, using the
     * given callback to perform lookups. This should be called before
     * calling {@link #finishProcessingAndGetList}.
     *
     * @param callback {@code non-null;} callback object
     */
    public void assignIndices(DalvCode.AssignIndicesCallback callback) {
        for (DalvInsn insn : insns) {
            if (insn instanceof CstInsn) {
                assignIndices((CstInsn) insn, callback);
            }
        }
    }

    /**
     * Helper for {@link #assignIndices} which does assignment for one
     * instruction.
     *
     * @param insn {@code non-null;} the instruction
     * @param callback {@code non-null;} the callback
     */
    private static void assignIndices(CstInsn insn,
            DalvCode.AssignIndicesCallback callback) {
        Constant cst = insn.getConstant();
        int index = callback.getIndex(cst);

        if (index >= 0) {
            insn.setIndex(index);
        }

        if (cst instanceof CstMemberRef) {
            CstMemberRef member = (CstMemberRef) cst;
            CstType definer = member.getDefiningClass();
            index = callback.getIndex(definer);
            if (index >= 0) {
                insn.setClassIndex(index);
            }
        }
    }

    /**
     * Does final processing on this instance and gets the output as
     * a {@link DalvInsnList}. Final processing consists of:
     *
     * <ul>
     *   <li>optionally renumbering registers (to make room as needed for
     *   expanded instructions)</li>
     *   <li>picking a final opcode for each instruction</li>
     *   <li>rewriting instructions, because of register number,
     *   constant pool index, or branch target size issues</li>
     *   <li>assigning final addresses</li>
     * </ul>
     *
     * <p><b>Note:</b> This method may only be called once per instance
     * of this class.</p>
     *
     * @return {@code non-null;} the output list
     * @throws UnsupportedOperationException if this method has
     * already been called
     */
    public DalvInsnList finishProcessingAndGetList() {
        if (reservedCount >= 0) {
            throw new UnsupportedOperationException("already processed");
        }

        InsnFormat[] formats = makeFormatsArray();
        reserveRegisters(formats);
        massageInstructions(formats);
        assignAddressesAndFixBranches();

        return DalvInsnList.makeImmutable(insns,
                reservedCount + unreservedRegCount);
    }

    /**
     * Helper for {@link #finishProcessingAndGetList}, which extracts
     * the format out of each instruction into a separate array, to be
     * further manipulated as things progress.
     *
     * @return {@code non-null;} the array of formats
     */
    private InsnFormat[] makeFormatsArray() {
        int size = insns.size();
        InsnFormat[] result = new InsnFormat[size];

        for (int i = 0; i < size; i++) {
            result[i] = insns.get(i).getOpcode().getFormat();
        }

        return result;
    }

    /**
     * Helper for {@link #finishProcessingAndGetList}, which figures
     * out how many reserved registers are required and then reserving
     * them. It also updates the given {@code formats} array so
     * as to avoid extra work when constructing the massaged
     * instruction list.
     *
     * @param formats {@code non-null;} array of per-instruction format selections
     */
    private void reserveRegisters(InsnFormat[] formats) {
        int oldReservedCount = (reservedCount < 0) ? 0 : reservedCount;

        /*
         * Call calculateReservedCount() and then perform register
         * reservation, repeatedly until no new reservations happen.
         */
        for (;;) {
            int newReservedCount = calculateReservedCount(formats);
            if (oldReservedCount >= newReservedCount) {
                break;
            }

            int reservedDifference = newReservedCount - oldReservedCount;
            int size = insns.size();

            for (int i = 0; i < size; i++) {
                /*
                 * CodeAddress instance identity is used to link
                 * TargetInsns to their targets, so it is
                 * inappropriate to make replacements, and they don't
                 * have registers in any case. Hence, the instanceof
                 * test below.
                 */
                DalvInsn insn = insns.get(i);
                if (!(insn instanceof CodeAddress)) {
                    /*
                     * No need to call this.set() since the format and
                     * other info are the same.
                     */
                    insns.set(i, insn.withRegisterOffset(reservedDifference));
                }
            }

            oldReservedCount = newReservedCount;
        }

        reservedCount = oldReservedCount;
    }

    /**
     * Helper for {@link #reserveRegisters}, which does one
     * pass over the instructions, calculating the number of
     * registers that need to be reserved. It also updates the
     * {@code formats} list to help avoid extra work in future
     * register reservation passes.
     *
     * @param formats {@code non-null;} array of per-instruction format selections
     * @return {@code >= 0;} the count of reserved registers
     */
    private int calculateReservedCount(InsnFormat[] formats) {
        int size = insns.size();

        /*
         * Potential new value of reservedCount, which gets updated in the
         * following loop. It starts out with the existing reservedCount
         * and gets increased if it turns out that additional registers
         * need to be reserved.
         */
        int newReservedCount = reservedCount;

        for (int i = 0; i < size; i++) {
            DalvInsn insn = insns.get(i);
            InsnFormat originalFormat = formats[i];
            InsnFormat newFormat = findFormatForInsn(insn, originalFormat);

            if (originalFormat == newFormat) {
                continue;
            }

            if (newFormat == null) {
                /*
                 * The instruction will need to be expanded, so reserve
                 * registers for it.
                 */
                int reserve = insn.getMinimumRegisterRequirement();
                if (reserve > newReservedCount) {
                    newReservedCount = reserve;
                }
            }

            formats[i] = newFormat;
        }

        return newReservedCount;
    }

    /**
     * Attempts to fit the given instruction into a format, returning
     * either a format that the instruction fits into or {@code null}
     * to indicate that the instruction will need to be expanded. This
     * fitting process starts with the given format as a first "best
     * guess" and then pessimizes from there if necessary.
     *
     * @param insn {@code non-null;} the instruction in question
     * @param format {@code null-ok;} the current guess as to the best instruction
     * format to use; {@code null} means that no simple format fits
     * @return {@code null-ok;} a possibly-different format, which is either a
     * good fit or {@code null} to indicate that no simple format
     * fits
     */
    private InsnFormat findFormatForInsn(DalvInsn insn, InsnFormat format) {
        if (format == null) {
            // The instruction is already known not to fit any simple format.
            return format;
        }

        if (format.isCompatible(insn)) {
            // The instruction already fits in the current best-known format.
            return format;
        }

        Dop dop = insn.getOpcode();
        int family = dop.getFamily();

        for (;;) {
            format = format.nextUp();
            if ((format == null) ||
                    (format.isCompatible(insn) &&
                     (Dops.getOrNull(family, format) != null))) {
                break;
            }
        }

        return format;
    }

    /**
     * Helper for {@link #finishProcessingAndGetList}, which goes
     * through each instruction in the output, making sure its opcode
     * can accomodate its arguments. In cases where the opcode is
     * unable to do so, this replaces the instruction with a larger
     * instruction with identical semantics that <i>will</i> work.
     *
     * <p>This method may also reserve a number of low-numbered
     * registers, renumbering the instructions' original registers, in
     * order to have register space available in which to move
     * very-high registers when expanding instructions into
     * multi-instruction sequences. This expansion is done when no
     * simple instruction format can be found for a given instruction that
     * is able to accomodate that instruction's registers.</p>
     *
     * <p>This method ignores issues of branch target size, since
     * final addresses aren't known at the point that this method is
     * called.</p>
     *
     * @param formats {@code non-null;} array of per-instruction format selections
     */
    private void massageInstructions(InsnFormat[] formats) {
        if (reservedCount == 0) {
            /*
             * The easy common case: No registers were reserved, so we
             * merely need to replace any instructions whose format changed
             * during the reservation pass, but all instructions will stay
             * at their original indices, and the instruction list doesn't
             * grow.
             */
            int size = insns.size();

            for (int i = 0; i < size; i++) {
                DalvInsn insn = insns.get(i);
                Dop dop = insn.getOpcode();
                InsnFormat format = formats[i];

                if (format != dop.getFormat()) {
                    dop = Dops.getOrNull(dop.getFamily(), format);
                    insns.set(i, insn.withOpcode(dop));
                }
            }
        } else {
            /*
             * The difficult uncommon case: Some instructions have to be
             * expanded to deal with high registers.
             */
            insns = performExpansion(formats);
        }
    }

    /**
     * Helper for {@link #massageInstructions}, which constructs a
     * replacement list, where each {link DalvInsn} instance that
     * couldn't be represented simply (due to register representation
     * problems) is expanded into a series of instances that together
     * perform the proper function.
     *
     * @param formats {@code non-null;} array of per-instruction format selections
     * @return {@code non-null;} the replacement list
     */
    private ArrayList<DalvInsn> performExpansion(InsnFormat[] formats) {
        int size = insns.size();
        ArrayList<DalvInsn> result = new ArrayList<DalvInsn>(size * 2);

        for (int i = 0; i < size; i++) {
            DalvInsn insn = insns.get(i);
            Dop dop = insn.getOpcode();
            InsnFormat originalFormat = dop.getFormat();
            InsnFormat currentFormat = formats[i];
            DalvInsn prefix;
            DalvInsn suffix;

            if (currentFormat != null) {
                // No expansion is necessary.
                prefix = null;
                suffix = null;
            } else {
                // Expansion is required.
                prefix = insn.hrPrefix();
                suffix = insn.hrSuffix();

                /*
                 * Get the initial guess as to the hr version, but then
                 * let findFormatForInsn() pick a better format, if any.
                 */
                insn = insn.hrVersion();
                originalFormat = insn.getOpcode().getFormat();
                currentFormat = findFormatForInsn(insn, originalFormat);
            }

            if (prefix != null) {
                result.add(prefix);
            }

            if (currentFormat != originalFormat) {
                dop = Dops.getOrNull(dop.getFamily(), currentFormat);
                insn = insn.withOpcode(dop);
            }
            result.add(insn);

            if (suffix != null) {
                result.add(suffix);
            }
        }

        return result;
    }

    /**
     * Helper for {@link #finishProcessingAndGetList}, which assigns
     * addresses to each instruction, possibly rewriting branches to
     * fix ones that wouldn't otherwise be able to reach their
     * targets.
     */
    private void assignAddressesAndFixBranches() {
        for (;;) {
            assignAddresses();
            if (!fixBranches()) {
                break;
            }
        }
    }

    /**
     * Helper for {@link #assignAddressesAndFixBranches}, which
     * assigns an address to each instruction, in order.
     */
    private void assignAddresses() {
        int address = 0;
        int size = insns.size();

        for (int i = 0; i < size; i++) {
            DalvInsn insn = insns.get(i);
            insn.setAddress(address);
            address += insn.codeSize();
        }
    }

    /**
     * Helper for {@link #assignAddressesAndFixBranches}, which checks
     * the branch target size requirement of each branch instruction
     * to make sure it fits. For instructions that don't fit, this
     * rewrites them to use a {@code goto} of some sort. In the
     * case of a conditional branch that doesn't fit, the sense of the
     * test is reversed in order to branch around a {@code goto}
     * to the original target.
     *
     * @return whether any branches had to be fixed
     */
    private boolean fixBranches() {
        int size = insns.size();
        boolean anyFixed = false;

        for (int i = 0; i < size; i++) {
            DalvInsn insn = insns.get(i);
            if (!(insn instanceof TargetInsn)) {
                // This loop only needs to inspect TargetInsns.
                continue;
            }

            Dop dop = insn.getOpcode();
            InsnFormat format = dop.getFormat();
            TargetInsn target = (TargetInsn) insn;

            if (format.branchFits(target)) {
                continue;
            }

            if (dop.getFamily() == DalvOps.GOTO) {
                // It is a goto; widen it if possible.
                InsnFormat newFormat = findFormatForInsn(insn, format);
                if (newFormat == null) {
                    /*
                     * The branch is already maximally large. This should
                     * only be possible if a method somehow manages to have
                     * more than 2^31 code units.
                     */
                    throw new UnsupportedOperationException("method too long");
                }
                dop = Dops.getOrNull(dop.getFamily(), newFormat);
                insn = insn.withOpcode(dop);
                insns.set(i, insn);
            } else {
                /*
                 * It is a conditional: Reverse its sense, and arrange for
                 * it to branch around an absolute goto to the original
                 * branch target.
                 *
                 * Note: An invariant of the list being processed is
                 * that every TargetInsn is followed by a CodeAddress.
                 * Hence, it is always safe to get the next element
                 * after a TargetInsn and cast it to CodeAddress, as
                 * is happening a few lines down.
                 *
                 * Also note: Size gets incremented by one here, as we
                 * have -- in the net -- added one additional element
                 * to the list, so we increment i to match. The added
                 * and changed elements will be inspected by a repeat
                 * call to this method after this invocation returns.
                 */
                CodeAddress newTarget;
                try {
                    newTarget = (CodeAddress) insns.get(i + 1);
                } catch (IndexOutOfBoundsException ex) {
                    // The TargetInsn / CodeAddress invariant was violated.
                    throw new IllegalStateException(
                            "unpaired TargetInsn (dangling)");
                } catch (ClassCastException ex) {
                    // The TargetInsn / CodeAddress invariant was violated.
                    throw new IllegalStateException("unpaired TargetInsn");
                }
                TargetInsn gotoInsn =
                    new TargetInsn(Dops.GOTO, target.getPosition(),
                            RegisterSpecList.EMPTY, target.getTarget());
                insns.set(i, gotoInsn);
                insns.add(i, target.withNewTargetAndReversed(newTarget));
                size++;
                i++;
            }

            anyFixed = true;
        }

        return anyFixed;
    }
}
