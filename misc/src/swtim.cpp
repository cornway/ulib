#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "../inc/swtim.h"

#include <arch.h>
#include <bsp_api.h>
#include <misc_utils.h>
#include <dev_io.h>
#include <debug.h>

#include "../inc/iterable.h"

class Swtim : public Link<Swtim> {
    private :
        swtim_chandler_t hdlr;
        uint32_t timeout, tstamp;
        uint32_t flags;
        void *arg;
    public :
        Swtim (swtim_chandler_t h, void *arg, uint32_t flags) :
            hdlr(h), timeout(0), tstamp(0), flags(flags), arg(arg)
        {
        }

        void Timeout (uint32_t t)
        {
            this->timeout = t;
            this->tstamp = d_time();
        }

        void UnTimeout (void)
        {
            this->timeout = 0;
            this->tstamp = 0;
        }

        void Sync (void)
        {
            while (!this->Tick()) {
                d_sleep(1);
            }
        }

        int Tick (void)
        {
            if (this->timeout + this->tstamp > d_time()) {
                return 0;
            }
            this->hdlr(this->arg);
            this->UnTimeout();
            return 1;
        }
};

static vector::Vector<Swtim> SwtimPool;

void swtim_core_init (void)
{
}

void *swtim_init (swtim_chandler_t h, void *arg, uint32_t flags)
{
    Swtim *swtim = new Swtim(h, arg, flags);

    SwtimPool.add(swtim);
    return swtim;
}

void *swtim_timeout (void *inst, uint32_t ms)
{
    Swtim *swtim = (Swtim *)inst;
    swtim->Timeout(ms);
    return inst;
}

void *swtim_untimeout (void *inst)
{
    Swtim *swtim = (Swtim *)inst;
    swtim->UnTimeout();
    return inst;
}

void *swtim_sync (void *inst)
{
    Swtim *swtim = (Swtim *)inst;
    swtim->Sync();
    return inst;
}

void swtim_tick (void)
{
    for (auto tim : SwtimPool) {
        tim->Tick();
    }
}

