

#ifndef __ARCH_CORE_H__
#define __ARCH_CORE_H__

#ifdef __cplusplus
    extern "C" {
#endif

#if defined(__ARCH_ARM_M4__)

#define __LITTLE_ENDIAN__
#define __LITTLE_ENDIAN_BF__

#include "Arm/m4/inc/machM4.h"

#elif defined(__ARCH_ARM_M7__)

#define __LITTLE_ENDIAN__
#define __LITTLE_ENDIAN_BF__

#include "Arm/m7/inc/machM7.h"

#else /*__ARCH_ARM_M4__*/

#error "CPU undefined"

#endif

#ifndef arch_get_usr_heap
extern void __arch_get_usr_heap (void *, void *);
#define arch_get_usr_heap __arch_get_usr_heap
#endif

#ifndef arch_soft_reset
extern void __arch_soft_reset (void);
#define arch_soft_reset __arch_soft_reset
#endif

#ifndef arch_rise
extern void __arch_rise (void *args);
#define arch_rise __arch_rise
#endif


#ifndef arch_get_stack
extern void __arch_get_stack (void *, void *);
#define arch_get_stack __arch_get_stack
#endif

#ifndef arch_get_heap
extern void __arch_get_heap (void *, void *);
#define arch_get_heap __arch_get_heap
#endif

#ifndef arch_asmgoto
extern void __arch_asmgoto (void *);
#define arch_asmgoto __arch_asmgoto
#endif

#ifndef arch_get_shared
extern void __arch_get_shared (void *, void *);
#define arch_get_shared __arch_get_shared
#endif

#ifndef arch_startup
extern void __arch_startup (void);
#define arch_startup __arch_startup
#endif

#ifndef arch_set_sp
#define arch_set_sp __set_MSP
#endif

#ifndef arch_dsb
#define arch_dsb __DSB
#endif

static int16_t ReadLeI16 (void *_p)
{
    uint8_t *p = (uint8_t *)_p;
    int16_t res;

    res = ((p[0] & 0xff));
    res |= (p[1] & 0xff) << 8;

    return res;
}

static uint16_t ReadLeU16 (void *_p)
{
    uint8_t *p = (uint8_t *)_p;
    uint16_t res;

    res = ((p[0] & 0xff));
    res |= (p[1] & 0xff) << 8;

    return res;
}

static int32_t ReadLeI32 (void *_p)
{
    uint8_t *p = (uint8_t *)_p;
    int32_t res;

    res = ((p[0] & 0xff));
    res |= (p[1] & 0xff) << 8;
    res |= (p[2] & 0xff) << 16;
    res |= (p[3] & 0xff) << 24;

    return res;
}

static uint32_t ReadLeU32 (void *_p)
{
    uint8_t *p = (uint8_t *)_p;
    uint32_t res;

    res = ((p[0] & 0xff));
    res |= (p[1] & 0xff) << 8;
    res |= (p[2] & 0xff) << 16;
    res |= (p[3] & 0xff) << 24;

    return res;
}

static void *ReadLeP (void *_p)
{
    return (void *)ReadLeU32(_p);
}


#define READ_LE_I16(x) \
    ReadLeI16((void *)&(x))

#define READ_LE_I16_P(p) \
    ReadLeI16((void *)(p))

#define READ_LE_U16(x) \
    ReadLeU16((void *)&(x))

#define READ_LE_U16_P(p) \
    ReadLeU16((void *)(p))

#define READ_LE_I32(x) \
    ReadLeI32((void *)&(x))

#define READ_LE_I32_P(p) \
    ReadLeI32((void *)(p))

#define READ_LE_U32(x) \
    ReadLeU32((void *)&(x))

#define READ_LE_U32_P(p) \
    ReadLeU32((void *)(p))

#define READ_LE_P(x) \
    ReadLeP((void *)&(x))

#define READ_LE_P_P(p) \
    ReadLeP((void *)(p))

#ifdef __cplusplus
    }
#endif

#endif /*__ARCH_CORE_H__*/
