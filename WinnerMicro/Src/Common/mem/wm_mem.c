/***************************************************************************** 
* 
* File Name : wm_mem.c
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

#include "wm_osal.h"
#include "wm_mem.h"
#include "list.h"


#if DEBUG

/**
 * This mutex is used to synchronize the list of allocated
 * memory blocks. This is a debug version only feature
 */
tls_os_sem_t    *mem_sem;

/**
 * This is a list of all the memory allocations that were made by the driver.
 * This is a debug version only feature.
 */
struct dl_list  memory_list;

/**
 * This variable is set if the memory mananger has been initialized.
 * This is available only for debug version of the driver
 */
bool         memory_manager_initialized = false;

u32 alloc_heap_mem_bytes = 0; 
u32 alloc_heap_mem_blk_cnt = 0;
u32 alloc_heap_mem_max_size = 0;

/**
 * This is a debug only function that performs memory management operations for us.
 * Memory allocated using this function is tracked, flagged when leaked, and caught for
 * overflows and underflows.
 *
 * \param size            The size in bytes of memory to
 *        allocate
 *
 * \param file            The full path of file where this
 *        function is invoked from
 * \param line            The line number in the file where this
 *        method was called from
 * \return Pointer to the allocated memory or NULL in case of a failure
 */
void * mem_alloc_debug(u32 size, char* file, int line)
{
    void *buf = NULL;
    u32 pad_len;

    //
    // If the memory manager has not been initialized, do so now
    //
    if (!memory_manager_initialized) {
        //
        // NOTE: If two thread allocate the very first allocation simultaneously
        // it could cause double initialization of the memory manager. This is a
        // highly unlikely scenario and will occur in debug versions only.
        //
        tls_os_sem_create(&mem_sem, 1);
        dl_list_init(&memory_list);
        memory_manager_initialized = true;
    }

    //
    // Allocate the required memory chunk plus header and trailer bytes
    //
    pad_len = sizeof(u32) - (size & 0x3);
    buf = malloc(size + sizeof(MEMORY_BLOCK) + sizeof(u32) + pad_len);

    if (buf) {
        //
        // Memory allocation succeeded. Add information about the allocated
        // block in the list that tracks all allocations.
        //
        PMEMORY_BLOCK  mem_blk_hd;

        // Fill in the memory header and trailer
        mem_blk_hd = (PMEMORY_BLOCK) buf;
        mem_blk_hd->pad = pad_len;
        mem_blk_hd->file = file;
        mem_blk_hd->line = line;
        mem_blk_hd->length = size;
        mem_blk_hd->header_pattern = MEM_HEADER_PATTERN;
        *((u32 __packed *) (((u8 *)(buf))+size + sizeof(MEMORY_BLOCK)+pad_len)) = MEM_TAILER_PATTERN;

        // Jump ahead by memory header so pointer returned to caller points at the right place
        buf = ((u8 *)buf) + sizeof (MEMORY_BLOCK);

#if 0

        printf("==>Memory was allocated from %s at line %d with length %d\n",
                  mem_blk_hd->file,
                  mem_blk_hd->line,               
                  mem_blk_hd->length);
        printf("==>mem alloc ptr = 0x%x\n", buf);

#endif

        // Store a reference to this block in the list
        tls_os_sem_acquire(mem_sem, 0);
        dl_list_add_tail(&memory_list, &mem_blk_hd->list);
        alloc_heap_mem_bytes += size+sizeof(MEMORY_BLOCK)+sizeof(u32)+pad_len;
        alloc_heap_mem_blk_cnt++;
        if (alloc_heap_mem_bytes > alloc_heap_mem_max_size)
            alloc_heap_mem_max_size = alloc_heap_mem_bytes;
        tls_os_sem_release(mem_sem);
    }

    return buf;
}

/**
 * This routine is called to free memory which was previously allocated using MpAllocateMemory function.
 * Before freeing the memory, this function checks and makes sure that no overflow or underflows have
 * happened and will also try to detect multiple frees of the same memory chunk.
 *
 * \param p    Pointer to allocated memory
 */
void tls_mem_free(void *p)
{
    PMEMORY_BLOCK  mem_blk_hd;
    u32            value;

    // Jump back by memory header size so we can get to the header
    mem_blk_hd = (PMEMORY_BLOCK) (((u8 *)p) - sizeof(MEMORY_BLOCK));

    //
    // Check that header was not corrupted
    //
    if (mem_blk_hd->header_pattern != MEM_HEADER_PATTERN) {
        if (mem_blk_hd->header_pattern == MEM_FREED_PATTERN) {
            printf("Possible double free of memory block at %p\n",
                    mem_blk_hd);
        }
        else {
            printf("Memory corruption due to "
                    "underflow detected at memory block %p\n",
                    mem_blk_hd);
        }

        printf("Dumping information about memory block. "
                "This information may itself have been "
                "corrupted and could cause machine to bugcheck.\n");
        printf("Memory was allocated from %s at line %d with length %d\n",
                mem_blk_hd->file,
                mem_blk_hd->line,
                mem_blk_hd->length);
    }

    //
    // Now corrupt the header so that double frees will fail.
    // Note simultaneous frees of same memory will not get caught this way!
    //
    mem_blk_hd->header_pattern = MEM_FREED_PATTERN;

#if 0
    printf("<==free memory allocated from %s at line %d with length %d\n",
            mem_blk_hd->file,
            mem_blk_hd->line,
            mem_blk_hd->length);
    printf("<==free memory 0x%x\n", (u8 *)mem_blk_hd+sizeof(*mem_blk_hd));
#endif

    //
    // Check that trailer was not corrupted
    //
    value = *(u32 __packed *)((u8 *)p + mem_blk_hd->length + mem_blk_hd->pad);
    if (value != MEM_TAILER_PATTERN) {
        printf("Memory corruption due to overflow detected at %p\n", p);
        printf("Dumping information about memory block. "
                "This information may itself have been "
                "corrupted and could cause machine to bugcheck.\n");
        printf("Memory was allocated from %s at line %d\n",
                mem_blk_hd->file, mem_blk_hd->length);
    }

    //
    // Remove this memory block from the list of allocations
    //
    tls_os_sem_acquire(mem_sem, 0);
    dl_list_del(&mem_blk_hd->list);
    alloc_heap_mem_bytes -= mem_blk_hd->length + sizeof(MEMORY_BLOCK) + sizeof(u32) + 
        mem_blk_hd->pad;
    alloc_heap_mem_blk_cnt--;
    tls_os_sem_release(mem_sem);

    //memset(mem_blk_hd, 0x5A, mem_blk_hd->length + sizeof(MEMORY_BLOCK) + + mem_blk_hd->pad + sizeof(u32));
    free(mem_blk_hd);
}

void tls_mem_alloc_info(void)
{
    int i;
    MEMORY_BLOCK * pos;

    tls_os_sem_acquire(mem_sem, 0);
    i = 1;
    dl_list_for_each(pos, &memory_list, MEMORY_BLOCK, list){
        printf("Block(%2d): addr<%p>, file<%s>, line<%d>, length<%d>\n",
                i, pos, pos->file, pos->line, pos->length);
        i++;
    }
    tls_os_sem_release(mem_sem);
    
}

#endif /* DEBUG */


