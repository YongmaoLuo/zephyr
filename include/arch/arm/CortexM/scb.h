/* scb.h - ARM CORTEX-M System Control Block interface */

/*
 * Copyright (c) 2013-2014 Wind River Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2) Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3) Neither the name of Wind River Systems nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
DESCRIPTION
Provide an interface to the System Control Block found on ARM Cortex-M
processors.

The API does not account for all possible usages of the SCB, only the
functionalities needed by the kernel. It does not contain NVIC
functionalities either: these can be found in nvic.h. MPU functionalities
are not implemented.

The same effect can be achieved by directly writing in the registers of the
SCB, with the layout available from scs.h, using the __scs.scb data
structure (or hardcoded values), but the APIs found here are less error-prone,
especially for registers with multiple instances to account for 16 exceptions.

If access to a missing functionality is needed, directly writing to the
registers is the way to implement it.
*/

#ifndef _SCB__H_
#define _SCB__H_

#ifdef _ASMLANGUAGE

/* needed by nano_cpu_atomic_idle() written in asm */
#define _SCB_SCR 0xE000ED10

#define _SCB_SCR_SEVONPEND (1 << 4)
#define _SCB_SCR_SLEEPDEEP (1 << 2)
#define _SCB_SCR_SLEEPONEXIT (1 << 1)

#else

#include <nanokernel.h>
#include <arch/cpu.h>
#include <misc/__assert.h>
#include <arch/arm/CortexM/scs.h>
#include <misc/util.h>
#include <stdint.h>

extern void _ScbSystemReset(void);
extern void _ScbNumPriGroupSet(unsigned int n);

/*******************************************************************************
*
* _ScbIsNmiPending - find out if the NMI exception is pending
*
* RETURNS: 1 if it is pending, 0 otherwise
*/

static inline int _ScbIsNmiPending(void)
{
	return !!__scs.scb.icsr.bit.nmipendset;
}

/*******************************************************************************
*
* _ScbNmiPend - pend the NMI exception
*
* Pend the NMI exception: it should fire immediately.
*
* RETURNS: N/A
*/

static inline void _ScbNmiPend(void)
{
	__scs.scb.icsr.bit.nmipendset = 1;
}

/*******************************************************************************
*
* _ScbIsPendsvPending - find out if the PendSV exception is pending
*
* RETURNS: 1 if it is pending, 0 otherwise
*/

static inline int _ScbIsPendsvPending(void)
{
	return __scs.scb.icsr.bit.pendsvset;
}

/*******************************************************************************
*
* _ScbPendsvSet - set the PendSV exception
*
* Set the PendSV exception: it will be handled when the last nested exception
* returns, or immediately if running in thread mode.
*
* RETURNS: N/A
*/

static inline void _ScbPendsvSet(void)
{
	__scs.scb.icsr.bit.pendsvset = 1;
}

/*******************************************************************************
*
* _ScbPendsvClear - clear the PendSV exception
*
* This routine clears the PendSV exception.
*
* RETURNS: N/A
*/

static inline void _ScbPendsvClear(void)
{
	__scs.scb.icsr.bit.pendsvclr = 1;
}

/*******************************************************************************
*
* _ScbIsSystickPending - find out if the SYSTICK exception is pending
*
* This routine determines if the SYSTICK exception is pending.
*
* RETURNS: 1 if it is pending, 0 otherwise
*/

static inline int _ScbIsSystickPending(void)
{
	return __scs.scb.icsr.bit.pendstset;
}

/*******************************************************************************
*
* _ScbSystickPend - pend the SYSTICK exception
*
* Pend the SYSTICK exception: it will be handled when returning from a higher
* priority exception or immediately if in thread mode or handling a lower
* priority exception.
*
* RETURNS: N/A
*/

static inline void _ScbSystickPendSet(void)
{
	__scs.scb.icsr.bit.pendstset = 1;
}

/*******************************************************************************
*
* _ScbSystickClear - clear the SYSTICK exception
*
* This routine clears the SYSTICK exception.
*
* RETURNS: N/A
*/

