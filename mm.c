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
 * ����� free_list�� LIFO ������� ����ó�� �����ߴ�.
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

/* free list�� �� ���� �������� ���Ե� ���� ����Ų��.*/
static char *_free_listp = NULL;

/* free_list�� �����ϴ� �Լ�*/
static void insert_free(char* ptr);

/* free_list���� �����ϴ� �Լ�*/
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
/* �պκ��� ���ѷα� ����� ����� �κ�
 *
 * free_listp�� ��Ʈ�� ���ѷα� ����κп� ����� �д�.
 *
 */
int mm_init(void) 
{
    /* �������� �÷��ش� */
	if (set_and_get_heap_listp(mem_sbrk(6 * WSIZE)) == (void *)-1)
	{
		return -1;
	}
	/* �÷��� �������� ���� �������ش�*/
   	PUT(get_heap_listp(), 0);                        /* �򰥸��� �ʱ� ���� padding �κ� */
   	PUT(get_heap_listp() + WSIZE, PACK(DSIZE, 1));  /* ���ѷα� ��� */
	PUT(get_heap_listp() + WSIZE * 2, PACK(DSIZE, 1)); /* ���ѷα� Ǫ�� */
	PUT(get_heap_listp() + WSIZE * 3, PACK(0, 1));   /* ���ʷα� ��� */

	/* get_heap_listp�� ����� �ٷ� ���ڸ��� �̵������༭ ����� ����Ű�� �����. */
	set_and_get_heap_listp(get_heap_listp() + 2 * WSIZE);

	_free_listp = get_heap_listp();
	
    /* ���� ũ�⸦ �÷��ش� */
	if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
	{
		return -1;
	}

    return 0;
}

/* 
 * mm_malloc - �������� �޸𸮸� �����Ҵ��ϴ� ������ �Ѵ�.
 *
 */

void *mm_malloc(size_t size) 
{
	int alloc_size;      /* �Ҵ��� ������ (=Byte ����) */
	int extendsize; /* �ּ����� �Ҵ� ������ (=chunck size���� ũ�ų� ���ƾ� �Ѵ�.) */
	char *bp;      

    /* �̻��� size�� �������� null ��ȯ */
	if (size <= 0) 
	{
		return NULL;
	}
    /* ����� �ּ����� ������� ������ �ּ�ũ�� ���� */
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
	/* ������� �ø��� �ִ´� */
    place(bp, alloc_size);

    return bp;
} 


/* free_list�� ���鼭 ũ�Ⱑ �˸��� free_list�� �ּҸ� ��ȯ�Ѵ� */
static void *find_fit(size_t asize)
{
	char* ptr;
	/* fee_list�� ����*/
	for (ptr = _free_listp; GET_ALLOC(HDRP(ptr)) == 0; ptr = GET_NEXT(ptr))
	{
		/* ���� ũ�Ⱑ �´°� �ִٸ�  ��ȯ*/
		if (GET_SIZE(HDRP(ptr)) >= asize)
		{
			return ptr;
		}
	}
	
	/* ũ�Ⱑ �´°� ���ٸ� NULL��ȯ */
	return NULL;   	
}
/* 
 * mm_free - free�ϴ� �Լ�
 */
void mm_free(void *bp)
{
	//�־��� bp������ �ּҿ� bp�� ���� �����ŭ alloc bit�� 0���� �ٲ��ش�.
	size_t size = GET_SIZE(HDRP(bp));
	PUT(HDRP(bp), PACK(size, 0));
	PUT(FTRP(bp), PACK(size, 0));
	//free�� �Ǿ����� ��ģ��.
	coalesce(bp);
}



/*
 * mm_realloc - realloc�� �����ϰ� ����ȴ�
 */
