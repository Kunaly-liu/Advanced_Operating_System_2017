#ifndef _COMM_H_
#define _COMM_H_
#include <autoconf.h>
#include <sel4/bootinfo.h>
#include <fcntl.h>
#include <unistd.h>

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <sel4/types.h>
#include <nfs/nfs.h>
#include <errno.h>


#include <cspace/cspace.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <sel4/types.h>
#include <cspace/cspace.h>
#include <sys/panic.h>
#include <sel4/sel4.h>

#include <ut_manager/ut.h>

#define mode_t uint32_t
#ifndef verbose
    #define verbose 5
    #include <sys/debug.h>
    #define comm_verbose 1
#endif

#define DIVROUND(a,b) (((a) + ((b) - 1)) / (b))
#define seL4_PAGE_SIZE          (1 << seL4_PageBits)

// mess sel4 cap+addr unit
struct sos_object
{
    seL4_Word addr;
    seL4_CPtr cap;
};

static inline void clear_sos_object(struct sos_object* obj)
{
    assert(obj != NULL);
    obj->addr = 0;
    obj->cap  = 0;
}

// only responsible for free sos object , not app object.
static inline void free_sos_object(struct sos_object* obj, int size_bits)
{
    assert(obj != NULL);
    cspace_t * target_cspace = cur_cspace;

    if (obj->cap != 0)
    {
        assert(0 == cspace_delete_cap(target_cspace, obj->cap));
    }
    if (obj->addr != 0)
    {
        ut_free(obj->addr, size_bits);
    }
    clear_sos_object(obj);
    return;
}

static inline int init_sos_object(struct sos_object* obj, seL4_ArchObjectType type, int size_bits)
{
    free_sos_object(obj, size_bits);

    obj->addr = ut_alloc(size_bits);
    if (obj->addr == 0)
    {
        ERROR_DEBUG("ut_alloc return 0\n");
        free_sos_object(obj, size_bits);
        return -1;
    }
    int ret = cspace_ut_retype_addr(obj->addr,
                                    type,
                                    size_bits,
                                    cur_cspace,
                                    &(obj->cap));
    if (ret != 0)
    {
        ERROR_DEBUG( "cspace_ut_retype_addr ret: %d\n", ret);
        free_sos_object(obj, size_bits);
        return -2;
    }

    return 0;
}

/* To differencient between async and and sync IPC, we assign a
 * badge to the async endpoint. The badge that we receive will
 * be the bitwise 'OR' of the async endpoint badge and the badges
 * of all pending notifications. */
#define IRQ_EP_BADGE         (1 << (seL4_BadgeBits - 1))
#define IRQ_BADGE_NETWORK    (1 << 0)
#define IRQ_EPIT1_BADGE      (1 << 2)
#define IRQ_GPT_BADGE        (1 << 1)



static inline seL4_CPtr badge_irq_ep(seL4_CPtr ep, seL4_Word badge)
{

    seL4_CPtr badged_cap = cspace_mint_cap(cur_cspace, cur_cspace, ep, seL4_AllRights, seL4_CapData_Badge_new(badge | IRQ_EP_BADGE));
    conditional_panic(!badged_cap, "Failed to allocate badged cap");
    return badged_cap;
}



static inline seL4_CPtr enable_irq(int irq, seL4_CPtr aep)
{
    seL4_CPtr cap;
    int err;
    /* Create an IRQ handler */
    cap = cspace_irq_control_get_cap(cur_cspace, seL4_CapIRQControl, irq);
    conditional_panic(!cap, "Failed to acquire and IRQ control cap");
    /* Assign to an end point */
    err = seL4_IRQHandler_SetEndpoint(cap, aep);
    conditional_panic(err, "Failed to set interrupt endpoint");
    /* Ack the handler before continuing */
    err = seL4_IRQHandler_Ack(cap);
    conditional_panic(err, "Failure to acknowledge pending interrupts");
    return cap;
}

static inline void print_current_cap()
{
    uint32_t cur = cspace_copy_cap(cur_cspace, cur_cspace, seL4_CapInitThreadTCB, seL4_AllRights);
    printf("current cap id: %u\n", cur);
    cspace_delete_cap(cur_cspace, cur);
}

// #undef verbose
#ifdef comm_verbose
#undef verbose
#endif
#endif