static inline void _ScbSystickPendClear(void)
{
	__scs.scb.icsr.bit.pendstclr = 1;
}

/*******************************************************************************
*
* _ScbIsIrqPending - find out if an external interrupt is pending
*
* Find out if an external interrupt, generated by the NVIC, is pending.
*
* RETURNS: 1 if one or more interrupt is pending, 0 otherwise
*/

static inline int _ScbIsIrqPending(void)
{
	return __scs.scb.icsr.bit.isrpending;
}

/*******************************************************************************
*
* _ScbHiPriVectorPendingGet - find out the exception number of highest-priority
*                             pending exception (including interrupts)
*
* If one or more exceptions are pending, return the exception number of the
* highest-priority one; otherwise, return 0.
*
* RETURNS: the exception number if one is pending, 0 otherwise
*/

static inline int _ScbHiPriVectorPendingGet(void)
{
	union __icsr reg;

	reg.val = __scs.scb.icsr.val;
	return reg.bit.vectpending;
}

/*******************************************************************************
*
* _ScbIsNested - find out if the currently executing exception is nested
*
* This routine determines if the currently executing exception is nested.
*
* RETURNS: 1 if nested, 0 otherwise
*/

static inline int _ScbIsNestedExc(void)
{
	/* !bit == preempted exceptions */
	return !__scs.scb.icsr.bit.rettobase;
}

/*******************************************************************************
*
* _ScbIsInThreadMode - find out if running in thread mode
*
* This routine determines if the current mode is thread mode.
*
* RETURNS: 1 if in thread mode, 0 otherwise
*/

static inline int _ScbIsInThreadMode(void)
{
	/* 0 == thread mode */
	return !__scs.scb.icsr.bit.vectactive;
}

/*******************************************************************************
*
* _ScbIsInHandlerMode - find out if running in handler mode
*
* This routine determines if the current mode is handler mode.
*
* RETURNS: 1 if in handler mode, 0 otherwise
*/

static inline int _ScbIsInHandlerMode(void)
{
	return !_ScbIsInThreadMode();
}

/*******************************************************************************
*
* _ScbIsInExc - find out if handling an exception
*
* This routine determines if an exception is being handled (handler mode).
*
* RETURNS: 1 if handling an exception, 0 otherwise
*/

static inline int _ScbIsInExc(void)
{
	return _ScbIsInHandlerMode();
}

/*******************************************************************************
*
* _ScbActiveVectorGet - obtain the currently executing vector
*
* If currently handling an exception/interrupt, return the exceuting vector
* number. If not, return 0.
*
* RETURNS: the currently excecuting vector number, 0 if in thread mode.
*/

static inline uint32_t _ScbActiveVectorGet(void)
{
	return __scs.scb.icsr.bit.vectactive;
}

/*******************************************************************************
*
* _ScbIsVtableInSram - find out if vector table is in SRAM or ROM
*
* This routine determines if the currently executing exception is nested.
*
* RETURNS: 1 if in SRAM, 0 if in ROM
*/

static inline uint32_t _ScbIsVtableInSram(void)
{
	return !!__scs.scb.vtor.bit.tblbase;
}

/*******************************************************************************
*
* _ScbVtableLocationSet - move vector table from SRAM to ROM and vice-versa
*
* This routine moves the vector table to the given memory region.
*
* RETURNS: 1 if in SRAM, 0 if in ROM
*/

static inline void _ScbVtableLocationSet(
	int sram /* 1 to move vector to SRAM, 0 to move it to ROM */
	)
{
	__ASSERT(!(sram & 0xfffffffe), "");
	__scs.scb.vtor.bit.tblbase = sram;
}

/*******************************************************************************
*
* _ScbVtableAddrGet - obtain base address of vector table
*
* This routine returs the vector table's base address.
*
* RETURNS: the base address of the vector table
*/

static inline uint32_t _ScbVtableAddrGet(void)
{
	return __scs.scb.vtor.bit.tbloff;
}

