#ifndef __TEST_MOD_MAIN_H__
#define __TEST_MOD_MAIN_H__

#if defined(MODULE) && defined(MOD_TEST)

typedef struct {
    const char *(*info) (void);
    const char *name;
} test_api_t;

#endif /*MODULE && MOD_TEST*/

#endif /*__TEST_MOD_MAIN_H__*/
