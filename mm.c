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
#define NEXT_FREE(ptr)  ((char *) ((ptr) + 4))//next free block
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

void validate() {
    void* ptr = heap_listp;
    int counter = 0;
    while( GET_SIZE(HEADER(ptr)) > 0)
    {
        if( GET_ALLOC(HEADER(ptr)) == 1 )
        {
            printf("Used block: %d bytes\n",GET_SIZE(HEADER(ptr)));
        }
        else {
            printf("Free block: %d bytes\n",GET_SIZE(HEADER(ptr)));
        }   
        ptr = NEXT(ptr);
        counter++;
    }
    printf("--validation done\n");
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
    return (bp);
} 

static void *find_fit(size_t asize)
{
    /* First fit search */
    int *bp;
    for (bp = free_listp; bp != NULL ; bp =*((int*)(bp+4))  ) {
    printf("it:%d %d\n",GET_SIZE(HEADER(bp)), GET_ALLOC(HEADER(bp)));
    printf("address: %x\n",*(bp+4));
    if (!GET_ALLOC(HEADER(bp)) && (asize <= GET_SIZE(HEADER(bp)))) {
        return ((bp));
        }
    }
    return NULL;/* No fit */
}

static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HEADER(bp));
    if ((csize - asize) >= (2*DSIZE)) {
        char* prev = *((char*) bp);
        int* next_current =*((int*)(bp+4));
            
        printf("place address: %x\n",bp);
        printf("next current: %x\n",next_current);
        printf("prev: %x\n",*((int*)bp));
        printf("next: %x\n",*((int*)(bp+4) ));
        PUT(HEADER(bp), PACK(asize, 1));
        PUT(FOOTER(bp), PACK(asize, 1));
        bp = NEXT(bp);
        if(prev == NULL)
        {
            free_listp = bp;
            *( (int*) free_listp) = NULL;
            *((int*)(free_listp+4)) = next_current;
        }
        else {
            *((int*)(prev+4)) = next_current;
        }
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
    printf("STARTING\n");
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0); /* Alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));/* Prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));/* Prologue footer */
    PUT(heap_listp + (3*WSIZE), PACK(0, 1)); /* Epilogue header */
    heap_listp += (2*WSIZE);
    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if ( (free_listp = (extend_heap(CHUNKSIZE/WSIZE))) == NULL)
        return -1;
    printf("size:%d\n", GET_SIZE(HEADER((char*) (free_listp))));
    printf("init address: %x\n",free_listp);
    printf("heap address: %x\n",heap_listp);
    *((int*)(free_listp)) = NULL; 
    *((int*)(free_listp+4)) = NULL;
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
        printf("%x/n",free_listp);
    if ((bp = find_fit(asize)) != NULL) {
        place( bp, asize);
        validate();
        return bp;
    }
    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize,CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
    return NULL;
    *( (int*) bp) = NULL;
    *((int*)(bp+4)) = free_listp;
    free_listp = bp;
    place(bp, asize);
    validate();
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
   int csize = GET_SIZE(HEADER(ptr));
   PUT(HEADER(ptr),PACK(csize,0));
   PUT(FOOTER(ptr),PACK(csize,0));
   *( (int*) ptr) = NULL;
   *((int*)(ptr+4)) = free_listp;
   free_listp = ptr;
   validate();
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
//    void *oldptr = ptr;
//    void *newptr;
//    size_t copySize;
//    
//    newptr = mm_malloc(size);
//    if (newptr == NULL)
//      return NULL;
//    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
//    if (size < copySize)
//      copySize = size;
//    memcpy(newptr, oldptr, copySize);
//    mm_free(oldptr);
//    return newptr;
}
static bool isInHeep(void *ptr)
{
	if(ptr >= mem_heep_lo() && ptr <= mem_heep_hi())
		return true;
	else
		return false;
}

static bool isAligned(void *ptr)
{
	return (size_t)ALIGN(ptr) == (size_t)ptr;
}

void mm_check(bool verb)
{
	int n = 0;
	void *listi = free_listp;
	printf("Checking list \n");
		while(listi != NULL)
		{
			printf("Block #%d\n",n);
			
			if(isInHeep(listi))
				printf("Found the pointer in heep\n");
			else
				printf("Error! the pointer was not in heep\n");
			
			if(isAligned(listi))
				printf("Aligned Block\n");
			else
				printf("Block is not aligned!\n");

			printf("Pointer address is: %p \n", listi);
			printf("Pointer next address is: %p \n", GET_NEXT(listi));
			printf("Pointer prev address is: %p \n", GET_PREV(listi));
			n++;
			listi = GET_NEXT(listi);
		}
	printf("\nNo segmentation fault in: %d blocks\n", n);
}