/*******************************************************************************
*
* _ScbVtableAddrSet - set base address of vector table
*
* <addr> must align to the number of exception entries in vector table:
*
*    numException = 16 + num_interrupts where each entry is 4 Bytes
*
* As a minimum, <addr> must be a multiple of 128:
*
*  0 <= num_interrupts <  16: multiple 0x080
* 16 <= num_interrupts <  48: multiple 0x100
* 48 <= num_interrupts < 112: multiple 0x200
*               ....
*
* RETURNS: N/A
*/

static inline void _ScbVtableAddrSet(uint32_t addr /* base address, aligned on
						      128 minimum */
				     )
{
	__ASSERT(!(addr & 0x7F), "invalid vtable base Addr");
	__scs.scb.vtor.bit.tbloff = addr;
}

/*******************************************************************************
*
* _ScbIsDataLittleEndian - find out if data regions are little endian
*
* Data regions on Cortex-M devices can be either little or big endian. Code
* regions are always little endian.
*
* RETURNS: 1 if little endian, 0 if big endian
*/

static inline int _ScbIsDataLittleEndian(void)
{
	return !(__scs.scb.aircr.bit.endianness);
}

/*******************************************************************************
*
* _ScbNumPriGroupGet - get the programmed number of priority groups
*
* Exception priorities can be sub-divided into groups, with sub-priorities.
* Within these groups, exceptions do not preempt each other. The sub-priorities
* are only used to decide which exception will run when several are pending.
*
* RETURNS: the number of priority groups
*/

static inline int _ScbNumPriGroupGet(void)
{
	return 1 << (7 - __scs.scb.aircr.bit.prigroup);
}

/*******************************************************************************
*
* _ScbSleepOnExitSet - CPU goes to sleep after exiting an ISR
*
* CPU never runs in thread mode until this is cancelled.
*
* This enables the feature until it is cancelled.
*
* RETURNS: N/A
*/

static inline void _ScbSleepOnExitSet(void)
{
	__scs.scb.scr.bit.sleeponexit = 1;
}

/*******************************************************************************
*
* _ScbSleepOnExitClear - CPU does not go to sleep after exiting an ISR
*
* This routine prevents CPU sleep mode upon exiting an ISR.
* This is the normal operating mode.
*
* RETURNS: N/A
*/

static inline void _ScbSleepOnExitClear(void)
{
	__scs.scb.scr.bit.sleeponexit = 0;
}

/*******************************************************************************
*
* _ScbSevOnPendSet - do not put CPU to sleep if pending exception are present
*                    when invoking wfe instruction
*
* By default, when invoking wfi or wfe instructions, if PRIMASK is masking
* interrupts and if an interrupt is pending, the CPU will go to sleep, and
* another interrupt is needed to wake it up. By coupling the use of the
* SEVONPEND feature and the wfe instruction (NOT wfi), pending exception will
* prevent the CPU from sleeping.
*
* This enables the feature until it is cancelled.
*
* RETURNS: N/A
*/

static inline void _ScbSevOnPendSet(void)
{
	__scs.scb.scr.bit.sevonpend = 1;
}

/*******************************************************************************
*
* _ScbSevOnPendClear - clear SEVONPEND bit
*
* See _ScbSevOnPendSet().
*
* RETURNS: N/A
*/

static inline void _ScbSevOnPendClear(void)
{
	__scs.scb.scr.bit.sevonpend = 0;
}

/*******************************************************************************
*
* _ScbSleepDeepSet - when putting the CPU to sleep, put it in deep sleep
*
* When wfi/wfe is invoked, the CPU will go into a "deep sleep" mode, using less
* power than regular sleep mode, but with some possible side-effect.
*
* Behaviour is processor-specific.
*
* RETURNS: N/A
*/

static inline void _ScbSleepDeepSet(void)
{
	__scs.scb.scr.bit.sleepdeep = 1;
}

/*******************************************************************************
*
* _ScbSleepDeepSet - when putting the CPU to sleep, do not put it in deep sleep
*
* This routine prevents CPU deep sleep mode.
*
* RETURNS: N/A
*/

