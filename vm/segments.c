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
#include <segments.h>
#include <vmstats.h>
#include <uio.h>
#include <vnode.h>


int page_loading(vaddr_t faultaddr,paddr_t paddr, int segment_type){
    
    struct addrspace *as = proc_getas();
    struct vnode *v = as->v;
    
    off_t offset_segment;
    vaddr_t vaddr_first_segment;
    size_t size_segment;

    switch (segment_type){

        case 0:
        offset_segment =  as->offset_text_elf; //OFFSET OF STARTING TEXT SEGMENT IN ELF FILE
        vaddr_first_segment = as->as_first_vaddr1; //START VADDR OF TEXT SEGMENT
        size_segment =  as->text_size; //SIZE OF TEXT SEGMENT INTO ELF FILE
         break;
         
        case 1:
        offset_segment =  as->offset_data_elf; //OFFSET OF STARTING DATA SEGMENT IN ELF FILE
        vaddr_first_segment = as->as_first_vaddr2; //START VADDR OF DATA SEGMENT
        size_segment =  as->data_size; //SIZE OF DATA SEGMENT INTO ELF FILE
         break;

        default : 
        offset_segment =  0; //OFFSET OF STARTING DATA SEGMENT IN ELF FILE
        vaddr_first_segment = 0; //START VADDR OF DATA SEGMENT
        size_segment =  0; //SIZE OF DATA SEGMENT INTO ELF FILE
        
        break;

    }
    
    off_t offset;
    size_t size;
    /* Since the first virtual address of the segment could be not page-aligned, we have to consider this scenario */
    
    //The fault address belongs to the first page of segment in physical memory
    if((faultaddr & PAGE_FRAME) == (vaddr_first_segment & PAGE_FRAME)){
        size = (PAGE_SIZE - (vaddr_first_segment &~PAGE_FRAME)) > size_segment ? 
                PAGE_SIZE -(vaddr_first_segment &~PAGE_FRAME) : size_segment;
        offset = offset_segment;
        
    }

    /* It is possible that segment size is not a multiple of PAGE SIZE so internal fragmentation can occurs */

    //The fault address belongs to the last page of segment in physical memory
    else if((faultaddr & PAGE_FRAME) == ((vaddr_first_segment+size_segment) & PAGE_FRAME)){
        size = (vaddr_first_segment+size_segment) & ~PAGE_FRAME;
        offset = offset_segment + (faultaddr & PAGE_FRAME) - vaddr_first_segment;
    }

    else {
        size = PAGE_SIZE;
        offset = offset_segment + (faultaddr & PAGE_FRAME) - vaddr_first_segment;
    }
    
    int result;
    result=load_page(v,offset,paddr,size);
    if (result) return result;
    /*PAGE FAULT FROM DISK*/
    page_faults_disk();
    return 0;
}

int load_page(struct vnode *v, off_t offset, paddr_t paddr, size_t size){
    struct iovec iov;
    struct uio u;
    int result;

    uio_kinit(&iov,&u,(void *)PADDR_TO_KVADDR(paddr),size,offset,UIO_READ);
    result=VOP_READ(v,&u);
    if(result) return result;
    return 0;
}