/* 
 * mm-implicit.c -  Simple allocator based on implicit free lists, 
 *                  first fit placement, and boundary tag coalescing. 
 *
 * Each block has header and footer of the form:
 * 
 *      31                     3  2  1  0 
 *      -----------------------------------
 *     | s  s  s  s  ... s  s  s  0  0  a/f
 *      ----------------------------------- 
 * 
 * where s are the meaningful size bits and a/f is set 
 * iff the block is allocated. The list has the following form:
 *
 * begin                                                          end
 * heap                                                           heap  
 *  -----------------------------------------------------------------   
 * |  pad   | hdr(8:a) | ftr(8:a) | zero or more usr blks | hdr(8:a) |
 *  -----------------------------------------------------------------
 *          |       prologue      |                       | epilogue |
 *          |         block       |                       | block    |
 *
 * The allocated prologue and epilogue blocks are overhead that
 * eliminate edge conditions during coalescing.
 */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "mm.h"
#include "memlib.h"



/*
 * If NEXT_FIT defined use next fit search, else use first fit search 
 */
#define NEXT_FIT



/* $begin mallocmacros */
/* Basic constants and macros */
#define WSIZE       4       /* word size (bytes) */  
#define DSIZE       8       /* doubleword size (bytes) */
#define CHUNKSIZE  (1<<12)  /* initial heap size (bytes) */
#define OVERHEAD    8       /* overhead of header and footer (bytes) */

#define MAX(x, y) ((x) > (y)? (x) : (y))  

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)       (*(size_t *)(p))
#define PUT(p, val)  (*(size_t *)(p) = (val))  

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)  
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* need to inform of this function*/
#define FT_NEXT(bp)  ((char*)(bp) + GET_SIZE(HDRP((char*)bp)) - WSIZE - DSIZE)
#define FT_PREV(bp)  ((char*)(bp) + GET_SIZE(HDRP((char*)bp)) - WSIZE - WSIZE)

/* $end mallocmacros */

/* Global variables */
static char *_heap_listp;  /* pointer to first block */  

//first free 
char *_free_listp;

#ifdef NEXT_FIT
static char *rover;       /* next fit rover */
#endif

static int _heap_ext_counter=0;

/* function prototypes for internal helper routines */
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void printblock(void *bp); 
static void checkblock(void *bp);

//for project
static void *find_free(size_t asize);

char* get_heap_listp() {
    return _heap_listp;
}
char* set_and_get_heap_listp(char* ptr) {
    _heap_listp = ptr;
    return _heap_listp;
}

/* 
 * mm_init - Initialize the memory manager 
 */
/* $begin mminit */
int mm_init(void) 
{
    /* create the initial empty heap */
    if (set_and_get_heap_listp(mem_sbrk(6*WSIZE)) == (void *)-1)
	return -1;
    PUT(get_heap_listp(), 0);                        /* alignment padding */
    PUT(get_heap_listp()+WSIZE, PACK(24, 0));  /* prologue header */ 
    PUT(get_heap_listp()+DSIZE * 2 + WSIZE, PACK(0, 0));   /* epilogue header */
    PUT(get_heap_listp()+WSIZE + WSIZE + WSIZE, 0);
    PUT(get_heap_listp()+DSIZE+DSIZE, 0);
    set_and_get_heap_listp(get_heap_listp()+WSIZE);

#ifdef NEXT_FIT
    _free_listp = get_heap_listp();
    rover = get_heap_listp();
#endif

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
	return -1;
    return 0;
}
/* $end mminit */

/* 
 * mm_malloc - Allocate a block with at least size bytes of payload 
 */
/* $begin mmmalloc */
void *mm_malloc(size_t size) 
{
	int alloc_size;      /* adjusted block size */
	int extendsize; /* amount to extend heap if no fit */
	char *bp;      

    /* Ignore spurious requests */
	if (size <= 0) 
	{
		return NULL;
	}
    /* Adjust block size to include overhead and alignment reqs. */
  	if (size <= DSIZE)
	{
		alloc_size = DSIZE + DSIZE + OVERHEAD;
	}
   	else
	{
		alloc_size = DSIZE*((DSIZE + DSIZE + OVERHEAD -1 + size) /DSIZE);
	}
    
    /* Search the free list for a fit */
	if ((bp = find_free(alloc_size)) != NULL) 
	{
		place(bp, alloc_size);
		return bp;
	}

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(alloc_size,CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL) {
		return NULL;
	 }
    place(bp, alloc_size);
	mm_checkheap();    

	
    return bp;
} 
/* $end mmmalloc */