static inline void _ScbSleepDeepClear(void)
{
	__scs.scb.scr.bit.sleepdeep = 0;
}

/*******************************************************************************
*
* _ScbDivByZeroFaultEnable - enable faulting on division by zero
*
* This routine enables the divide by zero fault.
* By default, the CPU ignores the error.
*
* RETURNS: N/A
*/

static inline void _ScbDivByZeroFaultEnable(void)
{
	__scs.scb.ccr.bit.div_0_trp = 1;
}

/*******************************************************************************
*
* _ScbDivByZeroFaultDisable - ignore division by zero errors
*
* This routine disables the divide by zero fault.
* This is the default behaviour.
*
* RETURNS: N/A
*/

static inline void _ScbDivByZeroFaultDisable(void)
{
	__scs.scb.ccr.bit.div_0_trp = 0;
}

/*******************************************************************************
*
* _ScbUnalignedFaultEnable - enable faulting on unaligned access
*
* This routine enables the unaligned access fault.
* By default, the CPU ignores the error.
*
* RETURNS: N/A
*/

static inline void _ScbUnalignedFaultEnable(void)
{
	__scs.scb.ccr.bit.unalign_trp = 1;
}

/*******************************************************************************
*
* _ScbUnalignedFaultDisable - ignore unaligned access errors
*
* This routine disables the divide by zero fault.
* This is the default behaviour.
*
* RETURNS: N/A
*/

static inline void _ScbUnalignedFaultDisable(void)
{
	__scs.scb.ccr.bit.unalign_trp = 0;
}

/*******************************************************************************
*
* _ScbCcrSet - write the CCR all at once
*
* This routine writes the given value to the Configuration Control Register.
*
* RETURNS: N/A
*/

static inline void ScbCcrSet(uint32_t val /* value to write to CCR */
			     )
{
	__scs.scb.ccr.val = val;
}

/*******************************************************************************
*
* _ScbExcPrioGet - obtain priority of an exception
*
* Only works with exceptions 4 to 15, ie. do not use this for interrupts, which
* are exceptions 16+.
*
* Exceptions 1 to 3 priorities are fixed (-3, -2, -1).
*
* RETURNS: priority of exception <exc>
*/

static inline uint8_t _ScbExcPrioGet(uint8_t exc /* exception number, 4 to 15 */
				     )
{
	/* For priority exception handler 4-15 */
	__ASSERT((exc > 3) && (exc < 16), "");
	return __scs.scb.shpr[exc - 4];
}

/*******************************************************************************
*
* _ScbExcPrioSet - set priority of an exception
*
* Only works with exceptions 4 to 15, ie. do not use this for interrupts, which
* are exceptions 16+.
*
* Note that the processor might not implement all 8 bits, in which case the
* lower N bits are ignored.
*
* Exceptions 1 to 3 priorities are fixed (-3, -2, -1).
*
* RETURNS: N/A
*/

static inline void _ScbExcPrioSet(uint8_t exc, /* exception number, 4 to 15 */
				  uint8_t pri  /* priority, 0 to 255 */
				  )
{
	/* For priority exception handler 4-15 */
	__ASSERT((exc > 3) && (exc < 16), "");
	__scs.scb.shpr[exc - 4] = pri;
}

/*******************************************************************************
*
* _ScbUsageFaultEnable - enable usage fault exceptions
*
* This routine enables usage faults.
* By default, the CPU does not raise usage fault exceptions.
*
* RETURNS: N/A
*/

static inline void _ScbUsageFaultEnable(void)
{
	__scs.scb.shcsr.bit.usgfaultena = 1;
}

/*******************************************************************************
*
* _ScbUsageFaultDisable - disable usage fault exceptions
*
* This routine disables usage faults.
* This is the default behaviour.
*
* RETURNS: N/A
*/

static inline void _ScbUsageFaultDisable(void)
{
	__scs.scb.shcsr.bit.usgfaultena = 0;
}

