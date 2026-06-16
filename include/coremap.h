#ifndef _COREMAPH
#define _COREMAPH

#include "opt-dumbvm.h"
#include "opt-PAGING.h"

#if OPT_PAGING

struct coremap_entry{

    struct PTE * cm_ptentry;/*page table entry of the page living 
                                                    in this frame, NULL if kernel page  */

    unsigned int chunck_size; //lenght of occupied frames
    unsigned char cm_lock;
    bool busy; // 0 means free , 1 means busy
};

void        coremap_bootstrap(void);
paddr_t coremap_getppages(unsigned npages, struct PTE *ptentry);
void        coremap_freeppages(paddr_t addr);

#endif
#endif  