/***************************************************************************** 
* 
* File Name : wm_mem.h
* 
* Description: memory manager Module 
* 
* Copyright (c) 2014 Winner Micro Electronic Design Co., Ltd. 
* All rights reserved. 
* 
* Author : dave
* 
* Date : 2014-6-12 
*****************************************************************************/ 


#ifndef WM_MEM_H
#define WM_MEM_H


#include "wm_type_def.h"

#if DEBUG

#include "list.h"

#define  MEM_HEADER_PATTERN          0x76028412
#define  MEM_TAILER_PATTERN          0x83395627
#define  MEM_FREED_PATTERN           0x82962503

extern u32 alloc_heap_mem_bytes; 
extern u32 alloc_heap_mem_blk_cnt;
extern u32 alloc_heap_mem_max_size;

//
// Note: it's important that the size of MP_MEMORY_BLOCK structure
//       be multiple of 16 bytes.
//
typedef struct _MEMORY_BLOCK {
    /** Pointer to next and previous blocks */
    struct dl_list  list;
    /** name of the file which is doing the allocation */
    char      *file;
    /** pad to make the size of whole structure multiple of 16 bytes */
    u32      pad;
    /** line number where allocated */
    u32       line;
    /** ulong index of trailer (=(length/4)-1 relative to data start */
    u32       length;
    /** To help detect underflows. A trailer is also added to find overflows */
    u32       header_pattern;
} MEMORY_BLOCK, *PMEMORY_BLOCK;

void tls_mem_free(void *p);
void *mem_alloc_debug(u32 size, char* file, int line);
void mem_free_allocated_blocks(void);
#define tls_mem_alloc(size)   mem_alloc_debug(size, __FILE__, __LINE__)

void tls_mem_alloc_info(void);


#else /* DEBUG */

#define tls_mem_alloc(size)   malloc(size)
#define tls_mem_free       free


#endif /* DEBUG */

#endif /* TLS_MEM_H */