/*******************************************************************************
*
* _ScbBusFaultEnable - enable bus fault exceptions
*
* This routine enables bus faults.
* By default, the CPU does not raise bus fault exceptions.
*
* RETURNS: N/A
*/

static inline void _ScbBusFaultEnable(void)
{
	__scs.scb.shcsr.bit.busfaultena = 1;
}

/*******************************************************************************
*
* _ScbBusFaultDisable - disable bus fault exceptions
*
* This routine disables bus faults.
* This is the default behaviour.
*
* RETURNS: N/A
*/

static inline void _ScbBusFaultDisable(void)
{
	__scs.scb.shcsr.bit.busfaultena = 0;
}

/*******************************************************************************
*
* _ScbMemFaultEnable - enable MPU faults exceptions
*
* This routine enables the MPU faults.
* By default, the CPU does not raise MPU fault exceptions.
*
* RETURNS: N/A
*/

static inline void _ScbMemFaultEnable(void)
{
	__scs.scb.shcsr.bit.memfaultena = 1;
}

/*******************************************************************************
*
* _ScbMemFaultDisable - disable MPU fault exceptions
*
* This routine disables MPU faults.
* This is the default behaviour.
*
* RETURNS: N/A
*/

static inline void _ScbMemFaultDisable(void)
{
	__scs.scb.shcsr.bit.memfaultena = 0;
}

/*******************************************************************************
*
* _ScbHardFaultIsBusErrOnVectorRead - find out if a hard fault is caused by
*                                     a bus error on vector read
*
* This routine determines if a hard fault is caused by a bus error during
* a vector table read operation.
*
* RETURNS: 1 if so, 0 otherwise
*/

static inline int _ScbHardFaultIsBusErrOnVectorRead(void)
{
	return __scs.scb.hfsr.bit.vecttbl;
}

/*******************************************************************************
*
* _ScbHardFaultIsForced - find out if a fault was escalated to hard fault
*
* Happens if a fault cannot be triggered because of priority or because it was
* disabled.
*
* RETURNS: 1 if so, 0 otherwise
*/

static inline int _ScbHardFaultIsForced(void)
{
	return __scs.scb.hfsr.bit.forced;
}

/*******************************************************************************
*
* _ScbHardFaultAllFaultsReset - clear all hard faults (HFSR register)
*
* HFSR register is a 'write-one-to-clear' (W1C) register.
*
* RETURNS: 1 if so, 0 otherwise
*/

static inline int _ScbHardFaultAllFaultsReset(void)
{
	return __scs.scb.hfsr.val = 0xffff;
}

/*******************************************************************************
*
* _ScbIsMemFault - find out if a hard fault is an MPU fault
*
* This routine determines if a hard fault is an MPU fault.
*
* RETURNS: 1 if so, 0 otherwise
*/

static inline int _ScbIsMemFault(void)
{
	return !!__scs.scb.cfsr.byte.mmfsr.val;
}

/*******************************************************************************
*
* _ScbMemFaultIsMmfarValid - find out if the MMFAR register contains a valid
*                            value
*
* The MMFAR register contains the faulting address on an MPU fault.
*
* RETURNS: 1 if so, 0 otherwise
*/

static inline int _ScbMemFaultIsMmfarValid(void)
{
	return !!__scs.scb.cfsr.byte.mmfsr.bit.mmarvalid;
}

/*******************************************************************************
*
* _ScbMemFaultMmfarReset - invalid the value in MMFAR
*
* This routine invalidates the MMFAR value.  This should be done after
* processing an MPU fault.
*
* RETURNS: N/A
*/

static inline void _ScbMemFaultMmfarReset(void)
{
	__scs.scb.cfsr.byte.mmfsr.bit.mmarvalid = 0;
}

/*******************************************************************************
*
* _ScbMemFaultAllFaultsReset - clear all MPU faults (MMFSR register)
*
* CFSR/MMFSR register is a 'write-one-to-clear' (W1C) register.
*
* RETURNS: 1 if so, 0 otherwise
*/

static inline void _ScbMemFaultAllFaultsReset(void)
{
	__scs.scb.cfsr.byte.mmfsr.val = 0xfe;
}

