#include <vfs.h>
#include <types.h>
#include <spinlock.h>
#include <lib.h>
#include <uio.h>
#include <addrspace.h>
#include <vnode.h>
#include <swapfile.h>
#include <kern/fcntl.h>
#include <vmstats.h>
#include <vm.h>



static struct spinlock swaplock = SPINLOCK_INITIALIZER; //lock for the swapfile
static struct vnode *swapfile;
static int Swap_slots= (SWAP_SIZE / PAGE_SIZE); //number of slots inside the swap file
static struct swap_map *swap_table[ SWAP_SIZE / PAGE_SIZE ]; // bitmap for the swap file

/**
 * creation of the swap file
 */

void swap_bootstrap(void)
{
    int swap_flag;
    char swap_name[] = SWAPFILE_NAME;
   

    KASSERT(SWAP_SIZE % PAGE_SIZE == 0);
    
    //opening the swap file
    swap_flag = vfs_open(swap_name, O_RDWR | O_CREAT | O_TRUNC, 0, &swapfile);
    KASSERT(swapfile != NULL);
    if (swap_flag)
    {
        panic("Swap:Error opening SwapFile\n");
    }

    //allocating the space for the swap_table
    for(int i=0;i<(Swap_slots);i++){
        struct swap_map *swap_t = kmalloc(sizeof(struct swap_map));
        swap_table[i]=swap_t;

    }
    
    //swap_table initialization
    swap_free(NULL,NULL,2);


}


/**
 *Destructor for the swapfile.
 * 
 */
void swap_destroy(void) //called in function shutdown() (main.c)
{
    kfree(*swap_table);
    vfs_close(swapfile); 
}
//Frees the swap table with 3 configurations: conf 0 search the as
                                              //conf 1 search the vadd
                                              //conf 2 clear all the structure 
                                              //for the initialization
void swap_free(struct addrspace *as, vaddr_t *vadd, int conf )
{
    int i;

    if(conf==0){
        spinlock_acquire(&swaplock);
        for(i = 0; i < ( Swap_slots ) ; i++){
            if(swap_table[i]->as == as){
            swap_table[i]->free = 1;
            swap_table[i]->as = NULL;
            swap_table[i]->vaddr = 0;
            }
        }
         spinlock_release(&swaplock);

    }else if(conf==1){
        spinlock_acquire(&swaplock);
    for(i = 0; i < ( Swap_slots ) ; i++){
            if(swap_table[i]->vaddr == *vadd){
            swap_table[i]->free = 1;
            swap_table[i]->as = NULL;
            swap_table[i]->vaddr = 0;
            }
        }
         spinlock_release(&swaplock);

    }else if(conf == 2){
        spinlock_acquire(&swaplock);
     for(i = 0; i <  Swap_slots ; i++){
            swap_table[i]->free = 1;
            swap_table[i]->as = NULL;
            swap_table[i]->vaddr = 0;
        }
         spinlock_release(&swaplock);
    }
}


/*Search inside the swap table for a particular vaddr. 
Return its index if it is founded, otherwise return -1*/
int swap_tracker(vaddr_t vaddr){

    int i;
    for(i = 0; i < Swap_slots; i++){
        if(vaddr == swap_table[i]->vaddr){
            return i;
            }
    }
    return -1;

} 

/*
 *    vop_read        - Read data from file to uio, at offset specified
 *                      in the uio, updating uio_resid to reflect the
 *                      amount read, and updating uio_offset to match.
 *                      Not allowed on directories or symlinks.
 */



/*
* funtion used to move a frame from the swap file to the memory
*search a particula vaddr and move it the new_paddr position
*/
int swap_in(vaddr_t vadd, paddr_t new_paddr)
{
    int err_f;
    off_t read_add; 
    struct iovec iov;
 	struct uio myuio;
    int swap_position;
    struct addrspace *as;
    bool free;
    vaddr_t vaddr;

    //search the swap position
    swap_position=swap_tracker(vadd);

    if(swap_position== -1){
        return -1;
    }

    page_faults_disk();
    
    read_add =swap_position * PAGE_SIZE;
    as=swap_table[swap_position]->as;
    free=swap_table[swap_position]->free;
    vaddr=swap_table[swap_position]->vaddr;

    uio_kinit(&iov, &myuio,(void *)PADDR_TO_KVADDR(new_paddr), PAGE_SIZE, read_add, UIO_READ);
    err_f = VOP_READ(swapfile, &myuio);
    if(err_f)
    {
        panic("SWAP:Error in swap_in\n");
        
    }
    if (myuio.uio_resid != 0) {
		panic("SWAP: short read on page");
        
	}

    swap_table[swap_position]->as=as;
    swap_table[swap_position]->free=free;
    swap_table[swap_position]->vaddr=vaddr;

    /*PAGE FAULTS THAT REQUIRE TO GET A PAGE FROM SWAP FILE*/
    page_faults_swapfile();
    return 0;

}


/*Find a free position inside the swap_table that is
used to allocate a new data inside the swap file*/
int swap_alloc(void){
        for(int i=0; i<Swap_slots;i++){
            
            if(swap_table[i]->free == 1){
               swap_table[i]->free = 0;
                return i;
                }

        }
        return -1;
}



/*Function used when a new data is introduced inside the swap file.*/
void swap_set(int index, vaddr_t vaddr,struct addrspace *as){
        swap_table[index]->vaddr=vaddr;
        swap_table[index]->as=as;
}


/*Funtion used to move a data from ram to the disk*/
int swap_out(paddr_t paddr,vaddr_t vaddr)
{   

    int err_f;
    int swap_index;
    off_t write_add; 
    struct iovec iov;
 	struct uio myuio;
 
    if(swap_tracker(vaddr)>=0){
        err_f=swap_tracker(vaddr);
    }
    else{
        spinlock_acquire(&swaplock);
        /*Searching for a free position inside the swap file*/
        err_f=swap_alloc();
        spinlock_release(&swaplock);
    }
        if (err_f== -1 ) 
        {
            panic("Swap:Out of swap space\n");
        }
        
        swap_index=err_f;
        
        write_add= swap_index*PAGE_SIZE;

        uio_kinit(&iov, &myuio, (void *)PADDR_TO_KVADDR(paddr), PAGE_SIZE, write_add, UIO_WRITE);
        err_f = VOP_WRITE(swapfile, &myuio);
        if (err_f)
        {
            panic("SWAP:Error in swap_in\n");
        }

        /*PAGE FAULTS WHICH REQUIRE TO WRITE A PAGE INTO THE SWAP FILE*/
        page_faults_writes();
        return swap_index;
}