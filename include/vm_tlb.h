#ifndef VM_TLB_H
#define VM_TLB_H

#define NUM_TLB 64
/* TLB functions */

int TLB_invalidate_entry(paddr_t paddr);
int TLB_invalidate_all(void);
int TLB_insert_entry(vaddr_t faultaddress, paddr_t paddr, bool ReadOnly);
int TLB_retrieve_index(vaddr_t vaddr);


#endif