/*******************************************************************************
*
* _ScbMemFaultIsStacking - find out if an MPU fault is a stacking fault
*
* This routine determines if an MPU fault is a stacking fault.
* This may occur upon exception entry.
*
* RETURNS: 1 if so, 0 otherwise
*/

static inline int _ScbMemFaultIsStacking(void)
{
	return !!__scs.scb.cfsr.byte.mmfsr.bit.mstkerr;
}

/*******************************************************************************
*
* _ScbMemFaultIsUnstacking - find out if an MPU fault is an unstacking fault
*
* This routine determines if an MPU fault is an unstacking fault.
* This may occur upon exception exit.
*
* RETURNS: 1 if so, 0 otherwise
*/

static inline int _ScbMemFaultIsUnstacking(void)
{
	return !!__scs.scb.cfsr.byte.mmfsr.bit.munstkerr;
}

/*******************************************************************************
*
* _ScbMemFaultIsDataAccessViolation - find out if an MPU fault is a data access
*                                     violation
*
* If this routine returns 1, read the MMFAR register via _ScbMemFaultAddrGet()
* to get the faulting address.
*
* RETURNS: 1 if so, 0 otherwise
*/

static inline int _ScbMemFaultIsDataAccessViolation(void)
{
	return !!__scs.scb.cfsr.byte.mmfsr.bit.daccviol;
}

/*******************************************************************************
*
* _ScbMemFaultIsInstrAccessViolation - find out if an MPU fault is an
*                                      instruction access violation
*
* This routine determines if an MPU fault is due to an instruction access
* violation.
*
* RETURNS: 1 if so, 0 otherwise
*/

static inline int _ScbMemFaultIsInstrAccessViolation(void)
{
	return !!__scs.scb.cfsr.byte.mmfsr.bit.iaccviol;
}

/*******************************************************************************
*
* _ScbMemFaultAddrGet - find out the faulting address on an MPU fault
*
* RETURNS: the faulting address
*/

static inline uint32_t _ScbMemFaultAddrGet(void)
{
	return __scs.scb.mmfar;
}

/*******************************************************************************
*
* _ScbIsBusFault - find out if a hard fault is a bus fault
*
* This routine determines if a hard fault is a bus fault.
*
* RETURNS: 1 if so, 0 otherwise
*/

static inline int _ScbIsBusFault(void)
{
	return !!__scs.scb.cfsr.byte.bfsr.val;
}

/*******************************************************************************
*
* _ScbBusFaultIsBfarValid - find out if the BFAR register contains a valid
*                           value
*
* The BFAR register contains the faulting address on bus fault.
*
* RETURNS: 1 if so, 0 otherwise
*/

static inline int _ScbBusFaultIsBfarValid(void)
{
	return !!__scs.scb.cfsr.byte.bfsr.bit.bfarvalid;
}

/*******************************************************************************
*
* _ScbMemFaultBfarReset - invalid the value in BFAR
*
* This routine clears/invalidates the Bus Fault Address Register.
* It should be done after processing a bus fault.
*
* RETURNS: N/A
*/

static inline void _ScbBusFaultBfarReset(void)
{
	__scs.scb.cfsr.byte.bfsr.bit.bfarvalid = 0;
}

/*******************************************************************************
*
* _ScbBusFaultAllFaultsReset - clear all bus faults (BFSR register)
*
* CFSR/BFSR register is a 'write-one-to-clear' (W1C) register.
*
* RETURNS: N/A
*/

static inline void _ScbBusFaultAllFaultsReset(void)
{
	__scs.scb.cfsr.byte.bfsr.val = 0xfe;
}

/*******************************************************************************
*
* _ScbBusFaultIsStacking - find out if a bus fault is a stacking fault
*
* This routine determines if a bus fault is a stacking fault.
* This may occurs upon exception entry.
*
* RETURNS: 1 if so, 0 otherwise
*/

static inline int _ScbBusFaultIsStacking(void)
{
	return !!__scs.scb.cfsr.byte.bfsr.bit.stkerr;
}

