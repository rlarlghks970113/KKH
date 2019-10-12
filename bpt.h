
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdint.h>
#include <string.h>
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

//���̺� id
int64_t unique_table_id = 0;

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

//in_memory ������
page_t page_data;



//�Լ� ����
//1.���� API��
pagenum_t file_alloc_page();
void file_free_page(pagenum_t pagenum);
void file_read_page(pagenum_t pagenum, page_t* dest);
void file_write_page(pagenum_t pagenum, const page_t* src);

//2.library
int open_table(char* pathname);
int db_insert(int64_t key, char* value);
int db_find(int64_t key, char* ret_val);
int db_delete(int64_t key);

//����
void print()
{
	file_read_page(0, &page_data);

	printf("��� ������ : ��Ʈ -> %lld, ����-> %lld, ���� : %lld\n", page_data.base.header_page.root_page_number, page_data.base.header_page.free_page_number, page_data.base.header_page.number_of_pages);
	file_read_page(2, &page_data);

	printf("������ �ѹ� 2 : ���� ������ -> %d\n", page_data.base.internal_page.parent_page_number);
	printf("is_leaf -> %d\n", page_data.base.internal_page.is_leaf);

}


//internal page�� ������ �ѹ� ���
pagenum_t get_root_internal_page();
//�ش� �������� height ���
int get_height(pagenum_t pagenum);
//���ڵ� �����
record* make_record(int64_t key, char* value);

//insert to leaf page
pagenum_t insert_to_leaf_page(pagenum_t pagenum, record new_record);
//insert to leaf page after split
pagenum_t insert_to_leaf_page_after_split(pagenum_t pagenum, record new_record);

//insert to internal page
pagenum_t insert_to_internal_page(pagenum_t pagenum, int64_t key, pagenum_t child_pagenum);
//insert to internal page after split
pagenum_t insert_to_internal_page_after_split(pagenum_t pagenum, int64_t key, pagenum_t child_pagenum);

//�θ� �������� ����
pagenum_t insert_to_parent_page(pagenum_t parent_pagenum, int64_t key, pagenum_t child_left, pagenum_t child_right);


/*�ѤѤѤѤѤѤѤѤѤѤѤѤ�Deletion�ѤѤѤѤ�*/
//pagenum�� leftmost�� �� -2 ��ȯ
//-1�� leftmost�� ��Ÿ���� �ε����̴�.
int get_neighbor_index(pagenum_t pagenum);

int coalesce(pagenum_t pagenum, pagenum_t neighbor, int neigbor_index, int k_prime);
int delete_entry(pagenum_t pagenum, int64_t key);
int adjust_root(pagenum_t root);
/*�ѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤ�*/


//key�� �����־�� �ϴ� leaf page ã�Ƽ� ��ȯ
pagenum_t find_leafpage(int64_t key);

//tree print�� ���� ���
void print_tree();

//��Ʈ���������� ���� ��ȯ
int path_to_root(pagenum_t child);


//level order�� ���� queue ����
void enqueue(pagenum_t new_pagenum);

//level order�� ���� deque
pagenum_t dequeue();