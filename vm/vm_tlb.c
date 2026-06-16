/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

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
#include <vmstats.h>
#include <swapfile.h>
#include <coremap.h>
#include <segments.h>
#include <pt.h>
#include <mips/tlb.h>


#define STACKPAGES    18
static int tlb_get_rr_victim(void);
void TLB_faults_with_free();

static int tlb_get_rr_victim(void){
	int victim;
	static unsigned int next_victim=0;
	victim=next_victim;
	next_victim=(next_victim+1)%NUM_TLB;
	return victim;
}

/*Remove entry in TLB by invalidating that entry*/
int TLB_invalidate_entry(paddr_t paddr){

    uint32_t ehi,elo,i;
    for (i=0; i<NUM_TLB; i++) 
    {
        tlb_read(&ehi, &elo, i);
        if ((elo & 0xfffff000) == (paddr & 0xfffff000))	
        {
            tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);		
        }
    }

    return 0;
}

int TLB_retrieve_index(vaddr_t vaddr){
	return tlb_probe(vaddr,0);
}

int TLB_invalidate_all(void){
	int i;
	int spl;

	spl=splhigh();
	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}
	splx(spl);
	/*TLB INVALIDATION*/
	TLB_invalidation();

	return 0;
}

int TLB_insert_entry(vaddr_t faultaddress, paddr_t paddr, bool ReadOnly){
	
	int i;
	uint32_t ehi, elo;
	int spl;
	
	/*Disable interrupts on this CPU while frobbing the TLB. */
	spl=splhigh();
	
	// Look for an invalid entry on the TLB
	for (i=0; i<NUM_TLB; i++) {
		tlb_read(&ehi, &elo, i);
		if (elo & TLBLO_VALID) {
			continue;
		} 
		ehi = faultaddress;
		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
		if(ReadOnly==1) elo = elo & ~TLBLO_DIRTY;
		tlb_write(ehi, elo, i);
		splx(spl); // Leave that to calling function

		/*TLB FAULTS FOR WHICH THERE WAS FREE SPACE IN TLB*/
		TLB_faults_free();
		return 0;
	}
	
	
	// No invalid entries so let's apply TLB replacement
	ehi = faultaddress;
	elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
	if(ReadOnly==1) elo = elo & ~TLBLO_DIRTY;
	tlb_write(ehi, elo, tlb_get_rr_victim());
	splx(spl);
	/*TLB FAULTS FOR WHICH THERE WAS NO FREE SPACE FOR THE NEW TLB ENTRY SO REPLACEMENT WAS REQUIRED*/
	TLB_faults_replace();	             	
	return 0;
}





