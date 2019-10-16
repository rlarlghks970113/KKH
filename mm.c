/* 
 * 시스템 프로그래밍 동적할당 구현 과제
 *  
 * 학과 : 컴퓨터소프트웨어학과
 * 학번 : 2016024739
 * 이름 : 김기환 
 */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "mm.h"
#include "memlib.h"


/*
 * 구현 방식
 *
 * | header | next | prev | ....payload.... | footer |
 *
 * explicit free list를 순차적으로 만들었다.
 * 
 * 방식은 free_list를 LIFO 방식으로 스택처럼 구현했다.
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



/* 내가 정의한 것들*/

#define GET_NEXT(bp)  (*(char**)(bp))
#define GET_PREV(bp)  (*(char**)(bp + WSIZE))

/* free list들 중 가장 마지막에 삽입된 것을 가르킨다.*/
static char *_free_listp = NULL;

/* free_list에 삽입하는 함수*/
static void insert_free(char* ptr);

/* free_list에서 삭제하는 함수*/
static void delete_free(char* ptr);

/* 내가 정의 한 것 끝*/

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
 * mm_init - 동적할당을 위해 인터페이스를 짜는 함수
 */
/* 앞부분의 프롤로그 헤더를 만드는 부분
 *
 * free_listp의 루트를 프롤로그 헤더부분에 만들어 둔다.
 *
 */
int mm_init(void) 
{
    /* 힙영역을 늘려준다 */
	if (set_and_get_heap_listp(mem_sbrk(6 * WSIZE)) == (void *)-1)
	{
		return -1;
	}
	/* 늘려진 힙영역에 값을 지정해준다*/
   	PUT(get_heap_listp(), 0);                        /* 헷갈리지 않기 위한 padding 부분 */
   	PUT(get_heap_listp() + WSIZE, PACK(DSIZE, 1));  /* 프롤로그 헤더 */
	PUT(get_heap_listp() + WSIZE * 2, PACK(DSIZE, 1)); /* 프롤로그 푸터 */
	PUT(get_heap_listp() + WSIZE * 3, PACK(0, 1));   /* 에필로그 헤더 */

	/* get_heap_listp를 헤더의 바로 뒷자리로 이동시켜줘서 헤더를 가르키게 만든다. */
	set_and_get_heap_listp(get_heap_listp() + 2 * WSIZE);

	_free_listp = get_heap_listp();
	
    /* 힙의 크기를 늘려준다 */
	if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
	{
		return -1;
	}

    return 0;
}

/* 
 * mm_malloc - 힙영역에 메모리를 동적할당하는 역할을 한다.
 *
 */

void *mm_malloc(size_t size) 
{
	int alloc_size;      /* 할당할 사이즈 (=Byte 단위) */
	int extendsize; /* 최소한의 할당 사이즈 (=chunck size보다 크거나 같아야 한다.) */
	char *bp;      

    /* 이상한 size가 들어오면은 null 반환 */
	if (size <= 0) 
	{
		return NULL;
	}
    /* 사이즈가 최소한의 사이즈보다 작으면 최소크기 지정 */
  	if (size <= DSIZE)
	{
		alloc_size = DSIZE + OVERHEAD;
	}
   	else
	{
		alloc_size = DSIZE*((DSIZE + OVERHEAD -1 + size) /DSIZE);
	}
    
    /* 맞는게 있으면 거기다 둔다 */
	if ((bp = find_fit(alloc_size)) != NULL) 
	{
		place(bp, alloc_size);
		return bp;
	}

    /* 맞는게 없으면 heap의 사이즈를 늘린다. */
    extendsize = MAX(alloc_size,CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL) {
		return NULL;
	 }
	/* 힙사이즈를 늘리고 넣는다 */
    place(bp, alloc_size);

    return bp;
} 


/* free_list를 돌면서 크기가 알맞은 free_list의 주소를 반환한다 */
static void *find_fit(size_t asize)
{
	char* ptr;
	/* fee_list를 돈다*/
	for (ptr = _free_listp; GET_ALLOC(HDRP(ptr)) == 0; ptr = GET_NEXT(ptr))
	{
		/* 만약 크기가 맞는게 있다면  반환*/
		if (GET_SIZE(HDRP(ptr)) >= asize)
		{
			return ptr;
		}
	}
	
	/* 크기가 맞는게 없다면 NULL반환 */
	return NULL;   	
}
/* 
 * mm_free - free하는 함수
 */