//same with find_fit 
static void *find_free(size_t asize)
{
	/* next fit search */
	char *oldrover = rover;

    /* search from the rover to the end of list */
	for ( ; rover != 0 ; rover = GET(FT_NEXT(rover)))
	{
		printf("pointer %p , HDRP %p,\n", rover, GET_SIZE(HDRP(rover)));
	//	if ( asize <= GET_SIZE(HDRP(rover)))
	//		{
//			return rover;
//		}
	}	

    /* search from start of list to old rover */
	for (rover = _free_listp; rover < oldrover; rover = GET(FT_NEXT(rover)))
		if (!GET_ALLOC(HDRP(rover)) && (asize <= GET_SIZE(HDRP(rover))))
	    		return rover;
	
	return NULL;  /* no fit found */ 	
}
/* 
 * mm_free - Free a block 
 */
/* $begin mmfree */
void mm_free(void *bp)
{
	//TODO
}

/* $end mmfree */

/*
 * mm_realloc - naive implementation of mm_realloc
 */
void *mm_realloc(void *ptr, size_t size)
{
	//TODO 
	return NULL;
}

/* 
 * mm_checkheap - Check the heap for consistency 
 */
void mm_checkheap(int verbose) 
{
    char *bp = get_heap_listp();

    if (verbose)
	printf("Heap (%p):\n", get_heap_listp());

    if ((GET_SIZE(HDRP(get_heap_listp())) != DSIZE) || !GET_ALLOC(HDRP(get_heap_listp())))
	printf("Bad prologue header\n");
    checkblock(get_heap_listp());

    for (bp = get_heap_listp(); GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
	if (verbose) 
	    printblock(bp);
	checkblock(bp);
    }
     
    if (verbose)
	printblock(bp);
    if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp))))
	printf("Bad epilogue header\n");
}

/* The remaining routines are internal helper routines */

/* 
 * extend_heap - Extend heap with free block and return its block pointer
 */
/* $begin mmextendheap */
static void *extend_heap(size_t words) 
{
    char *bp;
    size_t size;
    _heap_ext_counter++;
	
    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((bp = mem_sbrk(size)) == (void *)-1) 
	return NULL;


    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* free block header */
    PUT(FTRP(bp), PACK(size, 0));         /* free block footer */
    PUT(FT_NEXT(bp), 0);
    PUT(FT_PREV(bp), 0);
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* new epilogue header */

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}
/* $end mmextendheap */

/* 
 * place - Place block of asize bytes at start of free block bp 
 *         and split if remainder would be at least minimum block size
 */
/* $begin mmplace */
/* $begin mmplace-proto */
static void place(void *bp, size_t asize)
/* $end mmplace-proto */
{
    size_t csize = GET_SIZE(HDRP(bp));   
	char* prev, next;
	
    if ((csize - asize) >= (DSIZE + DSIZE + OVERHEAD)) { 
	PUT(HDRP(bp), PACK(asize, 1));
	PUT(FTRP(bp), PACK(asize, 1));
	prev = GET(FT_PREV(bp));

	bp = NEXT_BLKP(bp);
	PUT(HDRP(bp), PACK(csize-asize, 0));
	PUT(FTRP(bp), PACK(csize-asize, 0));

	//change prev
	if(prev != 0)
	{
		PUT(FT_NEXT(prev), bp);
	}
	_free_listp = bp;
    }
    else { 
	prev = GET(FT_PREV(bp));
	next = GET(FT_NEXT(bp));	

	PUT(HDRP(bp), PACK(csize, 1));
	PUT(FTRP(bp), PACK(csize, 1));
	if(prev != 0)
	{
		PUT(FT_NEXT(prev), next);
	}
	
	if(next != 0)
	{
		PUT(FT_PREV(next), prev);
	}
	_free_listp =next;
	
    }
}
/* $end mmplace */

