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
#include <syscall.h>
#include <pt.h>
#include <vm.h>
#include <vm_tlb.h>
#include "syscall.h"
#include <swapfile.h>
#include <vmstats.h>
#include <mips/vm.h>
#include <segments.h>
#include <coremap.h>
#include <mips/tlb.h>
#include "opt-dumbvm.h"
#include "opt-PAGING.h"

#define STACK_PAGES    18

#if OPT_DUMBVM
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;
#endif

#if OPT_PAGING
paddr_t getppages(unsigned npages, struct PTE *ptentry);
#elif OPT_DUMBVM
paddr_t getppages(unsigned npages);
#endif

void
vm_bootstrap(void)
{
	//si può richiamare il swap bootstrap -> in esecuzione condizionale
	#if !OPT_DUMBVM
	swap_bootstrap();
	#endif
}

// void vm_can_sleep(void){
// 		if (CURCPU_EXISTS()) {
// 		/* must not hold spinlocks */
// 		KASSERT(curcpu->c_spinlocks == 0);

// 		/* must not be in an interrupt handler */
// 		KASSERT(curthread->t_in_interrupt == 0);
// 	}
// }

vaddr_t alloc_kpages(unsigned npages){
    paddr_t paddr;

    //vm_can_sleep();
    #if OPT_PAGING
    paddr = getppages(npages, NULL);
    #else
    pa = getppages(npages);
    if (pa==0) {
        return 0;
    }
    #endif
    return PADDR_TO_KVADDR(paddr);
}

void free_kpages(vaddr_t addr){
	#if OPT_PAGING
	paddr_t paddr;
	paddr = PADDR_TO_KVADDR(addr);
	freeppages(paddr);
	#elif OPT_DUMBVM
	(void)addr
	#endif
}
#if OPT_PAGING
paddr_t getppages(unsigned npages, struct PTE *ptentry){
	paddr_t paddr;
	paddr = coremap_getppages(npages, ptentry);
	if(paddr==0) panic("Out of memory");
	return paddr;
}
#elif OPT_DUMBVM
paddr_t getppages(unsigned long npages)
{
	paddr_t addr;

	spinlock_acquire(&stealmem_lock);

	addr = ram_stealmem(npages);

	spinlock_release(&stealmem_lock);
	return addr;
}
#endif

