#ifndef BPT_H
#define BPT_H
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

/*
1. pagenum는 (page의 갯수 -1)이다.
2. header page의 pagenum는 0이다.
*/

//설정된 order는 32
#define ORDER 32

//페이지 크기 4096Byte
#define PAGE_SIZE 4096

typedef uint64_t pagenum_t;

//FILE_STREAM
FILE* file_pointer;



//트리 print를 위한 queue
struct node_
{
	pagenum_t here;
	struct node* next;
};

typedef struct node_ node;

node* queue = NULL;

//leaf에 사용되는 레코드
typedef struct record
{
	int64_t key;
	char value[120];
} record;
//internal에 사용되는 레코드
typedef struct internal_record
{
	int64_t key;
	pagenum_t page_number;
} internal_record;

typedef struct page_t {
	//공용체 이용 타입 판별
	union pages {
		//헤더페이지의 경우
		struct header_page
		{
			pagenum_t free_page_number;
			pagenum_t root_page_number;
			pagenum_t number_of_pages;
			//pagenum_t의 크기가 바뀌어도 크기 일정하게 조절
			char reserved[4096 - 3 * sizeof(pagenum_t)];
		} header_page;
		//free page의 경우;
		struct free_page
		{
			pagenum_t next_free_page_number;
			char reserved[4096 - sizeof(pagenum_t)];
		} free_page;
		//leaf 페이지의 경우
		struct leaf_page
		{
			pagenum_t parent_page_number;
			int32_t is_leaf;
			int32_t number_of_keys;
			char reserved[120 - sizeof(pagenum_t) - 8];
			pagenum_t right_sibling_page_number;
			record records[31];
		} leaf_page;
		//internal 페이지의 경우
		struct internal_page
		{
			pagenum_t parent_page_number;
			int32_t is_leaf;
			int32_t number_of_keys;
			char reserved[120 - sizeof(pagenum_t) - 8];
			pagenum_t left_most_page_number;
			internal_record internal_record[248];
		} internal_page;
	} base;
} page_t;

typedef struct buffer buffer;
//buffer 구조
struct buffer
{
	page_t frame;
	int64_t table_id;
	pagenum_t page_num;
	bool is_dirty;
	int is_pinned;
	int next_buffer;
	bool ref;
};

//테이블 id 레코드
typedef struct table_ids table_ids;
struct table_ids
{
	int table_id;
	bool used;
	char path_name[50];
};

//테이블 id를 따로 관리하기 위한 pool
struct table_id_pool
{
	//unique한 테이블id를 할당하기 위해 memory상에 id 저장
	table_ids table[101];
	int size;
};

struct table_id_pool table_id_pool;



//버퍼를 구성하는 pool
struct buffer_pool
{
	buffer* buffer_pool;
	int size;
	buffer* selected;
};
struct buffer_pool buffer_pools;




//함수 모음
//1.파일 API들
pagenum_t file_alloc_page(int table_id);
void file_free_page(int table_id, pagenum_t pagenum);
void file_read_page(int table_id, pagenum_t pagenum, page_t* dest);
void file_write_page(int table_id, pagenum_t pagenum, const page_t* src);
void file_write(int table_id){}

//2.library
int open_table(char* pathname);
int db_insert(int table_id, int64_t key, char* value);
int db_find(int table_id, int64_t key, char* ret_val);
int db_delete(int table_id, int64_t key);


//internal page의 페이지 넘버 얻기
pagenum_t get_root_internal_page(int table_id);

//해당 페이지의 height 얻기
int get_height(int table_id, pagenum_t pagenum);

//레코드 만들기
record* make_record(int64_t key, char* value);

//insert to leaf page
pagenum_t insert_to_leaf_page(int table_id, pagenum_t pagenum, record new_record);

//insert to leaf page after split
pagenum_t insert_to_leaf_page_after_split(int table_id, pagenum_t pagenum, record new_record);

//insert to internal page
pagenum_t insert_to_internal_page(int table_id, pagenum_t pagenum, int64_t key, pagenum_t child_pagenum);

//insert to internal page after split
pagenum_t insert_to_internal_page_after_split(int table_id, pagenum_t pagenum, int64_t key, pagenum_t child_pagenum);

//부모 페이지에 삽입
pagenum_t insert_to_parent_page(int table_id, pagenum_t parent_pagenum, int64_t key, pagenum_t child_left, pagenum_t child_right);


/*ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡDeletionㅡㅡㅡㅡㅡ*/
//pagenum이 leftmost일 때 -2 반환
//-1은 leftmost를 나타내는 인덱스이다.
int get_neighbor_index(int table_id, pagenum_t pagenum);
int coalesce(int table_id, pagenum_t pagenum, pagenum_t neighbor, int neigbor_index, int k_prime);
int delete_entry(int table_id, pagenum_t pagenum, int64_t key);
int adjust_root(int table_id, pagenum_t root);
/*ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ*/

//key가 속해있어야 하는 leaf page 찾아서 반환
pagenum_t find_leafpage(int table_id, int64_t key);

//tree print를 위한 노드
void print_tree(int table_id);

//루트페이지까지 길이 반환
int path_to_root(int table_id, pagenum_t child);

//level order를 위한 queue 생성
void enqueue(pagenum_t new_pagenum);

//level order를 위한 deque
pagenum_t dequeue();




/*ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ버퍼 API 시작ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ*/

int init_db(int num_buf);

/*pinned상태를 하나 내려준다*/
void buffer_unpinned(buffer* src);

/*dirty한 상태로 만든다*/
void buffer_set_dirty(buffer* src);

//table_id의 pagenum에 해당하는 버퍼를 찾아 dest에 할당 시킨다.
buffer* buffer_read_page(int table_id, pagenum_t pagenum, buffer* dest);

pagenum_t buffer_alloc_page(int table_id);

void buffer_free_page(int table_id, pagenum_t pagenum);

int close_table(int table_id);

int shutdown_db(void);
/*ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ버퍼 API 끝ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ*/

/*ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ조인 API 시작ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ*/
//Sort_merge 조인
int join_table(int table_id_1, int table_id_2, char* pathname);

//마지막 leafpage 반환
pagenum_t find_first_leafpage(int table_id);

/*ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ조인 API 끝ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ*/
#endif