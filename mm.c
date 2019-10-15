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
static char *_heap_listp;  /* pointer to first block */  



/* ���� ������ �͵�*/

#define GET_NEXT(bp)  (*(char**)(bp))
#define GET_PREV(bp)  (*(char**)(bp + WSIZE))

/* free list�� �� �ּҰ� ���� ���� ���� �����Ѵ�.*/
char *_free_listp = NULL;

void insert_free(char* ptr);

void delete_free(char* ptr);

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
	PUT(get_heap_listp() + WSIZE * 2, 0); //next
    PUT(get_heap_listp() + WSIZE * 3, 0); //prev
	PUT(get_heap_listp() + WSIZE * 5, PACK(0, 1));   /* epilogue header */
	set_and_get_heap_listp(get_heap_listp() + 2 * WSIZE);


	
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
		alloc_size = DSIZE + DSIZE + OVERHEAD;
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
	for (ptr = _free_listp; ptr != NULL; ptr = GET_NEXT(ptr))
	{
		if (GET_SIZE(HDRP(ptr)) <= asize)
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
	char* prev, next;
	
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
	int prev_alloc = (GET_ALLOC(FTRP(PREV_BLKP(bp))) | PREV_BLKP(bp) == bp);
    //���ʷα� ����� ������ �� �� ��Ȳ������ �� �۵��Ѵ�.
	int next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));

    if (prev_alloc && next_alloc) {          /* Case 1 : ���ڸ��� ���ڸ� ��� �Ҵ�� ���� �� ��*/
		delete_free(bp);
		return bp;
    }
    else if (prev_alloc && !next_alloc) {      /* Case 2 : ���ڸ��� �Ҵ�, ���ڸ��� �Ҵ���� ���� ���� �� �� */
		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
		delete_free(NEXT_BLKP(bp));
		PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size,0));
		//������ next, prev�� �״�� �����Ǵ� ���д�.
    }
    else if (!prev_alloc && next_alloc) {      /* Case 3 : ���ڸ��� �Ҵ� �����ʰ�, ���ڸ��� �Ҵ� �Ǿ��� ��*/
		size += GET_SIZE(HDRP(PREV_BLKP(bp)));
		delete_free(bp);
		PUT(FTRP(bp), PACK(size, 0));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
    }
    else {                                     /* Case 4 : ���ڸ��� ���ڸ� ��� �Ҵ� ���� �ʾ��� ��*/
		size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
		delete_free(NEXT_BLKP(bp));
		delete_free(bp);
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
    }
	

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


void insert_free(char* ptr)
{
	//������ ó���̸��� �ʱ�ȭ�����ش�
	if (_free_listp == NULL)
	{
		_free_listp = ptr;
		GET_PREV(ptr) = NULL;
		GET_NEXT(ptr) = NULL;
		return;
	}
	//���� �������� ���� ���� ����Ʈ ������� �������ش�.
	GET_NEXT(ptr) = _free_listp;
	GET_PREV(ptr) = NULL;
	GET_PREV(_free_listp) = ptr;

	_free_listp = ptr;
}

void delete_free(char* ptr)
{

	//�� �������̸� ���� ������ ������ ���´�.
	if (GET_PREV(ptr) == NULL)
	{
		_free_listp = GET_NEXT(ptr);
		GET_PREV(GET_NEXT(ptr)) = NULL;
	}
	else//�߰��̸� ���� ���� ���� ��带 ��������ش�.
	{
		GET_NEXT(GET_PREV(ptr)) = GET_NEXT(ptr);
		GET_PREV(GET_NEXT(ptr)) = GET_PREV(ptr);
	}

}

