/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "UESTC_GAY_HUB",
    /* First member's full name */
    "Xiafeng~~~",
    /* First member's email address */
    "xiafeng_li@qq.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Basic constants and macros */
#define WSIZE 4             /* Word and header/footer size (bytes) */
#define DSIZE 8             /* Double word size (bytes) */
#define CHUNKSIZE (1<<12)   /* Extend heap by this amount (bytes) */

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p))
#define PUT(p, val)  (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))
static void* extend_heap(size_t words);
static void* coalesce(void *bp);
static void* find_fit(size_t asize);
static void place(void* bp, size_t asize);
static void* heap_listp;

static void* coalesce(void* bp){
    size_t pre_alloced = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloced = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    if(pre_alloced&&next_alloced){
        return bp;
    }
    //后一个块空闲
    else if(pre_alloced && !next_alloced){
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp),PACK(size,0));//HEAD设置空闲块大小和是否使用
        PUT(FTRP(bp),PACK(size,0));//FOOT设置空闲块大小和是否使用
    }
    //前一个块空闲
    else if(!pre_alloced && next_alloced){
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        // PUT(HDRP(PREV_BLKP(bp)),PACK(size,0));//HEAD设置空闲块大小和是否使用
        // PUT(FTRP(bp),PACK(cru_size,0));//FOOT设置空闲块大小和是否使用
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp=PREV_BLKP(bp);
    }
    //前后块空闲
    else if(!pre_alloced && !next_alloced){
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)))+GET_SIZE(FTRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)),PACK(size,0));//HEAD设置空闲块大小和是否使用
        PUT(FTRP(NEXT_BLKP(bp)),PACK(size,0));//FOOT设置空闲块大小和是否使用
        bp=PREV_BLKP(bp);
    }
    return bp;
}

/*
extend_head by $words size
*/
static void* extend_heap(size_t words){
    char* bp;
    size_t size;
    /* Allocate an even number of words to maintain alignment */
    size = (words%2) ? (words+1) * WSIZE:words *WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp),PACK(size,0));//head设置空闲块大小和是否使用
    PUT(FTRP(bp),PACK(size,0));//foot设置空闲块大小和是否使用
    PUT(HDRP(NEXT_BLKP(bp)),PACK(0,1));//设置结尾块
    
    return coalesce(bp);
}
/*
 * find_fit - use first fit strategy to find an empty block.
 */
static void *find_fit(size_t asize){
    for(char * ptr = heap_listp ; GET_SIZE(HDRP(ptr)) > 0 ; ptr = NEXT_BLKP(ptr) ){
        if( !GET_ALLOC(HDRP(ptr)) && (GET_SIZE(HDRP(ptr)) >= asize) )
            return ptr;
    }
    return NULL;
}

static void  place(void *bp,size_t asize){
    size_t csize = GET_SIZE(HDRP(bp));
    if( (csize-asize) >=2*ALIGNMENT){
        size_t newsize =csize-asize;
        PUT(HDRP(bp),PACK(asize,1));
        PUT(FTRP(bp),PACK(asize,1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp),PACK(newsize,0));
        PUT(FTRP(bp),PACK(newsize,0));
    }
    else{
        PUT(HDRP(bp),PACK(csize,1));
        PUT(FTRP(bp),PACK(csize,1));
    }
}
/* 
 * mm_init - initialize the malloc package.
 */

int mm_init(void)
{
    if ((heap_listp = mem_sbrk(WSIZE*4)) == (void*) -1)
        return -1;
    PUT(heap_listp,0);//填充字内容设置为0
    PUT(heap_listp + (1*WSIZE),PACK(DSIZE,1));//序言块头部设置块大小为2*WSIZE 且已经被分配
    PUT(heap_listp + (2*WSIZE),PACK(DSIZE,1));//序言块脚部设置块大小为2*WSIZE 且已经被分配
    PUT(heap_listp + (3*WSIZE),PACK(0,1));//结尾块
    heap_listp+= (2*WSIZE);
    if( extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
// void *mm_malloc(size_t size)
// {
//     int newsize = ALIGN(size + SIZE_T_SIZE);
//     void *p = mem_sbrk(newsize);
//     if (p == (void *)-1)
// 	    return NULL;
//     else {
//         *(size_t *)p = size;
//         return (void *)((char *)p + SIZE_T_SIZE);
//     }
// }

void *mm_malloc(size_t size)
{
    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;

    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}
/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp),PACK(size,0));
    PUT(FTRP(bp),PACK(size,0));
    coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    if(ptr==NULL)
        return NULL;
    if(size==0)
        mm_free(ptr);
    if ((newptr =  mm_malloc(size)) == NULL)
        return NULL;
    size_t newsize = GET_SIZE(HDRP(newptr));
    if(size < newsize ){
        newsize = size;
    }
    memcpy(newptr , oldptr , newsize-WSIZE);
    mm_free(oldptr);
    // size_t copySize;
    
    // newptr = mm_malloc(size);
    // if (newptr == NULL)
    //   return NULL;
    // copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    // if (size < copySize)
    //   copySize = size;
    // memcpy(newptr, oldptr, copySize);
    // mm_free(oldptr);
    return newptr;
}