/* 
???END
 * find_fit - Find a fit for a block with asize bytes 
 */
static void *find_fit(size_t asize)
{
#ifdef NEXT_FIT 
    /* next fit search */
    char *oldrover = rover;

    /* search from the rover to the end of list */
    for ( ; GET_SIZE(HDRP(rover)) > 0; rover = NEXT_BLKP(rover))
	if (!GET_ALLOC(HDRP(rover)) && (asize <= GET_SIZE(HDRP(rover))))
	    return rover;

    /* search from start of list to old rover */
    for (rover = get_heap_listp(); rover < oldrover; rover = NEXT_BLKP(rover))
	if (!GET_ALLOC(HDRP(rover)) && (asize <= GET_SIZE(HDRP(rover))))
	    return rover;

    return NULL;  /* no fit found */
#else 
    /* first fit search */
    void *bp;

    for (bp = get_heap_listp(); GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
	if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
	    return bp;
	}
    }
    return NULL; /* no fit */
#endif
}

/*
 * coalesce - boundary tag coalescing. Return ptr to coalesced block
 */
static void *coalesce(void *bp) 
{
    char* prev = PREV_BLKP(bp);
    char* next = NEXT_BLKP(bp);    
    while(prev > get_heap_listp() && GET_ALLOC(FTRP(prev)))
    {
	prev = PREV_BLKP(bp);
	if(prev < get_heap_listp())
	{
		prev = NULL;
		break;
	}
    }
    
    while(GET_SIZE(HDRP(next)) > 0 && GET_ALLOC(HDRP(next)))
    {
	next = NEXT_BLKP(bp);
	if(GET_SIZE(HDRP(next)) <= 0)
	{
		next = NULL;
		break;
	}
    }
	
    size_t prev_alloc;
    if(PREV_BLKP(bp) < get_heap_listp())
    {
	prev_alloc = 1;
    }
    else
    {
	prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    }

    size_t next_alloc;
    if(GET_SIZE(HDRP(NEXT_BLKP(bp))) <= 0)
    {
	next_alloc = 1;
    }
    else
    {
    	next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    }
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {          /* Case 1 : front alloc && back  alloc */
	
    }
    else if (prev_alloc && !next_alloc) {      /* Case 2 : front alloc && back not alloc*/
	size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
	PUT(HDRP(bp), PACK(size, 0));
	PUT(FTRP(bp), PACK(size,0));
    }
    else if (!prev_alloc && next_alloc) {      /* Case 3 : front not alloc && back alloc*/
	size += GET_SIZE(HDRP(PREV_BLKP(bp)));
	PUT(FTRP(bp), PACK(size, 0));
	PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
	bp = PREV_BLKP(bp);
    }

    else {                                     /* Case 4 : front not alloc && back not alloc*/
	size += GET_SIZE(HDRP(PREV_BLKP(bp))) + 
	    GET_SIZE(FTRP(NEXT_BLKP(bp)));
	PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
	PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
	bp = PREV_BLKP(bp);
    }

#ifdef NEXT_FIT
    /* Make sure the rover isn't pointing into the free block */
    /* that we just coalesced */
    if ((rover > (char *)bp) && (rover < NEXT_BLKP(bp))) 
	rover = bp;
#endif

    return bp;
}


static void printblock(void *bp) 
{
    size_t hsize, halloc, fsize, falloc;

    hsize = GET_SIZE(HDRP(bp));
    halloc = GET_ALLOC(HDRP(bp));  
    fsize = GET_SIZE(FTRP(bp));
    falloc = GET_ALLOC(FTRP(bp));  
    
    if (hsize == 0) {
	printf("%p: EOL\n", bp);
	return;
    }

    printf("%p: header: [%d:%c] footer: [%d:%c]\n", bp, 
	   hsize, (halloc ? 'a' : 'f'), 
	   fsize, (falloc ? 'a' : 'f')); 
}

static void checkblock(void *bp) 
{
    if ((size_t)bp % 8)
	printf("Error: %p is not doubleword aligned\n", bp);
    if (GET(HDRP(bp)) != GET(FTRP(bp)))
	printf("Error: header does not match footer\n");
}


