#ifndef BPT_H
#define BPT_H

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
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

//테이블 id
int64_t unique_table_id = 0;

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

//in_memory 페이지
page_t page_data;



//함수 모음
//1.파일 API들
pagenum_t file_alloc_page();
void file_free_page(pagenum_t pagenum);
void file_read_page(pagenum_t pagenum, page_t* dest);
void file_write_page(pagenum_t pagenum, const page_t* src);

//2.library
int open_table(char* pathname);
int db_insert(int64_t key, char* value);
int db_find(int64_t key, char* ret_val);
int db_delete(int64_t key);

//실험
void print()
{
	file_read_page(0, &page_data);

	printf("헤더 페이지 : 루트 -> %lld, 프리-> %lld, 갯수 : %lld\n", page_data.base.header_page.root_page_number, page_data.base.header_page.free_page_number, page_data.base.header_page.number_of_pages);
	file_read_page(2, &page_data);

	printf("페이지 넘버 2 : 다음 페이지 -> %d\n", page_data.base.internal_page.parent_page_number);
	printf("is_leaf -> %d\n", page_data.base.internal_page.is_leaf);

}


//internal page의 페이지 넘버 얻기
pagenum_t get_root_internal_page();
//해당 페이지의 height 얻기
int get_height(pagenum_t pagenum);
//레코드 만들기
record* make_record(int64_t key, char* value);

//insert to leaf page
pagenum_t insert_to_leaf_page(pagenum_t pagenum, record new_record);
//insert to leaf page after split
pagenum_t insert_to_leaf_page_after_split(pagenum_t pagenum, record new_record);

//insert to internal page
pagenum_t insert_to_internal_page(pagenum_t pagenum, int64_t key, pagenum_t child_pagenum);
//insert to internal page after split
pagenum_t insert_to_internal_page_after_split(pagenum_t pagenum, int64_t key, pagenum_t child_pagenum);

//부모 페이지에 삽입
pagenum_t insert_to_parent_page(pagenum_t parent_pagenum, int64_t key, pagenum_t child_left, pagenum_t child_right);


/*ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡDeletionㅡㅡㅡㅡㅡ*/
//pagenum이 leftmost일 때 -2 반환
//-1은 leftmost를 나타내는 인덱스이다.
int get_neighbor_index(pagenum_t pagenum);

int coalesce(pagenum_t pagenum, pagenum_t neighbor, int neigbor_index, int k_prime);
int delete_entry(pagenum_t pagenum, int64_t key);
int adjust_root(pagenum_t root);
/*ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ*/


//key가 속해있어야 하는 leaf page 찾아서 반환
pagenum_t find_leafpage(int64_t key);

//tree print를 위한 노드
void print_tree();

//루트페이지까지 길이 반환
int path_to_root(pagenum_t child);


//level order를 위한 queue 생성
void enqueue(pagenum_t new_pagenum);

//level order를 위한 deque
pagenum_t dequeue();

/* ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ파일 API ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ*/

pagenum_t file_alloc_page()
{
	if (file_pointer == NULL)
	{
		printf("파일이 열리지 않았습니다.\n");
		return -1;
	}



	pagenum_t first_free_page_number, second_free_page_number;
	//헤더 페이지의 free page 읽기
	file_read_page(0, &page_data);
	first_free_page_number = page_data.base.header_page.free_page_number;

	//만약 다 할당했으면
	if (first_free_page_number == 0)
	{
		printf("프리페이지 추가 할당\n");
		pagenum_t num_of_page = page_data.base.header_page.number_of_pages;

		//헤더페이지가 가르키느 프리페이지 할당
		page_data.base.header_page.free_page_number = num_of_page;
		file_write_page(0, &page_data);
		int i;
		for (i = 1; i < 256 * 10; ++i, ++num_of_page)
		{
			file_read_page(num_of_page, &page_data);
			page_data.base.free_page.next_free_page_number = num_of_page + 1;
			file_write_page(num_of_page, &page_data);
		}
		//마지막 free page가 가르키는건 0으로 한다.
		file_read_page(num_of_page, &page_data);

		page_data.base.free_page.next_free_page_number = 0;
		file_write_page(num_of_page, &page_data);

		file_read_page(0, &page_data);
		page_data.base.header_page.number_of_pages += 256 * 10;
		first_free_page_number = page_data.base.header_page.free_page_number;
		file_write_page(0, &page_data);

	}

	//두번째 free page num 불러오기
	file_read_page(first_free_page_number, &page_data);
	second_free_page_number = page_data.base.free_page.next_free_page_number;

	//헤더 페이지가 second_free_page_number를 가르키게 설정
	file_read_page(0, &page_data);
	page_data.base.header_page.free_page_number = second_free_page_number;
	file_write_page(0, &page_data);

	//할당할 free_page 넘겨주기
	return first_free_page_number;
}

void file_free_page(pagenum_t pagenum)
{

	pagenum_t next_page_num = 0, first_page_number = 0;
	//헤더가 가르키는 첫번째 free page 저장
	file_read_page(0, &page_data);
	first_page_number = page_data.base.header_page.free_page_number;

	//헤더페이지가 가르키는 free page가 pagenum을 가르키게 함
	page_data.base.header_page.free_page_number = pagenum;
	file_write_page(0, &page_data);

	//비어있는 4096Byte의 문자열
	page_t empty;

	file_write_page(pagenum, &empty);

	//해당 페이지가 first_page_number를 가르키게 하기
	file_read_page(pagenum, &page_data);
	page_data.base.free_page.next_free_page_number = first_page_number;
	file_write_page(pagenum, &page_data);


}
void file_read_page(pagenum_t pagenum, page_t* dest)
{
	//해당 페이지에 접근해서 dest에 저장
	fflush(file_pointer);
	fseek(file_pointer, PAGE_SIZE * pagenum, SEEK_SET);
	fread(dest, sizeof(page_t), 1, file_pointer);
}

void file_write_page(pagenum_t pagenum, const page_t* src)
{
	//해당 페이지에 접근해서 src에 저장
	fseek(file_pointer, PAGE_SIZE * pagenum, SEEK_SET);
	fwrite(src, sizeof(page_t), 1, file_pointer);

	fflush(file_pointer);

}
/*ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ파일 API 끝 ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ*/

#endif