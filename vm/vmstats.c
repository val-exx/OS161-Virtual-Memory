#include <vmstats.h>
#include <lib.h>

static int cnt_tlb_faults=0;
static int cnt_tlb_faults_with_free=0;
static int cnt_tlb_faults_with_replace=0;
static int cnt_tlb_invalidations=0;
static int cnt_tlb_reloads=0;
static int cnt_page_faults_zeroed=0;
static int cnt_page_faults_disk=0;
static int cnt_page_faults_elf=0;
static int cnt_page_faults_swap=0;
static int cnt_swap_writes=0;

void TLB_faults(){
    cnt_tlb_faults++;
}
void TLB_faults_free(){
    cnt_tlb_faults_with_free++;
}
void TLB_faults_replace(){
    cnt_tlb_faults_with_replace++;
}
void TLB_invalidation(){
    cnt_tlb_invalidations++;
}
void TLB_reloads(){
    cnt_tlb_reloads++;
}


void page_faults_zeroed(){

    cnt_page_faults_zeroed++;
}

void page_faults_disk(){

    cnt_page_faults_disk++;
}

void page_faults_elf(){

    cnt_page_faults_elf++;
}

void page_faults_swapfile(){

    cnt_page_faults_swap++;

}
void page_faults_writes(){

    cnt_swap_writes++;

}

void print_stats(void){

    kprintf("TLB faults: %d \nTLB faults with free: %d \nTLB faults with replace: %d \nTLB invalidation: %d \nTLB reloads: %d\n",cnt_tlb_faults,cnt_tlb_faults_with_free,cnt_tlb_faults_with_replace,
             cnt_tlb_invalidations,cnt_tlb_reloads);


    kprintf("Page faults zeroed: %d\nPage faults disk: %d\nPage faults elf: %d \nPage faults swap: %d \nSwap writes: %d\n",cnt_page_faults_zeroed,cnt_page_faults_disk,cnt_page_faults_elf,
             cnt_page_faults_swap,cnt_swap_writes);

    KASSERT(cnt_page_faults_disk == cnt_page_faults_elf+cnt_page_faults_swap); 
    KASSERT(cnt_tlb_faults == cnt_tlb_faults_with_free+cnt_tlb_faults_with_replace);
    KASSERT(cnt_tlb_faults == cnt_tlb_reloads+cnt_page_faults_disk+cnt_page_faults_zeroed);
}