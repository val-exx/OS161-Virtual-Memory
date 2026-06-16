#include <types.h>

#ifndef SEGMENTS_H
#define SEGMENTS_H


int page_loading(vaddr_t faultaddr, paddr_t paddr, int segment_type);
int load_page(struct vnode *v, off_t offset, paddr_t paddr, size_t size);

#endif