void mm_free(void *bp)
{
	//주어진 bp포인터 주소에 bp의 원래 사이즈만큼 alloc bit를 0으로 바꿔준다.
	size_t size = GET_SIZE(HDRP(bp));
	PUT(HDRP(bp), PACK(size, 0));
	PUT(FTRP(bp), PACK(size, 0));
	//free가 되었으니 합친다.
	coalesce(bp);
}



/*
 * mm_realloc - realloc과 동일하게 실행된다
 */
void *mm_realloc(void *ptr, size_t size)
{
	/* 원래의 ptr이 가르키는 size*/
	size_t original_size;
	/* 합치기 과정을 위한 new_ptr*/
	void* new_ptr;
	/* 요구한 사이즈에 header, footer의 크기를 집어 넣는다*/
	size += 2 * WSIZE;
	
	/* ptr이 NULL값이면 malloc과 동일*/
	if(ptr == NULL)
	{
		return  mm_malloc(size);
	}
	
	/* size가 0이면 free랑 동일*/
	if(size == 0)
	{
		mm_free(ptr);
		return NULL;
	}
	
	/* ptr이 가르키는 것의 원래 크기*/
	original_size = GET_SIZE(HDRP(ptr));
	
	
	if(original_size >= size) /* 원하는 사이즈가 원래의 사이즈보다 작거나 같은경우*/
	{
		/* 왜 인지는 모르겠으나 이 부분을 넣을 경우 error가 난다.*/
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

			
			/* 사이즈가 작거나 같은 경우 그냥 통채로 반환 해준다*/
			return ptr;

	}
	else if(original_size < size)/* 원하는 사이즈가 원래의 사이즈보다 큰 경우*/
	{	
		
		_Bool prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(ptr))) || ptr == PREV_BLKP(ptr);/* 앞 칸이 alloc 되어 있는지 확인*/
		_Bool next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));/* 뒷 칸이 alloc 되어 있는지 확인*/
		/* total_size == 이용 가능한 크기*/
		size_t total_size = original_size;

		/* 만약 앞이나 뒤칸이 free상태라면은 total_size를 늘려준다*/
		if(!prev_alloc)
		{	
			total_size += GET_SIZE(FTRP(PREV_BLKP(ptr)));
		}
		if(!next_alloc)
		{
			total_size += GET_SIZE(HDRP(NEXT_BLKP(ptr)));
		}
		
		
		if(total_size >= size) /* 이용 가능한 크기가 원래 size보다 커졌다면*/
		{
			if(!prev_alloc && next_alloc) /* 앞칸이 free이고 뒷칸은 not free인 경우 */
			{

				/* 원래 구현은 줄일 사이즈만큼을 따로 배정하고 뒷 부분은 free로 만들 계획이었으나 에러가 나서 포기했습니다*/
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

					
					/*앞 부분을 합친 형태로 반환해준다.*/
					new_ptr = PREV_BLKP(ptr);
					delete_free(PREV_BLKP(ptr));
					 
					memcpy(new_ptr, ptr, size - 2*WSIZE);
					PUT(HDRP(new_ptr), PACK(total_size, 1));
					PUT(FTRP(new_ptr), PACK(total_size, 1));
					return new_ptr;

				
						
			}
			else if(prev_alloc && !next_alloc)
			{
				/*  마찬가지로 앞부분에 할당하고 뒷부분은 free로 지정해주려 했는데 error나서 포기했습니다*/
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

					/* 뒷부분을 합친다*/
					delete_free(NEXT_BLKP(ptr));

					PUT(HDRP(ptr), PACK(total_size, 1));
					PUT(FTRP(ptr), PACK(total_size, 1));
					return ptr;
			//	}
			}
			else if(!prev_alloc && !next_alloc)
			{
				/*앞부분 할당 후 뒷부분 free할려 했으나 에러나서 포기함*/
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

					/* 합친 것을 반환하는 것*/
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
			/*앞과 뒷부분을 고려해서 크기를 잡아도 realloc할 사이즈보다 작다면 그냥 free list를 돌면서 할당할 곳을 찾는다*/
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
 * extend_heap - 힙영역을 늘려주는 함수
 */
static void *extend_heap(size_t words) 
{
    char *bp;
    size_t size;
    _heap_ext_counter++;
	
    /* 홀수인 수가 나왔으면 짝수의 수로 바꿔준다. */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
	if ((bp = mem_sbrk(size)) == (void *)-1)
	{
		return NULL;
	}

    /* header와 footer만 만들고 next와 prev는 colaesce단계에서 다루기로 한다 */
    PUT(HDRP(bp), PACK(size, 0));         /* 헤더 만들기 */
	PUT(FTRP(bp), PACK(size, 0));	      /* 푸터 만들기 */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* 새로운 에필로그 헤더만들기 */

    /* 합치기 과정으로 넘어간다 */
    return coalesce(bp);
}


/* 
 * place - 일정한 크기만큼 넣은 다음에 만약 분할 할 수 있는 상태라면 분할해준다.
 */
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));   
	
    if ((csize - asize) >= (DSIZE + OVERHEAD)) { /* 만약 넣고 나서도 공간이 남으면 free로 지정해준다*/
		PUT(HDRP(bp), PACK(asize, 1));
		PUT(FTRP(bp), PACK(asize, 1));
		delete_free(bp);
		bp = NEXT_BLKP(bp);
		PUT(HDRP(bp), PACK(csize-asize, 0));
		PUT(FTRP(bp), PACK(csize-asize, 0));
		insert_free(bp);	   
     }
    else { /*free로 지정할 공간이 충분히 남아있지않으면 그냥 전부를 할당해준다.*/
		PUT(HDRP(bp), PACK(csize, 1));
		PUT(FTRP(bp), PACK(csize, 1));
		delete_free(bp);
    }
}