void
freeppages(paddr_t paddr)
{
	#if OPT_PAGING
	coremap_freeppages(paddr);
	#endif
}

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("vm tried to do tlb shootdown?!\n");
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	vaddr_t vbase1, vtop1, vbase2, vfirst1, vtop2, vfirst2, stackbase, stacktop;
	paddr_t paddr;
	vaddr_t basefaultaddress;
	struct addrspace *as;
    bool ReadOnly;
	

	int segment_type;
	/*segment_type=0 -> CODE
	segment_type=1 -> DATA
	segment_type=2 -> STACK*/

	basefaultaddress = faultaddress & PAGE_FRAME;
	
	DEBUG(DB_VM, "vm: fault: 0x%x\n", faultaddress);
	
	switch (faulttype) {
	    case VM_FAULT_READONLY:
		/* We always create pages read-write, so we can't get this */
		sys__exit(-1);
	    case VM_FAULT_READ:
	    case VM_FAULT_WRITE:
		break;
	    default:
		return EINVAL;
	}

	

	if (curproc == NULL) { 
		/*
		 * No process. This is probably a kernel fault early
		 * in boot. Return EFAULT so as to panic instead of
		 * getting into an infinite faulting loop.
		 */
		return EFAULT;
	}

	as = proc_getas();
	if (as == NULL) { ;
		/*
		 * No address space set up. This is probably also a
		 * kernel fault early in boot.
		 */
		return EFAULT;
	}

	/* Assert that the address space has been set up properly. */
	
	KASSERT(as->as_npages1 != 0);
	KASSERT(as->as_npages2 != 0);
	

	#if OPT-PAGING
	KASSERT(as->as_first_vaddr1 != 0);
	KASSERT(as->as_last_vaddr1 != 0);
	KASSERT(as->as_first_vaddr2 != 0);
	KASSERT(as->as_last_vaddr2 != 0);

	#endif

	#if OPT-DUMBVM
	KASSERT(as->as_vbase1 != 0);
	KASSERT(as->as_pbase1 != 0);
	KASSERT(as->as_pbase2 != 0);
	KASSERT(as->as_vbase2 != 0);
	KASSERT((as->as_pbase1 & PAGE_FRAME) == as->as_pbase1);
	KASSERT((as->as_pbase2 & PAGE_FRAME) == as->as_pbase2);
	#endif


	vfirst1 = as->as_first_vaddr1;
	vtop1 = as->as_last_vaddr1;
	vfirst2 = as->as_first_vaddr2;
	vtop2 = as->as_last_vaddr2;
	stackbase = USERSTACK - STACK_PAGES * PAGE_SIZE;
	stacktop = USERSTACK;

	if (faultaddress >= vfirst1 && faultaddress < vtop1) {
		#if OPT_PAGING
		(void) vbase1;
		segment_type=0; //CODE
		ReadOnly=1;
		#elif OPT_DUMBVM
		vbase1 = as->as_vbase1;
		paddr = (faultaddress - vbase1) + as->as_pbase1;
		#endif
	}
	else if (faultaddress >= vfirst2 && faultaddress < vtop2) {
		#if OPT_PAGING
		(void) vbase2;
		segment_type=1; //DATA
		ReadOnly=0;
		#elif OPT_DUMBVM
		vbase2 = as->as_vbase2;
		paddr = (faultaddress - vbase2) + as->as_pbase2;
		#endif
	}
	else if (faultaddress >= stackbase && faultaddress < stacktop) {
		#if OPT_PAGING
		segment_type=2; //STACK
		ReadOnly=0;
		#elif OPT_DUMBVM
		paddr = (faultaddress - stackbase) + as->as_stackpbase;
		#endif
	}
	else {
		kprintf("Fault address doesn't belog to any segment\n");
		return EFAULT;
	}

	#if OPT_PAGING

	/*TLB MISS OCCURED*/
	TLB_faults();

	//Check if faultaddress is in Page Table
	paddr=page_in_pt(basefaultaddress);


	if(paddr==0){ //the page isn't in page table 

		/*In page_replacement a new entry is inserted. As first thing checks if
		there is some free page in pt. If not, the page replacement is applied.*/
		paddr_t new_paddr = page_replacement(basefaultaddress,segment_type);
		/*In page_replacement the dirty bit is initially set to 0.
		This is important because if the fault address is not present in swap file and
		the segment type is STACK, we have inserted a useless page in PT which doesn't 
		contain nothing. In order to solve that, if dirty bit is clear, it will have the priority
		to be selected as page victim.*/

		if(swap_in(basefaultaddress, new_paddr)==-1) { //Is there fault address in swap file?
		
		//if fault address is not present in swap file so it is needed to load from elf file
			if(segment_type==0 || segment_type==1) {
				int result;
				result = page_loading(faultaddress,new_paddr,segment_type);
				if(result){
					kprintf("Page loading error\n");
					return result;
				}
				/*PAGE FAULTS WHICH REQUIRE TO GET A PAGE FROM ELF FILE*/
				page_faults_elf();

				int res=set_dirty_bit_pt(1,new_paddr);

				if(!res) panic("Error setting dirty bit\n");

			}
		

			else { //the segment is stack type so it's not possible loading requested page from elf file..
				/*TLB MISS FOR WHICH IS REQUIRED TO ZERO-FILLED A NEW PAGE*/
				page_faults_zeroed();
			}
		
		} 

		
		else {//the fault address is present in swap file
			int res=set_dirty_bit_pt(0,new_paddr);
			if(!res) panic("Error setting dirty bit\n");
			
		}

		//insert the new entry into TLB
		TLB_insert_entry(basefaultaddress, new_paddr, ReadOnly);
    

	}
	
	else { //the page is in page table so a reload into TLB is needed
		/* make sure it's page-aligned */
		KASSERT((paddr & PAGE_FRAME) == paddr);
		TLB_insert_entry(basefaultaddress, paddr, ReadOnly);

		/*TLB MISS FOR PAGES THAT WERE ALREADY IN MEMORY*/
		TLB_reloads();
	}
	return 0;

	#elif OPT_DUMBVM
	uint32_t ehi, elo;
	int spl;
		KASSERT((paddr & PAGE_FRAME) == paddr);

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (int i=0; i<NUM_TLB; i++) {
		tlb_read(&ehi, &elo, i);
		if (elo & TLBLO_VALID) {
			continue;
		}
		ehi = faultaddress;
		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
		DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
		tlb_write(ehi, elo, i);
		splx(spl);
		return 0;
	}

	kprintf("dumbvm: Ran out of TLB entries - cannot handle page fault\n");
	splx(spl);
	return EFAULT;
	#endif
	
}