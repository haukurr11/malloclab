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
team_t team =
{
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
#define GET_NEXT_FREE(p)       (*(unsigned int *)(p+WSIZE)) //read word at address p
#define GET_PREV_FREE(p)      GET(p)
#define PUT(p, val)  (*(unsigned int *)(p) = (val)) //write word at address p
#define PUT_NEXT_FREE(p, val)  (*(unsigned int *)(p+WSIZE) = (val)) //write word at address p
#define PUT_PREV_FREE(p, val) PUT(p,val)

#define GET_SIZE(p)  (GET(p) & ~0x7) //extracts size from 4 byte header/footer
#define GET_ALLOC(p) (GET(p) & 0x1) //extracts allocated byte from 4 byte header/footer

#define HEADER(ptr)       ((char *)(ptr) - WSIZE) //get ptr's header address
#define FOOTER(ptr)       ((char *)(ptr) + GET_SIZE(HEADER(ptr)) - DSIZE) //get ptr's footer address

#define NEXT(ptr)  ((char *)(ptr) + GET_SIZE(((char *)(ptr) - WSIZE))) //next block
#define PREVIOUS(ptr)  ((char *)(ptr) - GET_SIZE(((char *)(ptr) - DSIZE))) //prev block


//Private variables
void *free_listp;

/* Adds a pointer to the freestack */
static void inline freestack_push( void *element)
{
    if(free_listp == NULL)
        {
            PUT_NEXT_FREE(element, NULL);
            PUT(element, NULL);
        }
    else
        {
            PUT_NEXT_FREE(element, free_listp);
            PUT(element, NULL);
            PUT(free_listp, element);
        }
    free_listp = element;
}

/* Removes a pointer from the freestack */
static void inline freestack_remove( void* element)
{
    void *prev = GET_PREV_FREE(element);
    void *next = GET_NEXT_FREE(element);
    if(prev != NULL)
        {
            PUT_NEXT_FREE(prev, next);
            if(next != NULL)
                PUT_PREV_FREE(next, prev);
        }
    else
        {
            free_listp = next;
            if(free_listp != NULL)
                PUT_PREV_FREE(free_listp, NULL);
        }
}


/*Coalesces and adds to the freestack */
static void inline *coalesce(void *bp)
{
    if(bp == NULL)
        return NULL;
    size_t prev_alloc = GET_ALLOC(FOOTER(PREVIOUS(bp)));
    size_t next_alloc = GET_ALLOC(HEADER(NEXT(bp)));
    size_t size = GET_SIZE(HEADER(bp));
    if (prev_alloc && next_alloc)   /* Case 1 */
        {
            freestack_push(bp);
        }
    else if (prev_alloc && !next_alloc)   /* Case 2 */
        {
            freestack_remove(NEXT(bp));
            size += GET_SIZE(HEADER(NEXT(bp)));
            PUT(HEADER(bp), PACK(size, 0));
            PUT(FOOTER(bp), PACK(size, 0));
            freestack_push(bp);
        }
    else if (!prev_alloc && next_alloc)   /* Case 3 */
        {
            size += GET_SIZE(HEADER(PREVIOUS(bp)));
            PUT(FOOTER(bp), PACK(size, 0));
            PUT(HEADER(PREVIOUS(bp)), PACK(size, 0));
            bp = PREVIOUS(bp);
        }
    else   /* Case 4 */
        {
            freestack_remove( NEXT(bp));
            size += GET_SIZE(HEADER(PREVIOUS(bp))) +
                    GET_SIZE(FOOTER(NEXT(bp)));
            PUT(HEADER(PREVIOUS(bp)), PACK(size, 0));
            PUT(FOOTER(NEXT(bp)), PACK(size, 0));
            bp = PREVIOUS(bp);
        }
    return bp;
}

/* Extends the heap by N words */
static void inline *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HEADER(bp), PACK(size, 0)); /* Free block header */
    PUT(FOOTER(bp), PACK(size, 0)); /* Free block footer */
    PUT(HEADER(NEXT(bp)), PACK(0, 1));/* New epilogue header */
    /* Coalesce if the previous block was free */
    return (bp);
}

/* Finds a free space in the freelist, returns NULL if none found */
static void inline *find_fit(size_t asize)
{
    /* First fit search */
    void *bp;
    for (bp = free_listp; bp != NULL ; bp = GET_NEXT_FREE(bp)  )
        {
            if (asize <= GET_SIZE(HEADER(bp)))
                {
                    return (bp);
                }
        }
    return NULL;/* No fit */
}


/* Allocates memory from the freelist */
static void inline place(void *bp, size_t asize)
{
    if(bp == NULL)
        return;
    void* prev = GET_PREV_FREE(bp);
    void* next = GET_NEXT_FREE(bp);
    size_t csize = GET_SIZE(HEADER(bp));
    if ( (csize - asize) >= (2 * DSIZE))
        {
            PUT(HEADER(bp), PACK(asize, 1));
            PUT(FOOTER(bp), PACK(asize, 1));
            bp = NEXT(bp);
            PUT(HEADER(bp), PACK(csize - asize, 0));
            PUT(FOOTER(bp), PACK(csize - asize, 0));
            PUT_PREV_FREE(bp, NULL);
            PUT_NEXT_FREE(bp, NULL);
            if(prev == NULL && next == NULL)
                {
                    free_listp = bp;
                }
            else if( prev == NULL )
                {
                    free_listp = bp;
                    PUT_NEXT_FREE(bp, next);
                    if(next != NULL)
                        PUT_PREV_FREE(next, bp);
                }
            else
                {
                    PUT_NEXT_FREE(prev, bp);
                    PUT_NEXT_FREE(bp, next);
                    PUT_PREV_FREE(bp, prev);
                    if(next != NULL)
                        PUT_PREV_FREE(next, bp);
                }
        }
    else
        {
            PUT(HEADER(bp), PACK(csize, 1));
            PUT(FOOTER(bp), PACK(csize, 1));
            freestack_remove(bp);
        }
}

