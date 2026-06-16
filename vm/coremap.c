#include <types.h>
#include <lib.h>
#include <vm.h>
#include <mainbus.h>
#include <coremap.h>
#include <swapfile.h>
#include <spinlock.h>
#include <addrspace.h>
#include <proc.h>
#include <pt.h>


vaddr_t firstfree;   /* first free virtual address; set by start.S */
struct spinlock cm_spinlock = SPINLOCK_INITIALIZER;
static int    frame_slots = 0; /* number of ram frames */
static struct   coremap_entry *coremap;
static int        victim_index = 0; //index used to find the space for the page that we have to swap out

static int  coremap_swapout(int npages);
static int  coremap_get_victim();
static int  coremap_find_freeframes(int npages);

/*
 * Called very early in system boot to figure out how much physical
 * RAM is available.
 */
void
coremap_bootstrap(void)
{
  static paddr_t firstpaddr;  /* address of first free physical page */
  static paddr_t lastpaddr;   /* one past end of last free physical page */
	size_t coremap_bytes;  /* number of bytes of coremap               */
  int coremap_pages;    /* number of coremap pages                  */
  int kernel_pages;     /* number of kernel pages                   */
  int i;

	/* Get size of RAM. */
	lastpaddr = mainbus_ramsize();
  
	/*
	 * This is the same as the last physical address, as long as
	 * we have less than 512 megabytes of memory. If we had more,
	 * we wouldn't be able to access it all through kseg0 and
	 * everything would get a lot more complicated. This is not a
	 * case we are going to worry about.
	 */
	if (lastpaddr > 512*1024*1024) {
		lastpaddr = 512*1024*1024;
	}




	/*
	 * Get first free virtual address from where start.S saved it.
	 * Convert to physical address.
	 */

	firstpaddr = firstfree - MIPS_KSEG0;
 

	kprintf("%uk physical memory available\n",
		(lastpaddr-firstpaddr)/1024);


  frame_slots= lastpaddr / PAGE_SIZE;

  /* Allocates the coremap right in the firstfree address. */
  coremap = (struct coremap_entry *)firstfree;

 /*Computing the total number of pages related
  to coremap and kernel space*/

 coremap_bytes = sizeof(struct coremap_entry) * frame_slots; 
 coremap_pages = DIVROUNDUP(coremap_bytes, PAGE_SIZE);
 kernel_pages = firstpaddr / PAGE_SIZE;

  /*  Coremap vector initialization  */
  for (i = 0; i < frame_slots; i++)
  {
    coremap[i].busy = 0;
    coremap[i].chunck_size = 0;
    coremap[i].cm_lock = 0;
    coremap[i].cm_ptentry = NULL;
  }
 
/*Initialization for the part of the coremap referred to the
*kernel
*/
  for (i = 0; i < kernel_pages+coremap_pages; i++)
  {
    coremap[i].busy= 1;
    coremap[i].chunck_size = 1;

  }
}


/*Function used to find and return the index of the first free 
n frames inside the ram if no free frame is founded it returns -1*/
static int
coremap_find_freeframes(int npages)
{
  int i=0;
  int cnt=0;  
  int start_frame=0;
  
  for(i=0 ; i < frame_slots; i++ ){
    
    if(coremap[i].busy == 0){
       cnt++;
        if(cnt==npages){
          start_frame=i-npages+1;
          return start_frame;
        }
    }else{
      cnt=0;
    } 
  }

  if(i == frame_slots ){
    start_frame=-1;
  }
  return start_frame;
}

/*Funtion used to find space inside the memory if no free frames are aviable, 
then apply the swap out of the n pages needed, cleaning them,*/
paddr_t
coremap_getppages(unsigned npages, struct PTE *ptentry)
{
  int i;
  int start_frame;

  spinlock_acquire(&cm_spinlock);
  start_frame=coremap_find_freeframes(npages);
  
    if( start_frame ==-1 ){ //no free pages founded, so swap out
      start_frame=coremap_swapout(npages);
      if (start_frame == -1){
      spinlock_release(&cm_spinlock);
      return 0;
      }
    }
  //cleaning of the aviable pages
  bzero((void *)PADDR_TO_KVADDR(start_frame * PAGE_SIZE), PAGE_SIZE * npages);

  //updating the coremap data structure
  coremap[start_frame].chunck_size = npages;
  for( i=0; i<(int)npages; i++){

      coremap[start_frame + i].busy = 1;
      coremap[start_frame + i].cm_ptentry =ptentry;
  }
  spinlock_release(&cm_spinlock);
  return start_frame * PAGE_SIZE;
}



/*Function used to find a "victim" that have to be moved in the swap file.
It just check that the particular page is not a kernel or a code page.*/
static int
coremap_get_victim()
{
  int i;

  for(i=0; i<frame_slots; i++)
  {
    victim_index = (victim_index + 1) % frame_slots;

    /* Swap out only user pages */
    if(coremap[victim_index].cm_ptentry != NULL && !coremap[victim_index].cm_lock)
    {
      KASSERT(coremap[victim_index].busy == 1);
      KASSERT(coremap[victim_index].chunck_size);

      return victim_index;
    }
  }

  return -1;
}



static int
coremap_swapout(int npages)
{
  int swap_index;

  if(npages > 1)
  {
    panic("Cannot swap out multiple pages");
  }
 //find the victim to swap
  swap_index = coremap_get_victim(npages);
  if(swap_index == -1)
  {
    panic("Cannot find swappable victim");
  }

  //cleaning of the previous page inside the page table
  coremap[swap_index].cm_ptentry->vaddr=0;
  coremap[swap_index].cm_ptentry->paddr=0;
  coremap[swap_index].cm_ptentry->free=1;
  coremap[swap_index].cm_ptentry->segtype=3;
  TLB_invalidate_entry(swap_index*PAGE_SIZE);

  /**  
   * cm_lock = 1 in order to prevent the frame to be selected
   * as a victim for another concurrent swap out.
   */
  coremap[swap_index].cm_lock = 1;
  spinlock_release(&cm_spinlock);
  swap_index = swap_out(swap_index * PAGE_SIZE,1);
  spinlock_acquire(&cm_spinlock);
  coremap[swap_index].cm_lock = 0;
 
 //return the swap position
  return swap_index;
}




//Clean the state of the frames related to the victim_address

void coremap_freeppages(paddr_t victim_addr)
{
 int Frame_number;
 int busy_pages;
 int i;

 KASSERT(victim_addr % PAGE_SIZE == 0);
 //obtainig the frame index from the paddr
 Frame_number=victim_addr/PAGE_SIZE;

 KASSERT(frame_slots > Frame_number);

//check how many pages are related to that address
 busy_pages=coremap[Frame_number].chunck_size;
 
 //starting from the frame number clean all the busy 
 //pages related to it.
 if(busy_pages > 1){

  spinlock_acquire(&cm_spinlock);

        for(i=0; i<busy_pages; i++){
              coremap[Frame_number + i].busy=0;
        }     
  spinlock_release(&cm_spinlock);                    
 }else {
    coremap[Frame_number].busy=0;;
 }

}