/*******************************************************************************
*
* _ScbBusFaultIsUnstacking - find out if a bus fault is an unstacking fault
*
* This routine determines if a bus fault is an unstacking fault.
* This may occur upon exception exit.
*
* RETURNS: 1 if so, 0 otherwise
*/

static inline int _ScbBusFaultIsUnstacking(void)
{
	return !!__scs.scb.cfsr.byte.bfsr.bit.unstkerr;
}

/*******************************************************************************
*
* _ScbBusFaultIsImprecise - find out if a bus fault is an imprecise error
*
* This routine determines if a bus fault is an imprecise error.
*
* RETURNS: 1 if so, 0 otherwise
*/

static inline int _ScbBusFaultIsImprecise(void)
{
	return !!__scs.scb.cfsr.byte.bfsr.bit.impreciserr;
}

/*******************************************************************************
*
* _ScbBusFaultIsPrecise - find out if a bus fault is an precise error
*
* Read the BFAR register via _ScbBusFaultAddrGet() if this routine returns 1,
* as it will contain the faulting address.
*
* RETURNS: 1 if so, 0 otherwise
*/

static inline int _ScbBusFaultIsPrecise(void)
{
	return !!__scs.scb.cfsr.byte.bfsr.bit.preciserr;
}

/*******************************************************************************
*
* _ScbBusFaultIsInstrBusErr - find out if a bus fault is an instruction bus
*                             error
*
* This routine determines if a bus fault is an instruction bus error.
* It is signalled only if the instruction is issued.
*
* RETURNS: 1 if so, 0 otherwise
*/

static inline int _ScbBusFaultIsInstrBusErr(void)
{
	return !!__scs.scb.cfsr.byte.bfsr.bit.ibuserr;
}

/*******************************************************************************
*
* _ScbBusFaultAddrGet - get the faulting address on a precise bus fault
*
* This routine returns the faulting address for a precise bus fault.
*
* RETURNS: the faulting address
*/

static inline uint32_t _ScbBusFaultAddrGet(void)
{
	return __scs.scb.bfar;
}

/*******************************************************************************
*
* _ScbIsUsageFault - find out if a hard fault is a usage fault
*
* This routine determines if a hard fault is a usage fault.
*
* RETURNS: 1 if so, 0 otherwise
*/

static inline int _ScbIsUsageFault(void)
{
	return !!__scs.scb.cfsr.byte.ufsr.val;
}

/*******************************************************************************
*
* _ScbUsageFaultIsDivByZero - find out if a usage fault is a 'divide by zero'
*                             fault
*
* This routine determines if a usage fault is a 'divde by zero' fault.
*
* RETURNS: 1 if so, 0 otherwise
*/

static inline int _ScbUsageFaultIsDivByZero(void)
{
	return !!__scs.scb.cfsr.byte.ufsr.bit.divbyzero;
}

/*******************************************************************************
*
* _ScbUsageFaultIsUnaligned - find out if a usage fault is a unaligned access
*                             error
*
* This routine determines if a usage fault is an unaligned access error.
*
* RETURNS: 1 if so, 0 otherwise
*/

static inline int _ScbUsageFaultIsUnaligned(void)
{
	return !!__scs.scb.cfsr.byte.ufsr.bit.unaligned;
}

/*******************************************************************************
*
* _ScbUsageFaultIsNoCp - find out if a usage fault is a coprocessor access
*                        error
*
* This routine determines if a usage fault is caused by a coprocessor access.
* This happens if the coprocessor is either absent or disabled.
*
* RETURNS: 1 if so, 0 otherwise
*/

static inline int _ScbUsageFaultIsNoCp(void)
{
	return !!__scs.scb.cfsr.byte.ufsr.bit.nocp;
}

/*******************************************************************************
*
* _ScbUsageFaultIsInvalidPcLoad - find out if a usage fault is a invalid PC
*                                 load error
*
* Happens if the the instruction address on an exception return is not
* halfword-aligned.
*
* RETURNS: 1 if so, 0 otherwise
*/