//The functions we implement
int mm_init(void)
{
    void *heap_listp;
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *) - 1)
        return -1;
    PUT(heap_listp, 0); /* Alignment padding */
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); /* Prologue header */
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1)); /* Epilogue header */
    heap_listp += (2 * WSIZE);
    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if ( (free_listp = (extend_heap(CHUNKSIZE / WSIZE))) == NULL)
        return -1;
    PUT_PREV_FREE(free_listp, NULL);
    PUT_NEXT_FREE(free_listp, NULL);
    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    //mm_check();
    size_t asize; /* Adjusted block size */
    size_t extendsize;/* Amount to extend heap if no fit */
    char *bp;
    /* Ignore spurious requests */
    if (size == 0)
        return NULL;
    /* Adjust block size to include overhead and alignment reqs.*/
    if (size <= 4 * DSIZE)
        asize = 4 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL)
        {
            place( bp, asize);
            return bp;
        }
    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    freestack_push(bp);
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    if(ptr == NULL)
        return;
    int csize = GET_SIZE(HEADER(ptr));
    PUT(HEADER(ptr), PACK(csize, 0));
    PUT(FOOTER(ptr), PACK(csize, 0));
    coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    if(ptr == NULL)
        return;
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    copySize = GET_SIZE(HEADER(ptr));
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}


static inline int isInHeap( void *ptr)
{
    void* heaplo = mem_heap_lo();
    void* heaphi = mem_heap_hi();
    return (ptr >= heaplo  && ptr <= heaphi);
}

void mm_check()
{
    int count = 0;
    void *it;
    printf("Checking FreeList for errors\n");
    for(it = free_listp; it != NULL; it = GET_NEXT_FREE(it) )
        {
            printf("-------Checking block %d:\n", count);
            printf("Pointer address is: %p \n", it);
            printf("Size of block: %d \n", GET_SIZE(HEADER(it)));
            printf("Pointer next address is: %p \n", GET_NEXT_FREE(it));
            printf("Pointer prev address is: %p \n", GET_PREV_FREE(it));

            /* Is every block in the free list marked as free? */
            if(!GET_ALLOC(HEADER(it)))
                printf("Block is marked as free!\n");
            else
                {
                    printf("ERROR: ALLOCATED BLOCK IN FREELIST!\n");
                    exit(1);
                }
            /*Are there any contiguous free blocks that somehow escaped coalescing? */
            size_t prev_alloc = GET_ALLOC(FOOTER(PREVIOUS(it)));
            size_t next_alloc = GET_ALLOC(HEADER(NEXT(it)));
            if(!prev_alloc || !next_alloc)
                {
                    printf("ERROR: FREEBLOCK HAS ESCAPED COALESCING!\n");
                    exit(1);
                }
            else
                printf("Coalescing is correct\n");


            /*Do the pointers in the free list point to valid free blocks?*/
            if( GET_NEXT_FREE(it) != NULL &&  GET_ALLOC( HEADER(GET_NEXT_FREE(it))))
                {
                    printf("ERROR! NEXT POINTER NOT POINTING TO A FREE BLOCK");
                    exit(1);
                }
            if( GET_PREV_FREE(it) != NULL &&  GET_ALLOC( HEADER(GET_PREV_FREE(it))))
                {
                    printf("ERROR! PREV POINTER NOT POINTING TO A FREE BLOCK");
                    exit(1);
                }

            /*Do the pointers in a heap block point to valid heap addresses? */
            if(it >= mem_heap_lo() && it <= mem_heap_hi())
                printf("Found the pointer in heap\n");
            else
                {
                    printf("ERROR! POINTER NOT WITHIN HEAP\n");
                    exit(1);
                }

            if(GET_NEXT_FREE(it) == NULL || isInHeap(GET_NEXT_FREE(it)))
                printf("next is in Heap!\n");
            else
                {
                    printf("ERROR! NEXT MISSING FROM HEAP!\n");
                    exit(1);
                }
            if(GET_PREV_FREE(it) == NULL || isInHeap(GET_PREV_FREE(it)))
                printf("prev is in Heap!\n");
            else
                {
                    printf("ERROR! PREV MISSING FROM HEAP!!\n");
                    exit(1);
                }
            count++;
        }
    printf("No Errors( %d blocks )\n\n", count);

    /* If we reach this point, the freelist looks okay,
     * lets check if the heaplist is valid
    */
    void* heapit;
    int found = 0;
    void* listp = 0xf6bf0010; //same as heap_listp in mm_init
    for(heapit = listp; heapit < mem_heap_hi(); heapit = NEXT(heapit))
        {
            /* Do any allocated blocks overlap? */
            if( !(NEXT(heapit) > FOOTER(heapit)) )
                {
                    printf("ERROR! Block %p overlaps block %p\n", heapit, NEXT(heapit));
                    exit(1);
                }
            /* Is every free block actually in the free list? */
            if(!GET_ALLOC(HEADER(heapit)))
                {
                    void* it;
                    int found = 0;
                    for(it = free_listp; it != NULL; it = GET_NEXT_FREE(it))
                        if(it == heapit)
                            {
                                found = 1;
                                break;
                            }
                    if(!found)
                        {
                            printf("ERROR! FREE BLOCK NOT IN FREE LIST!\n");
                            exit(1);
                        }
                }
        }
}
