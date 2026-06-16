#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <cpu.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <pt.h>
#include <vm_tlb.h>
#include <swapfile.h>
#include <vmstats.h>


static int page_victim(); /*Get victim page*/
paddr_t getppages(unsigned npages, struct PTE *ptentry);


int initialize_pt(struct addrspace *as){
paddr_t paddr;
     if (as==NULL) return 1;

     for(int i=0;i<N_FRAME;i++){

         paddr = getppages(1, as->page_table[i]);

         if(!paddr) return 2;

         as->page_table[i]->paddr=paddr; 
         as->page_table[i]->vaddr=0x00000000;
         as->page_table[i]->dirty=1;
         as->page_table[i]->free=1;
         as->page_table[i]->segtype=3;

     }
     return 0;
}

paddr_t page_in_pt(vaddr_t vaddr){
    int i;
    struct addrspace *as = proc_getas();
    for(i = 0; i < N_FRAME; i++){
        if(vaddr == as->page_table[i]->vaddr)
            return as->page_table[i]->paddr;
    }
    return 0;
}

static int page_victim(void){
    static int victim = 0;
    return victim++ % N_FRAME;
}



int set_dirty_bit_pt(bool dirty, paddr_t paddr){
    struct addrspace *as = proc_getas();
    for(int i=0;i<N_FRAME;i++){
        if(as->page_table[i]->paddr == paddr) {
            as->page_table[i]->dirty=dirty;
            return 1;
        }
    }
    return 0;
}

bool check_if_any_dirtyzero(void){
    struct addrspace *as = proc_getas();
    int i;
   
    for(i=0;i<N_FRAME;i++){
        if(as->page_table[i]->dirty==0) 
            return 1;
    }
    return 0;
}

paddr_t page_replacement(vaddr_t vaddr, unsigned int segment_type){
    struct addrspace *as = proc_getas();
    int victim_index, tlb_index;
    //Search for a free page table entry
    for(int i = 0; i < N_FRAME; i++){
        if(as->page_table[i]->free==1){ //free=1 -> free and frees=0 -> not free
            as->page_table[i]->vaddr = vaddr;
            as->page_table[i]->free = 0;
            as->page_table[i]->dirty = 0;
            as->page_table[i]->segtype = segment_type;
            return as->page_table[i]->paddr;
        }
    }
    /* Page replacement */

    //First let's check if there is at least one page in pt with dirty bit clear
   bool any_ndirty = check_if_any_dirtyzero();

    if(any_ndirty==1){ //there is at least one page with dirty bit clear (=0)
        for(;;){
            victim_index=page_victim();
            if(as->page_table[victim_index]->dirty==0)
                break;
            else continue;
        }
    }
    else { //there are not some page with dirty bit clear.. every dirty bit in pt is set
        for(;;){
            victim_index=page_victim();
            if(as->page_table[victim_index]->segtype!=0)
                break;
            else continue;
        }
    }


    if(as->page_table[victim_index]->dirty==1 || (/*as->page_table[victim_index]->dirty==0 &&*/ as->page_table[victim_index]->segtype!=0)) {
        swap_set(swap_out(as->page_table[victim_index]->paddr,as->page_table[victim_index]->vaddr),as->page_table[victim_index]->vaddr,as);
    }  

        /* Lookup TLB and remove if present */
    tlb_index = TLB_retrieve_index(as->page_table[victim_index]->vaddr);
    if(tlb_index >= 0)
        TLB_invalidate_entry(tlb_index); //remove entry
    
    /* Set page table entry */
    as->page_table[victim_index]->free = 0;
    as->page_table[victim_index]->vaddr = vaddr;
    as->page_table[victim_index]->dirty = 0;
    as->page_table[victim_index]->segtype = segment_type;
    return as->page_table[victim_index]->paddr;
    
}
