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
1. pagenum�� (page�� ���� -1)�̴�.
2. header page�� pagenum�� 0�̴�.
*/

//������ order�� 32
#define ORDER 32

//������ ũ�� 4096Byte
#define PAGE_SIZE 4096

typedef uint64_t pagenum_t;

//FILE_STREAM
FILE* file_pointer;



//Ʈ�� print�� ���� queue
struct node_
{
	pagenum_t here;
	struct node* next;
};

typedef struct node_ node;

node* queue = NULL;

//leaf�� ���Ǵ� ���ڵ�
typedef struct record
{
	int64_t key;
	char value[120];
} record;
//internal�� ���Ǵ� ���ڵ�
typedef struct internal_record
{
	int64_t key;
	pagenum_t page_number;
} internal_record;

typedef struct page_t {
	//����ü �̿� Ÿ�� �Ǻ�
	union pages {
		//����������� ���
		struct header_page
		{
			pagenum_t free_page_number;
			pagenum_t root_page_number;
			pagenum_t number_of_pages;
			//pagenum_t�� ũ�Ⱑ �ٲ� ũ�� �����ϰ� ����
			char reserved[4096 - 3 * sizeof(pagenum_t)];
		} header_page;
		//free page�� ���;
		struct free_page
		{
			pagenum_t next_free_page_number;
			char reserved[4096 - sizeof(pagenum_t)];
		} free_page;
		//leaf �������� ���
		struct leaf_page
		{
			pagenum_t parent_page_number;
			int32_t is_leaf;
			int32_t number_of_keys;
			char reserved[120 - sizeof(pagenum_t) - 8];
			pagenum_t right_sibling_page_number;
			record records[31];
		} leaf_page;
		//internal �������� ���
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
//buffer ����
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

//���̺� id ���ڵ�
typedef struct table_ids table_ids;
struct table_ids
{
	int table_id;
	bool used;
	char path_name[50];
};

//���̺� id�� ���� �����ϱ� ���� pool
struct table_id_pool
{
	//unique�� ���̺�id�� �Ҵ��ϱ� ���� memory�� id ����
	table_ids table[101];
	int size;
};

struct table_id_pool table_id_pool;



//���۸� �����ϴ� pool
struct buffer_pool
{
	buffer* buffer_pool;
	int size;
	buffer* selected;
};
struct buffer_pool buffer_pools;




//�Լ� ����
//1.���� API��
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


//internal page�� ������ �ѹ� ���
pagenum_t get_root_internal_page(int table_id);

//�ش� �������� height ���
int get_height(int table_id, pagenum_t pagenum);

//���ڵ� �����
record* make_record(int64_t key, char* value);

//insert to leaf page
pagenum_t insert_to_leaf_page(int table_id, pagenum_t pagenum, record new_record);

//insert to leaf page after split
pagenum_t insert_to_leaf_page_after_split(int table_id, pagenum_t pagenum, record new_record);

//insert to internal page
pagenum_t insert_to_internal_page(int table_id, pagenum_t pagenum, int64_t key, pagenum_t child_pagenum);

//insert to internal page after split
pagenum_t insert_to_internal_page_after_split(int table_id, pagenum_t pagenum, int64_t key, pagenum_t child_pagenum);

//�θ� �������� ����
pagenum_t insert_to_parent_page(int table_id, pagenum_t parent_pagenum, int64_t key, pagenum_t child_left, pagenum_t child_right);


/*�ѤѤѤѤѤѤѤѤѤѤѤѤ�Deletion�ѤѤѤѤ�*/
//pagenum�� leftmost�� �� -2 ��ȯ
//-1�� leftmost�� ��Ÿ���� �ε����̴�.
int get_neighbor_index(int table_id, pagenum_t pagenum);
int coalesce(int table_id, pagenum_t pagenum, pagenum_t neighbor, int neigbor_index, int k_prime);
int delete_entry(int table_id, pagenum_t pagenum, int64_t key);
int adjust_root(int table_id, pagenum_t root);
/*�ѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤ�*/

//key�� �����־�� �ϴ� leaf page ã�Ƽ� ��ȯ
pagenum_t find_leafpage(int table_id, int64_t key);

//tree print�� ���� ���
void print_tree(int table_id);

//��Ʈ���������� ���� ��ȯ
int path_to_root(int table_id, pagenum_t child);

//level order�� ���� queue ����
void enqueue(pagenum_t new_pagenum);

//level order�� ���� deque
pagenum_t dequeue();




/*�ѤѤѤѤѤѤѤѤѤѤѤѹ��� API ���ۤѤѤѤѤѤѤѤѤѤѤѤѤѤ�*/

int init_db(int num_buf);

/*pinned���¸� �ϳ� �����ش�*/
void buffer_unpinned(buffer* src);

/*dirty�� ���·� �����*/
void buffer_set_dirty(buffer* src);

//table_id�� pagenum�� �ش��ϴ� ���۸� ã�� dest�� �Ҵ� ��Ų��.
buffer* buffer_read_page(int table_id, pagenum_t pagenum, buffer* dest);

pagenum_t buffer_alloc_page(int table_id);

void buffer_free_page(int table_id, pagenum_t pagenum);

int close_table(int table_id);

int shutdown_db(void);
/*�ѤѤѤѤѤѤѤѤѤѤѤѹ��� API ���ѤѤѤѤѤѤѤѤѤѤѤѤѤѤ�*/

/*�ѤѤѤѤѤѤѤѤѤѤѤ����� API ���ۤѤѤѤѤѤѤѤѤѤѤѤѤѤѤ�*/
//Sort_merge ����
int join_table(int table_id_1, int table_id_2, char* pathname);

//������ leafpage ��ȯ
pagenum_t find_first_leafpage(int table_id);

/*�ѤѤѤѤѤѤѤѤѤѤѤ����� API ���ѤѤѤѤѤѤѤѤѤѤѤѤѤѤ�*/
#endif