static inline int _ScbUsageFaultIsInvalidPcLoad(void)
{
	return !!__scs.scb.cfsr.byte.ufsr.bit.invpc;
}

/*******************************************************************************
*
* _ScbUsageFaultIsInvalidState - find out if a usage fault is a invalid state
*			         error
*
* Happens if the the instruction address loaded in the PC via a branch, LDR or
* POP, or if the instruction address installed in a exception vector, does not
* have bit 0 set, ie., is not halfword-aligned.
*
* RETURNS: 1 if so, 0 otherwise
*/

static inline int _ScbUsageFaultIsInvalidState(void)
{
	return !!__scs.scb.cfsr.byte.ufsr.bit.invstate;
}

/*******************************************************************************
*
* _ScbUsageFaultIsUndefinedInstr - find out if a usage fault is a undefined
*			           instruction error
*
* The processor tried to execute an invalid opcode.
*
* RETURNS: 1 if so, 0 otherwise
*/

static inline int _ScbUsageFaultIsUndefinedInstr(void)
{
	return !!__scs.scb.cfsr.byte.ufsr.bit.undefinstr;
}

/*******************************************************************************
*
* _ScbUsageFaultDivByZeroReset - clear the 'division by zero' fault
*
* CFSR/UFSR register is a 'write-one-to-clear' (W1C) register.
*
* RETURNS: N/A
*/

static inline void _ScbUsageFaultDivByZeroReset(void)
{
	__scs.scb.cfsr.byte.ufsr.bit.divbyzero = 1;
}

/*******************************************************************************
*
* _ScbUsageFaultUnalignedReset - clear the 'unaligned access' fault
*
* CFSR/UFSR register is a 'write-one-to-clear' (W1C) register.
*
* RETURNS: N/A
*/

static inline void _ScbUsageFaultUnalignedReset(void)
{
	__scs.scb.cfsr.byte.ufsr.bit.unaligned = 1;
}

/*******************************************************************************
*
* _ScbUsageFaultNoCpReset - clear the 'no co-processor' fault
*
* CFSR/UFSR register is a 'write-one-to-clear' (W1C) register.
*
* RETURNS: N/A
*/

static inline void _ScbUsageFaultNoCpReset(void)
{
	__scs.scb.cfsr.byte.ufsr.bit.nocp = 1;
}

/*******************************************************************************
*
* _ScbUsageFaultInvalidPcLoadReset - clear the 'invalid PC load ' fault
*
* CFSR/UFSR register is a 'write-one-to-clear' (W1C) register.
*
* RETURNS: N/A
*/

static inline void _ScbUsageFaultInvalidPcLoadReset(void)
{
	__scs.scb.cfsr.byte.ufsr.bit.invpc = 1;
}

/*******************************************************************************
*
* _ScbUsageFaultInvalidStateReset - clear the 'invalid state' fault
*
* CFSR/UFSR register is a 'write-one-to-clear' (W1C) register.
*
* RETURNS: N/A
*/

static inline void _ScbUsageFaultInvalidStateReset(void)
{
	__scs.scb.cfsr.byte.ufsr.bit.invstate = 1;
}

/*******************************************************************************
*
* _ScbUsageFaultUndefinedInstrReset - clear the 'undefined instruction' fault
*
* CFSR/UFSR register is a 'write-one-to-clear' (W1C) register.
*
* RETURNS: N/A
*/

static inline void _ScbUsageFaultUndefinedInstrReset(void)
{
	__scs.scb.cfsr.byte.ufsr.bit.undefinstr = 1;
}

/*******************************************************************************
*
* _ScbUsageFaultAllFaultsReset - clear all usage faults (UFSR register)
*
* CFSR/UFSR register is a 'write-one-to-clear' (W1C) register.
*
* RETURNS: N/A
*/

static inline void _ScbUsageFaultAllFaultsReset(void)
{
	__scs.scb.cfsr.byte.ufsr.val = 0xffff;
}
#endif /* _ASMLANGUAGE */

#endif /* _SCB__H_ */
