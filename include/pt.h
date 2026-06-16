#ifndef PT_H
#define PT_H

#include <types.h>
#include <addrspace.h>
#include "opt-PAGING.h"

struct addrspace;

struct PTE{
        vaddr_t vaddr;
        paddr_t paddr; 
        bool free; // free=1 the page is free, otherwise the page is occupied
        bool dirty;
        unsigned int segtype; // segtype=0 -> CODE, segtype=1 -> DATA, segtype=2 -> STACK, segtype=3 -> UNDEFINED
};

int initialize_pt(struct addrspace *as);
int set_dirty_bit_pt(bool dirty, paddr_t paddr);
int remove_pte(vaddr_t vaddr, paddr_t paddr);
bool check_if_any_dirtyzero(void);
paddr_t page_in_pt(vaddr_t vaddr); /*Search an entry in page table*/
paddr_t page_replacement(vaddr_t vaddr,unsigned int segtype);

#endif