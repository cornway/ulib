#ifndef __MACH_M4_H__
#define __MACH_M4_H__

#include <stdint.h>

#ifdef __cplusplus
    extern "C" {
#endif

#if defined (__ARMCC_VERSION)

#define V_PREPACK
#define V_POSTPACK __attribute__((packed))
#define PACKED __packed

#define _VALUES_IN_REGS __value_in_regs

#elif defined(__ARMGCC_VERSION)

#define V_PREPACK
#define V_POSTPACK __attribute__((packed))
#define PACKED V_POSTPACK

#define _VALUES_IN_REGS

#else
#error "UNKNOWN COMPILER!"
#endif

#define CPU_CHACHELINE (32)

typedef uint64_t      arch_dword_t;
typedef uint32_t      arch_word_t;
typedef uint16_t      arch_hword_t;
typedef uint8_t       arch_byte_t; 

#define UINT32_T      uint32_t
#define INT32_T       int32_t
#define INT64_T       int64_t
#define UINT64_T      uint64_t

#define _WEAK __weak

#define _STATIC static

#define _EXTERN extern

#define _UNUSED(a) a __attribute__((unused))

#define arch_tick_alias                 export_mach_m4_sys_tick
#define arch_pend_alias                 export_mach_m4_psv
#define arch_hard_fault_alias           export_mach_m4_hard
#define arch_fpu_en_alias               export_mach_m4_fpu_en
#define arch_swrst_alias                export_mach_m4_swrst
#define arch_upcall_alias               export_mach_m4_svc
#define arch_boot_alias                 export_mach_m4_boot

#ifdef __cplusplus
    }
#endif

#endif /*__MACH_M7_H__*/
