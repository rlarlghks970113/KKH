/* 
 * �ý��� ���α׷��� �����Ҵ� ���� ����
 *  
 * �а� : ��ǻ�ͼ���Ʈ�����а�
 * �й� : 2016024739
 * �̸� : ���ȯ 
 */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "mm.h"
#include "memlib.h"


/*
 * ���� ���
 *
 * | header | next | prev | ....payload.... | footer |
 *
 * explicit free list�� ���������� �������.
 *
 *
 *
 */




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

/* $end mallocmacros */

/* Global variables */
static char *_heap_listp = 0;  /* pointer to first block */  



/* ���� ������ �͵�*/

#define GET_NEXT(bp)  (*(char**)(bp))
#define GET_PREV(bp)  (*(char**)(bp + WSIZE))

/* free list�� �� �ּҰ� ���� ���� ���� �����Ѵ�.*/
static char *_free_listp = NULL;

static void insert_free(char* ptr);

static void delete_free(char* ptr);

/* ���� ���� �� �� ��*/

static int _heap_ext_counter=0;

/* function prototypes for internal helper routines */

static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void printblock(void *bp); 
static void checkblock(void *bp);


char* get_heap_listp() {
    return _heap_listp;
}
char* set_and_get_heap_listp(char* ptr) {
    _heap_listp = ptr;
    return _heap_listp;
}

/* 
 * mm_init - �����Ҵ��� ���� �������̽��� ¥�� �Լ�
 */
/* $begin mminit */
int mm_init(void) 
{
    /* create the initial empty heap */
	if (set_and_get_heap_listp(mem_sbrk(6 * WSIZE)) == (void *)-1)
	{
		return -1;
	}
   	PUT(get_heap_listp(), 0);                        /* alignment padding */
   	PUT(get_heap_listp() + WSIZE, PACK(DSIZE, 1));  /* prologue header */
	PUT(get_heap_listp() + WSIZE * 2, PACK(DSIZE, 1)); //next
	PUT(get_heap_listp() + WSIZE * 3, PACK(0, 1));   /* epilogue header */
	set_and_get_heap_listp(get_heap_listp() + 2 * WSIZE);

	_free_listp = get_heap_listp();
	
    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
	if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
	{
		return -1;
	}

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
		alloc_size = DSIZE + OVERHEAD;
	}
   	else
	{
		alloc_size = DSIZE*((DSIZE + OVERHEAD -1 + size) /DSIZE);
	}
    
    /* �´°� ������ �ű�� �д� */
	if ((bp = find_fit(alloc_size)) != NULL) 
	{
		place(bp, alloc_size);
		return bp;
	}

    /* �´°� ������ heap�� ����� �ø���. */
    extendsize = MAX(alloc_size,CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL) {
		return NULL;
	 }
    place(bp, alloc_size);

    return bp;
} 
/* $end mmmalloc */

//same with find_fit 
static void *find_fit(size_t asize)
{
	char* ptr;
	for (ptr = _free_listp; GET_ALLOC(HDRP(ptr)) == 0; ptr = GET_NEXT(ptr))
	{
		if (GET_SIZE(HDRP(ptr)) >= asize)
		{
			return ptr;
		}
	}
	
	return NULL;  /* no fit found */ 	
}
/* 
 * mm_free - Free a block 
 */
/* $begin mmfree */
void mm_free(void *bp)
{
	size_t size = GET_SIZE(HDRP(bp));
	PUT(HDRP(bp), PACK(size, 0));
	PUT(FTRP(bp), PACK(size, 0));
	coalesce(bp);
}

/* $end mmfree */

/*
 * mm_realloc - naive implementation of mm_realloc
 */
void *mm_realloc(void *ptr, size_t size)
{
	size_t oldsize,newsize;
	void *newptr;

	//If size is negative it means nothing, just return NULL
	if((int)size < 0) 
    	return NULL;

	/* If size == 0 then this is just free, and we return NULL. */
	if (size == 0) {
		mm_free(ptr);
		return (NULL);
	}

	/* If oldptr is NULL, then this is just malloc. */
	if (ptr == NULL)
		return (mm_malloc(size));

	oldsize=GET_SIZE(HDRP(ptr));
	newsize = size + (2 * WSIZE);					// newsize after adding header and footer to asked size

	/* Copy the old data. */

	//If the size needs to be decreased, shrink the block and return the same pointer
	if (newsize <= oldsize){
		
	   /*
		* AS MENTIONED IN THE PROJECT HANDOUT THE CODE SNIPPET BELOW SHRINKS THE OLD ALLOCATED BLOCK
		* SIZE TO THE REQUESTED NEW SIZE BY REMOVING EXTRA DATA i.e. (oldsize-newsize) AMOUNT OF DATA.
		* ON RUNNING CODE WITH THIS SNIPPET, THE FOLLOWING ERROR OCCURS 'mm_realloc did not preserve 
		* the data from old block' WHICH WILL ALWAYS HAPPEN IF WE SHRINK THE BLOCK.
		*/

		/*if(oldsize-newsize<=2*DSIZE){
			return ptr;
		}
		PUT(HDRP(ptr),PACK(newsize,1));
		PUT(FTRP(ptr),PACK(newsize,1));
		PUT(HDRP(NEXT_BLKP(ptr)),PACK(oldsize-newsize,1));
		PUT(FTRP((NEXT_BLKP(ptr)),PACK(oldsize-newsize,1));
		mm_free(NEXT_BLKP(ptr));
		free_list_add(NEXT_BLKP(ptr));*/
		
		return ptr;
	}
	else{
		size_t if_next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));		//check if next block is allocated
		size_t next_blk_size = GET_SIZE(HDRP(NEXT_BLKP(ptr)));		//size of next block
		size_t total_free_size = oldsize + next_blk_size;			//total free size of current and next block

		//combining current and next block if total_free_size is greater then or equal to new size
		if(!if_next_alloc && total_free_size>= newsize){
			delete_free(NEXT_BLKP(ptr));	
			PUT(HDRP(ptr),PACK(total_free_size,1));
			PUT(FTRP(ptr),PACK(total_free_size,1));
			return ptr;
		}
		//finding new size elsewhere in free_list and copy old data to new place
		else{
			newptr=mm_malloc(newsize);
			
			/* If realloc() fails the original block is left untouched  */
			if (newptr == NULL)
				return (NULL);

			place(newptr,newsize);
			memcpy(newptr,ptr,oldsize);
			mm_free(ptr);
			return newptr;
		}
	}

}

