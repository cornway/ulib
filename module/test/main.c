#if defined(MODULE) && defined(MOD_TEST)

#include "../../int/bsp_mod_int.h"
#include "main.h"
#include <bsp_api.h>
#include <debug.h>
#include <bsp_sys.h>

static const char *test_api_info (void);

const static test_api_t test_api =
{
    .info = test_api_info,
    .name = "test"
};

int main (int argc, const char **argv)
{
    g_bspapi = bsp_api_attach();

    dprintf("TEST MODULE+\n");

    bspmod_register_api(test_api.name, &test_api, sizeof(test_api));

    dprintf("TEST MODULE-\n");
    return 0;
}

static const char *test_api_info (void)
{
    dprintf("====================================\n");
    dprintf("TEST MODULE\n");
    dprintf("Build : %s %s\n", __DATE__, __TIME__);
    dprintf("====================================\n");

    return "TEST MODULE";
}

#endif /*MODULE && MOD_TEST*/
