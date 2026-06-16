#ifndef VMSTATS_H
#define VMSTATS_H

void TLB_faults(void);
void TLB_faults_free(void);
void TLB_faults_replace(void);
void TLB_invalidation(void);
void TLB_reloads(void);
void page_faults_zeroed(void);
void page_faults_disk(void);
void page_faults_elf(void);
void page_faults_swapfile(void);
void page_faults_writes(void);

void print_stats(void);


#endif