/* 
 * mm_checkheap - Check the heap for consistency 
 */
void mm_checkheap(int verbose) 
{
    char *bp = _free_listp;
	
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
	
    /* Ȧ���� ���� �������� ¦���� ���� �ٲ��ش�. */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
	if ((bp = mem_sbrk(size)) == (void *)-1)
	{
		return NULL;
	}

    /* header�� footer�� ����� next�� prev�� colaesce�ܰ迡�� �ٷ��� �Ѵ� */
    PUT(HDRP(bp), PACK(size, 0));         /* ��� ����� */
	PUT(FTRP(bp), PACK(size, 0));	      /* Ǫ�� ����� */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* new epilogue header */

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}


/* 
 * place - Place block of asize bytes at start of free block bp 
 *         and split if remainder would be at least minimum block size
 */
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));   
	
	//������ �� �Ҵ�� ���� �Ǹ��� next�� prev�� ���������� �� (2 * DSIZE)���� ���ų� ũ�� ������ ������ �̿��� �� �ִ�.
    if ((csize - asize) >= (DSIZE + OVERHEAD)) { 
		PUT(HDRP(bp), PACK(asize, 1));
		PUT(FTRP(bp), PACK(asize, 1));
		delete_free(bp);
		bp = NEXT_BLKP(bp);
		PUT(HDRP(bp), PACK(csize-asize, 0));
		PUT(FTRP(bp), PACK(csize-asize, 0));
		insert_free(bp);	   
     }
    else { 
		//���� ��� �Ҵ� �Ǿ��ٸ� free_list���� �����ش�.
		PUT(HDRP(bp), PACK(csize, 1));
		PUT(FTRP(bp), PACK(csize, 1));
		delete_free(bp);
    }
}



/*
 * coalesce - boundary tag�� �̿��Ͽ� 4���� ���̽��� �����.
 *			- LIFO����� �̿��߱� ������ �� �����ϴ�.
 */
static void *coalesce(void *bp)
{
	/*�� �ڸ��� ���ڸ� �˻�*/

	//bp���� ���ڸ��� �̵��ص� �ּ� ��ȭ�� ���� ���� �� ���ڸ��̱� �����̴�.
	int prev = (GET_ALLOC(FTRP(PREV_BLKP(bp)))) | (PREV_BLKP(bp) == bp);
    //���ʷα� ����� ������ �� �� ��Ȳ������ �� �۵��Ѵ�.
	int next = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	size_t size = GET_SIZE(HDRP(bp));
    if (prev && next) {          /* Case 1 : ���ڸ��� ���ڸ� ��� �Ҵ�� ���� �� ��*/
		insert_free(bp);
		return bp;
    }
    else if (prev && !next) {      /* Case 2 : ���ڸ��� �Ҵ�, ���ڸ��� �Ҵ���� ���� ���� �� �� */
		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
		delete_free(NEXT_BLKP(bp));
		PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size,0));
		//������ next, prev�� �״�� �����Ǵ� ���д�.
    }
    else if (!prev && next) {      /* Case 3 : ���ڸ��� �Ҵ� �����ʰ�, ���ڸ��� �Ҵ� �Ǿ��� ��*/
		size += GET_SIZE(HDRP(PREV_BLKP(bp)));
		delete_free(PREV_BLKP(bp));
		PUT(FTRP(bp), PACK(size, 0));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
    }
    else {                                     /* Case 4 : ���ڸ��� ���ڸ� ��� �Ҵ� ���� �ʾ��� ��*/
		size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
		delete_free(PREV_BLKP(bp));
		delete_free(NEXT_BLKP(bp));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
    }
    insert_free(bp);

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


static void insert_free(char* ptr)
{
	//���� �������� ���� ���� ����Ʈ ������� �������ش�.
	GET_NEXT(ptr) = _free_listp;
	GET_PREV(ptr) = NULL;
	GET_PREV(_free_listp) = ptr;

	_free_listp = ptr;
}

static void delete_free(char* ptr)
{

	//�� �������̸� ���� ������ ������ ���´�.
	if (GET_PREV(ptr) == NULL)
	{
		_free_listp = GET_NEXT(ptr);
		GET_PREV(GET_NEXT(ptr)) = GET_PREV(ptr);
	}
	else//�߰��̸� ���� ���� ���� ��带 ��������ش�.
	{
		GET_NEXT(GET_PREV(ptr)) = GET_NEXT(ptr);
		GET_PREV(GET_NEXT(ptr)) = GET_PREV(ptr);
	}

}