/*
 * coalesce - boundary tag를 이용하여 4가지 케이스로 지운다.

 */
static void *coalesce(void *bp)
{
	/*앞 자리와 뒷자리 검사*/

	//bp에서 앞자리로 이동해도 주소 변화가 없는 것은 맨 앞자리이기 때문이다.
	int prev = (GET_ALLOC(FTRP(PREV_BLKP(bp)))) | (PREV_BLKP(bp) == bp);
    //에필로그 헤더가 있으니 맨 끝 상황에서도 잘 작동한다.
	int next = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	size_t size = GET_SIZE(HDRP(bp));
    if (prev && next) {          /* Case 1 : 앞자리와 뒷자리 모두 할당된 상태 일 때*/
		insert_free(bp);
		return bp;
    }
    else if (prev && !next) {      /* Case 2 : 앞자리는 할당, 뒷자리는 할당되지 않은 상태 일 때 */
		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
		delete_free(NEXT_BLKP(bp));
		PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size,0));
		//어차피 next, prev는 그대로 유지되니 놔둔다.
    }
    else if (!prev && next) {      /* Case 3 : 앞자리는 할당 되지않고, 뒷자리는 할당 되었을 때*/
		size += GET_SIZE(HDRP(PREV_BLKP(bp)));
		delete_free(PREV_BLKP(bp));
		PUT(FTRP(bp), PACK(size, 0));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
    }
    else {                                     /* Case 4 : 앞자리와 뒷자리 모두 할당 되지 않았을 때*/
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
	//가장 마지막에 LIFO 방식으로 연결해준다.
	GET_NEXT(ptr) = _free_listp;
	GET_PREV(ptr) = NULL;
	GET_PREV(_free_listp) = ptr;

	_free_listp = ptr;
}

static void delete_free(char* ptr)
{

	//맨 마지막이면 앞의 노드와의 연결을 끊는다.
	if (GET_PREV(ptr) == NULL)
	{
		_free_listp = GET_NEXT(ptr);
		GET_PREV(GET_NEXT(ptr)) = GET_PREV(ptr);
	}
	else//중간이면 뒤의 노드와 앞의 노드를 연결시켜준다.
	{
		GET_NEXT(GET_PREV(ptr)) = GET_NEXT(ptr);
		GET_PREV(GET_NEXT(ptr)) = GET_PREV(ptr);
	}

}

