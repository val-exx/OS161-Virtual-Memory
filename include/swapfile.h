#ifndef _SWAPFILEH
#define _SWAPFILEH

#include <types.h>


#define SWAP_SIZE 9 * 1024 * 1024 //In order to make easy to change
#define SWAPFILE_NAME "emu0:SWAPFILE" //directory in which the swapfile is generated 



//#if OPT_SWAP

struct swap_map
{
    struct addrspace *as;
    vaddr_t vaddr;
    int free;
};

void            swap_bootstrap(void);
int             swap_in(vaddr_t vadd, paddr_t new_paddr);
int             swap_out(paddr_t paddr,vaddr_t vaddr);
void            swap_free(struct addrspace *as, vaddr_t *vadd, int conf );
void            swap_destroy(void);
int             swap_tracker(vaddr_t vaddr);
int             swap_alloc(void);
void            swap_set(int index, vaddr_t vaddr,struct addrspace *as);

//#endif /* OPT_SWAP /

#endif 