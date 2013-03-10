/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * You __MUST__ add your user information here and in the team array bellow.
 * 
 * === User information ===
 * Group: HardRock
 * User 1: knutur11 
 * SSN: 1701834449
 * User 2: haukurr11
 * SSN: 1911912269 
 * === End User Information ===
 */

/*
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
    "HardRock",
    /* First member's full name */
    "Knutur Oli Magnusson",
    /* First member's email address */
    "knutur11@ru.is",
    /* Second member's full name (leave blank if none) */
    "Haukur Rosinkranz",
    /* Second member's email address (leave blank if none) */
    "haukurr11@ru.is",
    /* Third member's full name (leave blank if none) */
    "",
    /* Third member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// Macros from the text book on page 830
//
#define WSIZE       4       //header/footer size
#define DSIZE       8       //total overhead size
#define CHUNKSIZE  (1<<12)  //amnt to extend heap by

#define MAX(x, y) ((x) > (y)? (x) : (y))

#define PACK(size, alloc)  ((size) | (alloc)) //puts size and allocated byte into 4 bytes

#define GET(p)       (*(unsigned int *)(p)) //read word at address p
#define PUT(p, val)  (*(unsigned int *)(p) = (val)) //write word at address p

#define GET_SIZE(p)  (GET(p) & ~0x7) //extracts size from 4 byte header/footer
#define GET_ALLOC(p) (GET(p) & 0x1) //extracts allocated byte from 4 byte header/footer

#define HEADER(ptr)       ((char *)(ptr) - WSIZE) //get ptr's header address
#define FOOTER(ptr)       ((char *)(ptr) + GET_SIZE(HEADER(ptr)) - DSIZE) //get ptr's footer address

#define NEXT(ptr)  ((char *)(ptr) + GET_SIZE(((char *)(ptr) - WSIZE))) //next block
#define PREVIOUS(ptr)  ((char *)(ptr) - GET_SIZE(((char *)(ptr) - DSIZE))) //prev block


//Private variables
void *heap_listp;
void *free_listp;

//Private functions from the book
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FOOTER(PREVIOUS(bp)));
    size_t next_alloc = GET_ALLOC(HEADER(NEXT(bp)));
    size_t size = GET_SIZE(HEADER(bp));
    if (prev_alloc && next_alloc) { /* Case 1 */
        return bp;
    }
    else if (prev_alloc && !next_alloc) { /* Case 2 */
        size += GET_SIZE(HEADER(NEXT(bp)));
        PUT(HEADER(bp), PACK(size, 0));
        PUT(FOOTER(bp), PACK(size,0));
    }
    else if (!prev_alloc && next_alloc) { /* Case 3 */
        size += GET_SIZE(HEADER(PREVIOUS(bp)));
        PUT(FOOTER(bp), PACK(size, 0));
        PUT(HEADER(PREVIOUS(bp)), PACK(size, 0));
        bp = PREVIOUS(bp);
    }
    else { /* Case 4 */
        size += GET_SIZE(HEADER(PREVIOUS(bp))) +
        GET_SIZE(FOOTER(NEXT(bp)));
        PUT(HEADER(PREVIOUS(bp)), PACK(size, 0));
        PUT(FOOTER(NEXT(bp)), PACK(size, 0));
        bp = PREVIOUS(bp);
    }
    return bp;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;
    
    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
    return NULL;
    
    /* Initialize free block header/footer and the epilogue header */
    PUT(HEADER(bp), PACK(size, 0)); /* Free block header */
    PUT(FOOTER(bp), PACK(size, 0)); /* Free block footer */
    PUT(HEADER(NEXT(bp)), PACK(0, 1));/* New epilogue header */
    
    /* Coalesce if the previous block was free */
    return coalesce(bp);
} 

static void *find_fit(size_t asize)
{
    /* First fit search */
    void *bp;
    for (bp = heap_listp; GET_SIZE(HEADER(bp)) > 0; bp = NEXT(bp)) {
    if (!GET_ALLOC(HEADER(bp)) && (asize <= GET_SIZE(HEADER(bp)))) {
        return bp;
        }
    }
    return NULL;/* No fit */
}

static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HEADER(bp));
    if ((csize - asize) >= (2*DSIZE)) {
        PUT(HEADER(bp), PACK(asize, 1));
        PUT(FOOTER(bp), PACK(asize, 1));
        bp = NEXT(bp);
        PUT(HEADER(bp), PACK(csize-asize, 0));
        PUT(FOOTER(bp), PACK(csize-asize, 0));
    }
    else {
        PUT(HEADER(bp), PACK(csize, 1));
        PUT(FOOTER(bp), PACK(csize, 1));
    }
}

//The functions we implement
int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0); /* Alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));/* Prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));/* Prologue footer */
    PUT(heap_listp + (3*WSIZE), PACK(0, 1)); /* Epilogue header */
    heap_listp += (2*WSIZE);
    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if ( (extend_heap(CHUNKSIZE/WSIZE)) == NULL)
        return -1;
    (*(int *)(heap_listp+(DSIZE/4))) = NULL; 
    (*(int *)(heap_listp+((DSIZE+WSIZE/4)))) = NULL;
    
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize; /* Adjusted block size */
    size_t extendsize;/* Amount to extend heap if no fit */
    char *bp;
    /* Ignore spurious requests */
    if (size == 0)
    return NULL;
    /* Adjust block size to include overhead and alignment reqs.*/
    if (size <= DSIZE)
    asize = 2*DSIZE;
    else
    asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }
    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize,CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
    return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
   int csize = GET_SIZE(HEADER(ptr));
   PUT(HEADER(ptr), PACK(csize, 0)); /* Free block header */
   PUT(FOOTER(ptr), PACK(csize, 0)); /* Free block footer */
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}