void *mm_realloc(void *ptr, size_t size)
{
	/* ������ ptr�� ����Ű�� size*/
	size_t original_size;
	/* ��ġ�� ������ ���� new_ptr*/
	void* new_ptr;
	/* �䱸�� ����� header, footer�� ũ�⸦ ���� �ִ´�*/
	size += 2 * WSIZE;
	
	/* ptr�� NULL���̸� malloc�� ����*/
	if(ptr == NULL)
	{
		return  mm_malloc(size);
	}
	
	/* size�� 0�̸� free�� ����*/
	if(size == 0)
	{
		mm_free(ptr);
		return NULL;
	}
	
	/* ptr�� ����Ű�� ���� ���� ũ��*/
	original_size = GET_SIZE(HDRP(ptr));
	
	
	if(original_size >= size) /* ���ϴ� ����� ������ ������� �۰ų� �������*/
	{
		/* �� ������ �𸣰����� �� �κ��� ���� ��� error�� ����.*/
	/*	if(original_size - size >= 2 * DSIZE)
		{
			printf("-> 1\n");
			PUT(HDRP(ptr), PACK(size, 1));
			PUT(FTRP(ptr), PACK(size, 1));
			ptr = NEXT_BLKP(ptr);
			PUT(HDRP(ptr), PACK(original_size - size, 0));	
			PUT(HDRP(ptr), PACK(original_size - size, 0));
			
			insert_free(ptr);
			return PREV_BLKP(ptr);
		}
		else

			
			/* ����� �۰ų� ���� ��� �׳� ��ä�� ��ȯ ���ش�*/
			return ptr;

	}
	else if(original_size < size)/* ���ϴ� ����� ������ ������� ū ���*/
	{	
		
		_Bool prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(ptr))) || ptr == PREV_BLKP(ptr);/* �� ĭ�� alloc �Ǿ� �ִ��� Ȯ��*/
		_Bool next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));/* �� ĭ�� alloc �Ǿ� �ִ��� Ȯ��*/
		/* total_size == �̿� ������ ũ��*/
		size_t total_size = original_size;

		/* ���� ���̳� ��ĭ�� free���¶���� total_size�� �÷��ش�*/
		if(!prev_alloc)
		{	
			total_size += GET_SIZE(FTRP(PREV_BLKP(ptr)));
		}
		if(!next_alloc)
		{
			total_size += GET_SIZE(HDRP(NEXT_BLKP(ptr)));
		}
		
		
		if(total_size >= size) /* �̿� ������ ũ�Ⱑ ���� size���� Ŀ���ٸ�*/
		{
			if(!prev_alloc && next_alloc) /* ��ĭ�� free�̰� ��ĭ�� not free�� ��� */
			{

				/* ���� ������ ���� �����ŭ�� ���� �����ϰ� �� �κ��� free�� ���� ��ȹ�̾����� ������ ���� �����߽��ϴ�*/
			/*	if(total_size - size >= 2 * DSIZE)
				{
					printf("->1\n");
					new_ptr = PREV_BLKP(ptr);
					delete_free(PREV_BLKP(ptr));
					memmove(new_ptr, ptr, size - 2 *WSIZE);					

					PUT(HDRP(new_ptr), PACK(size, 1));
					PUT(FTRP(new_ptr), PACK(size, 1));
					ptr = NEXT_BLKP(new_ptr);
					PUT(HDRP(ptr), PACK(total_size - size, 0));
					PUT(FTRP(ptr), PACK(total_size - size, 0));
					insert_free(ptr);
					return new_ptr;

					
					/*�� �κ��� ��ģ ���·� ��ȯ���ش�.*/
					new_ptr = PREV_BLKP(ptr);
					delete_free(PREV_BLKP(ptr));
					 
					memcpy(new_ptr, ptr, size - 2*WSIZE);
					PUT(HDRP(new_ptr), PACK(total_size, 1));
					PUT(FTRP(new_ptr), PACK(total_size, 1));
					return new_ptr;

				
						
			}
			else if(prev_alloc && !next_alloc)
			{
				/*  ���������� �պκп� �Ҵ��ϰ� �޺κ��� free�� �������ַ� �ߴµ� error���� �����߽��ϴ�*/
			/*	if(total_size - size >= 2 * DSIZE)
				{
					printf("-> 1\n");
					delete_free(NEXT_BLKP(ptr));
	
					PUT(HDRP(ptr), PACK(size, 1));
					PUT(FTRP(ptr), PACK(size, 1));
					ptr = NEXT_BLKP(ptr);
					PUT(HDRP(ptr), PACK(total_size - size, 0));
					PUT(FTRP(ptr), PACK(total_size - size, 0));
					insert_free(ptr);
					
					return PREV_BLKP(ptr);
				}
				else
				{*/

					/* �޺κ��� ��ģ��*/
					delete_free(NEXT_BLKP(ptr));

					PUT(HDRP(ptr), PACK(total_size, 1));
					PUT(FTRP(ptr), PACK(total_size, 1));
					return ptr;
			//	}
			}
			else if(!prev_alloc && !next_alloc)
			{
				/*�պκ� �Ҵ� �� �޺κ� free�ҷ� ������ �������� ������*/
			/*	if(total_size - size >= 2 * DSIZE)
				{
					printf("-> 1\n");
					new_ptr = PREV_BLKP(ptr);
					delete_free(NEXT_BLKP(ptr));
					delete_free(PREV_BLKP(ptr));
					memmove(new_ptr, ptr, size - 2*WSIZE);
					
					PUT(HDRP(new_ptr), PACK(size, 1));
					PUT(FTRP(new_ptr), PACK(size, 1));
					ptr = NEXT_BLKP(new_ptr);
					PUT(HDRP(ptr), PACK(total_size - size, 1));
					PUT(FTRP(ptr), PACK(total_size - size, 1));
					insert_free(ptr);
					
					return new_ptr;
				}
				else
				{*/

					/* ��ģ ���� ��ȯ�ϴ� ��*/
					new_ptr = PREV_BLKP(ptr);
					delete_free(PREV_BLKP(ptr));
					delete_free(NEXT_BLKP(ptr));
					memcpy(new_ptr, ptr, size - 2 * WSIZE);
					
					PUT(HDRP(new_ptr), PACK(total_size, 1));
					PUT(FTRP(new_ptr), PACK(total_size, 1));
					return new_ptr;
				//}
			}
			return ptr;
		}
		else
		{
			/*�հ� �޺κ��� ����ؼ� ũ�⸦ ��Ƶ� realloc�� ������� �۴ٸ� �׳� free list�� ���鼭 �Ҵ��� ���� ã�´�*/
			new_ptr = mm_malloc(size);
			place(new_ptr, size);
			memcpy(new_ptr, ptr, size - 2 * WSIZE);
			mm_free(ptr);
			return new_ptr;
		}
	}

	return NULL;	
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
 * extend_heap - �������� �÷��ִ� �Լ�
 */
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
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* ���ο� ���ʷα� �������� */

    /* ��ġ�� �������� �Ѿ�� */
    return coalesce(bp);
}


/* 
 * place - ������ ũ�⸸ŭ ���� ������ ���� ���� �� �� �ִ� ���¶�� �������ش�.
 */
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));   
	
    if ((csize - asize) >= (DSIZE + OVERHEAD)) { /* ���� �ְ� ������ ������ ������ free�� �������ش�*/
		PUT(HDRP(bp), PACK(asize, 1));
		PUT(FTRP(bp), PACK(asize, 1));
		delete_free(bp);
		bp = NEXT_BLKP(bp);
		PUT(HDRP(bp), PACK(csize-asize, 0));
		PUT(FTRP(bp), PACK(csize-asize, 0));
		insert_free(bp);	   
     }
    else { /*free�� ������ ������ ����� �������������� �׳� ���θ� �Ҵ����ش�.*/
		PUT(HDRP(bp), PACK(csize, 1));
		PUT(FTRP(bp), PACK(csize, 1));
		delete_free(bp);
    }
}



/*
 * coalesce - boundary tag�� �̿��Ͽ� 4���� ���̽��� �����.

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
	//���� �������� LIFO ������� �������ش�.
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

