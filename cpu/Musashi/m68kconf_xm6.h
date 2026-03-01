/* ======================================================================== */
/* Musashi configuration for XM6 X68000 Emulator                            */
/* ======================================================================== */

#ifndef M68KCONF_XM6__HEADER
#define M68KCONF_XM6__HEADER

/* Configuration switches */
#define OPT_OFF             0
#define OPT_ON              1
#define OPT_SPECIFY_HANDLER 2

/* Not MAME */
#ifndef M68K_COMPILE_FOR_MAME
#define M68K_COMPILE_FOR_MAME      OPT_OFF
#endif

#if M68K_COMPILE_FOR_MAME == OPT_OFF

/* ======================================================================== */
/* ======================== MSVC COMPATIBILITY ============================ */
/* ======================================================================== */
/* Include stdio/stdlib before remapping inline to avoid CRT errors */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* MSVC compiler in C mode requires __inline instead of inline */
#if defined(_MSC_VER) && !defined(inline) && !defined(__cplusplus)
#define inline __inline
#endif

/* MSVC uint definition for Musashi */
typedef unsigned int uint;

/* ======================================================================== */
/* Callbacks implemented in musashi_adapter.cpp                             */
/* ======================================================================== */
#ifdef __cplusplus
extern "C" {
#endif
	int musashi_int_ack_callback(int int_level);
	void musashi_reset_callback(void);
	void musashi_fc_callback(unsigned int new_fc);
#ifdef __cplusplus
}
#endif

/* ======================================================================== */
/* ======================== XM6-SPECIFIC CONFIG =========================== */
/* ======================================================================== */

/* XM6 only emulates 68000 - disable everything else for performance */
#define M68K_EMULATE_010            OPT_OFF
#define M68K_EMULATE_EC020          OPT_OFF
#define M68K_EMULATE_020            OPT_OFF
#define M68K_EMULATE_030            OPT_OFF
#define M68K_EMULATE_040            OPT_OFF

/* XM6 doesn't need separate immediate/PC-relative reads */
#define M68K_SEPARATE_READS         OPT_OFF

/* Not needed for X68000 */
#define M68K_SIMULATE_PD_WRITES     OPT_OFF

/* XM6 needs interrupt acknowledge to dispatch to the correct device */
#define M68K_EMULATE_INT_ACK        OPT_SPECIFY_HANDLER
#define M68K_INT_ACK_CALLBACK(A)    musashi_int_ack_callback(A)

/* Not needed */
#define M68K_EMULATE_BKPT_ACK       OPT_OFF
#define M68K_BKPT_ACK_CALLBACK()    your_bkpt_ack_handler_function()

/* Trace - not currently used by XM6 */
#define M68K_EMULATE_TRACE          OPT_OFF

/* XM6 CPU::ResetInst() needs this callback for the RESET instruction */
#define M68K_EMULATE_RESET          OPT_SPECIFY_HANDLER
#define M68K_RESET_CALLBACK()       musashi_reset_callback()

/* Not needed */
#define M68K_CMPILD_HAS_CALLBACK    OPT_OFF
#define M68K_CMPILD_CALLBACK(v,r)   your_cmpild_handler_function(v,r)

#define M68K_RTE_HAS_CALLBACK       OPT_OFF
#define M68K_RTE_CALLBACK()         your_rte_handler_function()

#define M68K_TAS_HAS_CALLBACK       OPT_OFF
#define M68K_TAS_CALLBACK()         your_tas_handler_function()

#define M68K_ILLG_HAS_CALLBACK      OPT_OFF
#define M68K_ILLG_CALLBACK(opcode)  op_illg(opcode)

/* Function code callback for user/supervisor distinction.
 * XM6 needs this to properly emulate memory access permissions.
 */
#define M68K_EMULATE_FC             OPT_SPECIFY_HANDLER
#define M68K_SET_FC_CALLBACK(A)     musashi_fc_callback(A)

/* Not needed for XM6 */
#define M68K_MONITOR_PC             OPT_OFF
#define M68K_SET_PC_CALLBACK(A)     your_pc_changed_handler_function(A)

/* Instruction hook - not used */
extern "C" void musashi_trace_hook(unsigned int pc);
#define M68K_INSTRUCTION_HOOK       OPT_SPECIFY_HANDLER
#define M68K_INSTRUCTION_CALLBACK(pc) musashi_trace_hook(pc)

/* 68000 prefetch emulation - improves accuracy */
#define M68K_EMULATE_PREFETCH       OPT_OFF

/* Address error exceptions - XM6 uses these (cpu.cpp:630) */
#define M68K_EMULATE_ADDRESS_ERROR  OPT_ON

/* Logging - off for release */
#define M68K_LOG_ENABLE             OPT_OFF
#define M68K_LOG_1010_1111          OPT_OFF
#define M68K_LOG_FILEHANDLE         stderr

/* PMMU - 68000 doesn't have it */
#define M68K_EMULATE_PMMU           OPT_OFF

/* Use 64-bit integers for speed on modern CPUs */
#define M68K_USE_64_BIT             OPT_ON

#endif /* M68K_COMPILE_FOR_MAME */

#endif /* M68KCONF_XM6__HEADER */
