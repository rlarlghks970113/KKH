#include "bpt.h"


int open_table(char* pathname)
{
	int64_t file_size, table_id = unique_table_id++;

	file_pointer = fopen(pathname, "rb+");
	//파일포인터 안만들어졌으면
	if (file_pointer == NULL)
	{
		file_pointer = fopen(pathname, "wb+");
		if (file_pointer == NULL)
		{
			return -1;
		}

		//헤더페이지 초기화
		file_read_page(0, &page_data);
		page_data.base.header_page.free_page_number = 1;
		page_data.base.header_page.root_page_number = NULL;
		//할당할 free page수 256 * 10(==10mb) + header page
		page_data.base.header_page.number_of_pages = 256 * 10 + 1;
		file_write_page(0, &page_data);

		//free page만들기
		pagenum_t i;
		for (i = 1; i < 256 * 10; ++i)
		{
			file_read_page(i, &page_data);
			page_data.base.free_page.next_free_page_number = i + 1;
			file_write_page(i, &page_data);
		}

		//마지막 free page가 가르키는건 0으로 한다.
		file_read_page(i, &page_data);

		page_data.base.free_page.next_free_page_number = 0;
		file_write_page(i, &page_data);
	}

	return table_id;
}




//삽입함수
int db_insert(int64_t key, char* value)
{
	pagenum_t root_internal_page = get_root_internal_page();

	//insert가 처음인 경우
	if (root_internal_page == NULL)
	{
		//첫 leaf page 쓰기
		pagenum_t first_leaf_page = file_alloc_page();
		file_read_page(first_leaf_page, &page_data);
		record record_pair = *make_record(key, value);

		page_data.base.leaf_page.is_leaf = 1;
		page_data.base.leaf_page.parent_page_number = NULL;
		page_data.base.leaf_page.number_of_keys = 1;

		page_data.base.leaf_page.records[0] = record_pair;
		page_data.base.leaf_page.right_sibling_page_number = 0;
		file_write_page(first_leaf_page, &page_data);


		//헤더페이지에 루트페이지 입력
		file_read_page(0, &page_data);
		page_data.base.header_page.root_page_number = first_leaf_page;
		file_write_page(0, &page_data);
		return 0;
	}
	//이미 있을 경우
	char* new = (char*)malloc(sizeof(record));
	if (db_find(key, new) == 0) return -1;
	free(new);

	pagenum_t leaf_pagenum = find_leafpage(key);

	file_read_page(leaf_pagenum, &page_data);
	//ORDER - 2 이하 일때(=안 쪼개도 될 때)
	if (page_data.base.leaf_page.number_of_keys < ORDER - 1)
	{
		return insert_to_leaf_page(leaf_pagenum, *make_record(key, value));
	}

	return insert_to_leaf_page_after_split(leaf_pagenum, *make_record(key, value));
}

//insert to leaf page
pagenum_t insert_to_leaf_page(pagenum_t pagenum, record new_record)
{

	file_read_page(pagenum, &page_data);

	//insertion 위치
	int insertion_point = page_data.base.leaf_page.number_of_keys;


	//레코드 삽입
	while (insertion_point > 0)
	{
		if (page_data.base.leaf_page.records[insertion_point - 1].key < new_record.key) break;

		//오른쪽에 복사
		page_data.base.leaf_page.records[insertion_point] = page_data.base.leaf_page.records[insertion_point - 1];
		insertion_point--;
	}
	page_data.base.leaf_page.records[insertion_point] = *make_record(new_record.key, new_record.value);
	(page_data.base.leaf_page.number_of_keys)++;

	file_write_page(pagenum, &page_data);
	return 0;
}

//insert to leaf page after split, return new page
pagenum_t insert_to_leaf_page_after_split(pagenum_t pagenum, record new_record)
{
	int64_t left_last, right_first;
	char value[120];
	int64_t for_insert_key = 0;
	file_read_page(pagenum, &page_data);

	//오른쪽으로 옮길 자료들 복사
	pagenum_t new_parent_pagenum = page_data.base.leaf_page.parent_page_number;
	int32_t new_number_of_keys = ORDER - 1 - cut();
	record* new_right_record = (record*)malloc(sizeof(record)*(ORDER - 1));
	pagenum_t new_right_sibling = page_data.base.leaf_page.right_sibling_page_number;

	int i = 0;
	for (int j = cut(); j < ORDER - 1;)
	{
		new_right_record[i++] = page_data.base.leaf_page.records[j++];
	}


	//왼쪽 데이터 정리
	page_data.base.leaf_page.number_of_keys = cut();
	left_last = page_data.base.leaf_page.records[page_data.base.leaf_page.number_of_keys - 1].key;

	file_write_page(pagenum, &page_data);

	//오른쪽 페이지 만들고 값 쓰기
	pagenum_t new_right_pagenum = file_alloc_page();
	file_read_page(new_right_pagenum, &page_data);
	page_data.base.leaf_page.parent_page_number = new_parent_pagenum;
	page_data.base.leaf_page.is_leaf = 1;
	page_data.base.leaf_page.number_of_keys = new_number_of_keys;
	page_data.base.leaf_page.right_sibling_page_number = new_right_sibling;

	for (int i = 0; i < new_number_of_keys; ++i)
	{
		page_data.base.leaf_page.records[i] = new_right_record[i];
	}

	right_first = page_data.base.leaf_page.records[0].key;
	file_write_page(new_right_pagenum, &page_data);

	//왼쪽페이지로 돌아가서 right sibling 값 조절
	file_read_page(pagenum, &page_data);
	page_data.base.leaf_page.right_sibling_page_number = new_right_pagenum;
	file_write_page(pagenum, &page_data);



	//오른쪽 key값 삽입
	if (left_last < new_record.key)
	{
		file_read_page(new_right_pagenum, &page_data);
		//insertion 위치
		int insertion_point = page_data.base.leaf_page.number_of_keys;

		//레코드 삽입
		while (insertion_point > 0)
		{
			if (page_data.base.leaf_page.records[insertion_point - 1].key < new_record.key) break;

			//오른쪽에 복사
			page_data.base.leaf_page.records[insertion_point] = page_data.base.leaf_page.records[insertion_point - 1];
			insertion_point--;
		}
		page_data.base.leaf_page.records[insertion_point] = new_record;
		page_data.base.leaf_page.number_of_keys++;

		for_insert_key = page_data.base.leaf_page.records[0].key;
		file_write_page(new_right_pagenum, &page_data);
	}
	else if (left_last > new_record.key)//왼쪽 마지막 값 삽입
	{
		file_read_page(pagenum, &page_data);
		//insertion 위치
		int insertion_point = page_data.base.leaf_page.number_of_keys;

		//레코드 삽입
		while (insertion_point > 0)
		{
			if (page_data.base.leaf_page.records[insertion_point - 1].key < new_record.key) break;

			//오른쪽에 복사
			page_data.base.leaf_page.records[insertion_point] = page_data.base.leaf_page.records[insertion_point - 1];
			insertion_point--;
		}
		page_data.base.leaf_page.records[insertion_point] = new_record;
		page_data.base.leaf_page.number_of_keys++;

		int number_of_key = page_data.base.leaf_page.number_of_keys;

		for_insert_key = page_data.base.leaf_page.records[number_of_key - 1].key;
		strcpy(value, page_data.base.leaf_page.records[number_of_key - 1].value);
		(page_data.base.leaf_page.number_of_keys)--;
		file_write_page(pagenum, &page_data);



		//오른쪽에 삽입
		file_read_page(new_right_pagenum, &page_data);
		//insertion 위치
		insertion_point = page_data.base.leaf_page.number_of_keys;

		//레코드 삽입
		while (insertion_point > 0)
		{
			if (page_data.base.leaf_page.records[insertion_point - 1].key < for_insert_key) break;

			//오른쪽에 복사
			page_data.base.leaf_page.records[insertion_point] = page_data.base.leaf_page.records[insertion_point - 1];
			insertion_point--;
		}
		page_data.base.leaf_page.records[insertion_point].key = for_insert_key;
		strcpy(page_data.base.leaf_page.records[insertion_point].value, value);

		page_data.base.leaf_page.number_of_keys++;

		for_insert_key = page_data.base.leaf_page.records[0].key;
		file_write_page(new_right_pagenum, &page_data);
	}




	//쪼개졌으니 부모페이지(=internal page)에 insert
	return insert_to_parent_page(new_parent_pagenum, for_insert_key, pagenum, new_right_pagenum);
}

pagenum_t insert_to_parent_page(pagenum_t parent_pagenum, int64_t key, pagenum_t child_left, pagenum_t child_right)
{
	//루트 페이지 일때
	if (parent_pagenum == NULL)
	{
		pagenum_t new_root_pagenum = file_alloc_page();

		file_read_page(new_root_pagenum, &page_data);
		//새 루트 페이지 값 할당(아직 left_most는 할당 안함)
		page_data.base.internal_page.is_leaf = 0;
		page_data.base.internal_page.number_of_keys = 1;
		page_data.base.internal_page.parent_page_number = NULL;
		page_data.base.internal_page.internal_record[0].key = key;
		page_data.base.internal_page.left_most_page_number = child_left;
		page_data.base.internal_page.internal_record[0].page_number = child_right;
		file_write_page(new_root_pagenum, &page_data);

		//헤더 불러옴
		file_read_page(0, &page_data);
		page_data.base.header_page.root_page_number = new_root_pagenum;
		file_write_page(0, &page_data);

		//자식 페이지들의 부모 페이지 바꿔주기
		file_read_page(child_left, &page_data);
		page_data.base.internal_page.parent_page_number = new_root_pagenum;
		file_write_page(child_left, &page_data);

		file_read_page(child_right, &page_data);
		page_data.base.internal_page.parent_page_number = new_root_pagenum;
		file_write_page(child_right, &page_data);
		return 0;
	}

	file_read_page(parent_pagenum, &page_data);
	int64_t number_of_key = page_data.base.internal_page.number_of_keys;

	//레코드의 갯수가 아직 넣어도 꽉차지 않을 때
	if (number_of_key < ORDER - 1)
	{
		return insert_to_internal_page(parent_pagenum, key, child_right);
	}

	return insert_to_internal_page_after_split(parent_pagenum, key, child_right);
}

//insert to internal page
pagenum_t insert_to_internal_page(pagenum_t pagenum, int64_t key, pagenum_t child_pagenum)
{

	file_read_page(pagenum, &page_data);
	int number_of_keys = page_data.base.internal_page.number_of_keys;

	//일단 insert
	internal_record internal_record = { key, child_pagenum };

	//insertion 위치
	int insertion_point = page_data.base.leaf_page.number_of_keys;

	//레코드 삽입
	while (insertion_point > 0)
	{

		if (page_data.base.internal_page.internal_record[insertion_point - 1].key < key) break;

		//오른쪽에 복사
		page_data.base.internal_page.internal_record[insertion_point] = page_data.base.internal_page.internal_record[insertion_point - 1];
		insertion_point--;
	}
	page_data.base.internal_page.internal_record[insertion_point] = internal_record;

	//데이터 수정
	page_data.base.internal_page.number_of_keys++;

	file_write_page(pagenum, &page_data);

	return 0;
}

//insert to internal page after split
pagenum_t insert_to_internal_page_after_split(pagenum_t pagenum, int64_t key, pagenum_t child_pagenum)
{
	//오른쪽 첫번째, 왼쪽 마지막 저장
	int64_t right_first, left_last;

	file_read_page(pagenum, &page_data);

	//internal page의 경우 왼쪽은 0~ (split-1), 오른쪽은 (split+1)~(ORDER-1)
	//오른쪽 페이지 담을 정보 저장
	int split = cut();
	pagenum_t right_parent_page = page_data.base.internal_page.parent_page_number;

	int32_t right_number_of_key = ORDER - 1 - split;
	internal_record right_internal_records[ORDER - 1];

	int j = 0;
	for (int i = split; i <= ORDER - 1; ++i)
	{
		right_internal_records[j++] = page_data.base.internal_page.internal_record[i];
	}

	left_last = page_data.base.internal_page.internal_record[split - 1].key;
	//왼쪽 페이지 수정 후 저장
	page_data.base.internal_page.number_of_keys = split;
	file_write_page(pagenum, &page_data);


	//오른쪽 페이지 할당
	pagenum_t new_right_page = file_alloc_page();
	file_read_page(new_right_page, &page_data);
	page_data.base.internal_page.is_leaf = 0;
	page_data.base.internal_page.parent_page_number = right_parent_page;
	page_data.base.internal_page.number_of_keys = right_number_of_key;

	for (int i = 0; i < right_number_of_key; ++i)
	{
		page_data.base.internal_page.internal_record[i] = right_internal_records[i];
	}
	right_first = right_internal_records[0].key;
	file_write_page(new_right_page, &page_data);


	//insert 부분
	file_read_page(new_right_page, &page_data);

	pagenum_t right_left_most_pagenum;
	pagenum_t for_insert_to_parent;
	if (key == left_last || key == right_first)
	{
		print();
		return -1;
	}
	if (left_last < key && key < right_first)
	{
		//가운데 일 때
		right_left_most_pagenum = child_pagenum;
		for_insert_to_parent = key;


	}
	else if (left_last > key)
	{
		//맨 왼 쪽 일 때
		file_read_page(pagenum, &page_data);
		internal_record internal_record_ = { key, child_pagenum };

		int insertion_point = page_data.base.internal_page.number_of_keys;
		while (insertion_point > 0)
		{
			if (page_data.base.internal_page.internal_record[insertion_point - 1].key < key) break;
			page_data.base.internal_page.internal_record[insertion_point] = page_data.base.internal_page.internal_record[insertion_point - 1];
			insertion_point--;
		}

		//맞는 위치에 삽입
		page_data.base.internal_page.internal_record[insertion_point] = internal_record_;
		//갯수 올리기
		page_data.base.internal_page.number_of_keys++;

		right_left_most_pagenum = page_data.base.internal_page.internal_record[page_data.base.internal_page.number_of_keys - 1].page_number;
		//맨 오른쪽 값 선택
		for_insert_to_parent = page_data.base.internal_page.internal_record[page_data.base.internal_page.number_of_keys - 1].key;
		page_data.base.internal_page.number_of_keys--;
		file_write_page(pagenum, &page_data);
	}
	else if (right_first < key)
	{
		//맨 오른쪽 일 때
		internal_record internal_record_ = { key, child_pagenum };

		int insertion_point = page_data.base.internal_page.number_of_keys;
		while (insertion_point > 0)
		{
			if (page_data.base.internal_page.internal_record[insertion_point - 1].key < key) break;
			page_data.base.internal_page.internal_record[insertion_point] = page_data.base.internal_page.internal_record[insertion_point - 1];

			insertion_point--;
		}

		//맞는 위치에 삽입
		page_data.base.internal_page.internal_record[insertion_point] = internal_record_;
		//갯수 올리기
		page_data.base.internal_page.number_of_keys++;

		right_left_most_pagenum = page_data.base.internal_page.internal_record[0].page_number;
		for_insert_to_parent = page_data.base.internal_page.internal_record[0].key;

		//맨 첫번째값 빼기
		for (int i = 0; i < page_data.base.internal_page.number_of_keys - 1; ++i)
		{
			page_data.base.internal_page.internal_record[i] = page_data.base.internal_page.internal_record[i + 1];
		}
		page_data.base.internal_page.number_of_keys--;
		file_write_page(new_right_page, &page_data);
	}

	//오른쪽 페이지 left most page 입력
	file_read_page(new_right_page, &page_data);
	page_data.base.internal_page.left_most_page_number = right_left_most_pagenum;
	right_number_of_key = page_data.base.internal_page.number_of_keys;
	file_write_page(new_right_page, &page_data);

	//오른쪽 페이지의 자식 페이지들 부모페이지 바꿔주기

	file_read_page(right_left_most_pagenum, &page_data);
	page_data.base.internal_page.parent_page_number = new_right_page;
	file_write_page(right_left_most_pagenum, &page_data);


	for (int i = 0; i < right_number_of_key; ++i)
	{
		file_read_page(new_right_page, &page_data);
		pagenum_t next = page_data.base.internal_page.internal_record[i].page_number;
		file_read_page(next, &page_data);
		page_data.base.internal_page.parent_page_number = new_right_page;
		file_write_page(next, &page_data);
	}


	//쪼개졌으니 부모페이지(=internal page)에 insert
	return insert_to_parent_page(right_parent_page, for_insert_to_parent, pagenum, new_right_page);
}

//놔눌 경우 오른쪽 배열의 첫칸 반환
int cut()
{
	if (ORDER % 2 == 0)
	{
		return ORDER / 2;
	}
	else
	{
		return ORDER / 2;
	}
}

//key가 속해있어야 하는 leaf page 찾아서 반환
pagenum_t find_leafpage(int64_t key)
{
	pagenum_t root = get_root_internal_page();

	while (1)
	{
		file_read_page(root, &page_data);
		int is_leaf = page_data.base.internal_page.is_leaf;
		int32_t number_of_key = page_data.base.internal_page.number_of_keys;
		//리프 페이지가 맞다면
		if (is_leaf == 1)
		{
			return root;
		}
		else//리프페이지 아닐 때
		{
			//left most 인경우
			if (key < page_data.base.internal_page.internal_record[0].key)
			{
				root = page_data.base.internal_page.left_most_page_number;
			}
			else
			{
				int i;
				for (i = 0; i < number_of_key - 1; ++i)
				{
					if (page_data.base.internal_page.internal_record[i + 1].key > key)
					{
						root = page_data.base.internal_page.internal_record[i].page_number;
						break;
					}
				}
				if (i == number_of_key - 1) root = page_data.base.internal_page.internal_record[i].page_number;
			}
		}

	}
	//찾기 실패
	return -1;
}



//레코드 만들기
record *make_record(int64_t key, char* value)
{
	record* new_record = (char*)malloc(sizeof(record));

	new_record->key = key;
	strcpy(new_record->value, value);
	return new_record;
}



//internal root page의 페이지 넘버 얻기
pagenum_t get_root_internal_page()
{
	pagenum_t root;
	//헤더 페이지 읽기
	file_read_page(0, &page_data);

	root = page_data.base.header_page.root_page_number;
	return root;
}

//해당 페이지의 height 얻기
int get_height(pagenum_t pagenum)
{
	pagenum_t root_pagenum = get_root_internal_page();
	int height = 0;

	while (root_pagenum != pagenum)
	{
		//root 페이지 넘버랑 다르면 읽고
		file_read_page(pagenum, &page_data);

		//부모 페이지 넘버로 이동
		pagenum = page_data.base.internal_page.parent_page_number;
		height++;
	}
	return height;
}



int db_find(int64_t key, char* ret_val)
{


	pagenum_t root = get_root_internal_page();

	while (1)
	{
		file_read_page(root, &page_data);
		int is_leaf = page_data.base.internal_page.is_leaf;
		int32_t number_of_key = page_data.base.internal_page.number_of_keys;
		//리프 페이지가 맞다면
		if (is_leaf == 1)
		{
			//리프페이지 안에서 key값 찾기
			int i;
			for (i = 0; i < number_of_key; ++i)
			{
				//찾는게 있을 경우
				if (page_data.base.leaf_page.records[i].key == key)
				{
					strcpy(ret_val, page_data.base.leaf_page.records[i].value);
					return 0;
				}
			}
			//찾는 값이 없을 경우
			return -1;
		}
		else//리프페이지 아닐 때
		{
			//left most page 일 때
			if (key < page_data.base.internal_page.internal_record[0].key)
			{
				root = page_data.base.internal_page.left_most_page_number;

			}
			else
			{
				int i;
				for (i = 0; i < number_of_key - 1; ++i)
				{

					if (page_data.base.internal_page.internal_record[i + 1].key > key)
					{
						root = page_data.base.internal_page.internal_record[i].page_number;
						break;
					}

				}
				if (i == number_of_key - 1) root = page_data.base.internal_page.internal_record[i].page_number;
			}
		}

	}
	//찾기 실패
	return -1;
}

//마스터 딜리션
int db_delete(int64_t key)
{
	pagenum_t root_internal_page = get_root_internal_page();

	//아직 root도 생성되지 않았을 경우
	if (root_internal_page == NULL)
	{
		return -1;
	}
	//값이 없을 경우
	char* new = (char*)malloc(sizeof(record));
	if (db_find(key, new) != 0) return -1;
	free(new);

	pagenum_t leaf_pagenum = find_leafpage(key);

	return delete_entry(leaf_pagenum, key);
}

int delete_entry(pagenum_t pagenum, int64_t key)
{
	file_read_page(pagenum, &page_data);

	//리프 페이지 일 때 삭제
	if (page_data.base.internal_page.is_leaf == 1)
	{
		//leaf 페이지의 키 갯수
		int32_t num_of_key = page_data.base.leaf_page.number_of_keys;
		int deletion_point;
		for (deletion_point = 0; deletion_point < num_of_key; ++deletion_point)
		{
			//해당 키 만나면 삭제하고 빠져나오기
			if (page_data.base.leaf_page.records[deletion_point].key == key)
			{
				for (int i = deletion_point; i < num_of_key - 1; ++i)
				{
					page_data.base.leaf_page.records[i] = page_data.base.leaf_page.records[i + 1];
				}
				page_data.base.leaf_page.number_of_keys--;

				file_write_page(pagenum, &page_data);
				break;
			}
		}
	}
	else //인터널페이지일 때 삭제
	{
		//leaf 페이지의 키 갯수
		int32_t num_of_key = page_data.base.internal_page.number_of_keys;
		int deletion_point;
		for (deletion_point = 0; deletion_point < num_of_key; ++deletion_point)
		{
			//해당 키 만나면 삭제하고 빠져나오기
			if (page_data.base.internal_page.internal_record[deletion_point].key == key)
			{
				//	if (deletion_point == 0)
					//{
				//		page_data.base.internal_page.left_most_page_number = page_data.base.internal_page.internal_record[0].page_number;
				//	}

				for (int i = deletion_point; i < num_of_key - 1; ++i)
				{
					page_data.base.internal_page.internal_record[i] = page_data.base.internal_page.internal_record[i + 1];
				}
				page_data.base.internal_page.number_of_keys--;
				file_write_page(pagenum, &page_data);

				break;
			}
		}
	}


	//루트일 경우

	file_read_page(pagenum, &page_data);

	if (page_data.base.internal_page.parent_page_number == NULL)
	{
		return adjust_root(pagenum);
	}


	//underflow일 때
	pagenum_t parent_page, neighbor_page;
	int32_t neighbor_index;
	int32_t k_prime_index, k_prime;
	if (page_data.base.leaf_page.number_of_keys < 1)
	{
		neighbor_index = get_neighbor_index(pagenum);
		k_prime_index = neighbor_index == -1 ? 0 : neighbor_index + 1;
		if (neighbor_index == -2)
		{
			k_prime_index = 0;
		}
		//부모페이지 얻기
		file_read_page(pagenum, &page_data);
		parent_page = page_data.base.internal_page.parent_page_number;
		file_read_page(parent_page, &page_data);
		k_prime = page_data.base.internal_page.internal_record[k_prime_index].key;
		if (neighbor_index == -2)
		{
			neighbor_page = page_data.base.internal_page.internal_record[0].page_number;

		}
		else if (neighbor_index == -1)
		{
			neighbor_page = page_data.base.internal_page.left_most_page_number;
		}
		else
		{
			neighbor_page = page_data.base.internal_page.internal_record[neighbor_index].page_number;
		}

		return coalesce(pagenum, neighbor_page, neighbor_index, k_prime);
	}
	else //not underflow일 때
	{
		int64_t for_change_key = page_data.base.leaf_page.records[0].key;
		pagenum_t parent_page = page_data.base.leaf_page.parent_page_number;
		file_write_page(pagenum, &page_data);

		//더 이상 같은 값 없을 때 0
		int find = 1;
		while (find == 1)
		{
			//부모 노드들 탐색하면서 값 변경
			file_read_page(parent_page, &page_data);
			int32_t num_of_key = page_data.base.internal_page.number_of_keys;

			//같은 값 있으면 변경
			for (int i = 0; i < num_of_key; ++i)
			{
				if (page_data.base.internal_page.internal_record[i].key == key)
				{
					page_data.base.internal_page.internal_record[i].key = for_change_key;
					break;
				}

				if (i == num_of_key - 1) find = 0;
			}

			file_write_page(parent_page, &page_data);
			parent_page = page_data.base.internal_page.parent_page_number;
			if (parent_page == NULL) break;
		}
		return 0;
	}
}

int adjust_root(pagenum_t root)
{
	file_read_page(root, &page_data);

	if (page_data.base.internal_page.number_of_keys > 0)
	{
		return 0;
	}

	//empty 인 경우
	if (!page_data.base.leaf_page.is_leaf)
	{
		pagenum_t newroot = page_data.base.internal_page.left_most_page_number;
		file_read_page(0, &page_data);
		page_data.base.header_page.root_page_number = newroot;
		file_write_page(0, &page_data);

		file_read_page(newroot, &page_data);
		page_data.base.internal_page.parent_page_number = NULL;
		file_write_page(newroot, &page_data);
		file_free_page(root);
	}
	else
	{

	}
	return 0;
}

int coalesce(pagenum_t pagenum, pagenum_t neighbor, int neigbor_index, int k_prime)
{
	int i, j, neighbor_insertion_index, n_end;
	pagenum_t parent;

	//page가 leftmost child이면
	if (neigbor_index == -2)
	{
		//tmp = n
		pagenum_t temp = neighbor;
		neighbor = pagenum;
		pagenum = temp;
	}
	file_read_page(neighbor, &page_data);
	neighbor_insertion_index = page_data.base.internal_page.number_of_keys;
	parent = page_data.base.internal_page.parent_page_number;

	//리프페이지가 아니면
	if (page_data.base.internal_page.is_leaf == 0)
	{
		if (neigbor_index == -2)
		{
			page_t n;
			file_read_page(pagenum, &n);
			file_read_page(neighbor, &page_data);
			page_data.base.internal_page.internal_record[0].key = k_prime;
			page_data.base.internal_page.internal_record[0].page_number = n.base.internal_page.left_most_page_number;
			page_data.base.internal_page.number_of_keys++;

			for (int i = 0; i < n.base.internal_page.number_of_keys; ++i)
			{
				page_data.base.internal_page.internal_record[i + 1] = n.base.internal_page.internal_record[i];
				page_data.base.internal_page.number_of_keys++;
			}

			file_write_page(neighbor, &page_data);

			file_read_page(n.base.internal_page.left_most_page_number, &page_data);
			page_data.base.internal_page.parent_page_number = neighbor;
			file_write_page(n.base.internal_page.left_most_page_number, &page_data);

			for (int i = 0; i < n.base.internal_page.number_of_keys; ++i)
			{
				file_read_page(n.base.internal_page.internal_record[i].page_number, &page_data);
				page_data.base.internal_page.parent_page_number = neighbor;
				file_write_page(n.base.internal_page.internal_record[i].page_number, &page_data);
			}
		}
		else
		{

			//neighbor, pagenum
			page_data.base.internal_page.internal_record[neighbor_insertion_index].key = k_prime;
			file_write_page(neighbor, &page_data);

			file_read_page(pagenum, &page_data);
			pagenum_t left_most = page_data.base.internal_page.left_most_page_number;


			file_read_page(neighbor, &page_data);
			page_data.base.internal_page.internal_record[neighbor_insertion_index].page_number = left_most;
			page_data.base.internal_page.number_of_keys++;
			file_write_page(neighbor, &page_data);

			//left most page가 네이버를 가르키도록 설정
			file_read_page(left_most, &page_data);
			page_data.base.internal_page.parent_page_number = neighbor;
			file_write_page(left_most, &page_data);
		}

	}
	else
	{
		file_read_page(pagenum, &page_data);
		int32_t num_of_key = page_data.base.leaf_page.number_of_keys;

		page_t n;
		file_read_page(neighbor, &page_data);
		file_read_page(pagenum, &n);

		for (i = neighbor_insertion_index, j = 0; j < num_of_key; i++, j++) {
			page_data.base.leaf_page.records[i] = n.base.leaf_page.records[j];
			page_data.base.leaf_page.number_of_keys++;
		}
		page_data.base.leaf_page.right_sibling_page_number = n.base.leaf_page.right_sibling_page_number;
		file_write_page(neighbor, &page_data);
	}


	file_free_page(pagenum);
	return delete_entry(parent, k_prime);

}

//pagenum이 leftmost일 때 -2 반환
//-1은 leftmost를 나타내는 인덱스이다.
int get_neighbor_index(pagenum_t pagenum)
{
	file_read_page(pagenum, &page_data);
	pagenum_t parent_page = page_data.base.internal_page.parent_page_number;
	file_read_page(parent_page, &page_data);
	int i;
	for (i = 0; i < page_data.base.internal_page.number_of_keys; i++)
	{
		if (page_data.base.internal_page.internal_record[i].page_number == pagenum)
			return i - 1;
	}
	return -2;
}



//루트페이지까지 길이 반환
int path_to_root(pagenum_t child)
{
	int length = 0;
	pagenum_t root_page = get_root_internal_page();

	while (child != root_page) {
		file_read_page(child, &page_data);

		child = page_data.base.internal_page.parent_page_number;
		length++;
	}
	return length;
}

//level order를 위한 queue 생성
void enqueue(pagenum_t new_pagenum)
{
	if (queue == NULL)
	{
		queue = (node*)malloc(sizeof(node));
		queue->here = new_pagenum;
		queue->next = NULL;
	}
	else
	{
		node* temp = queue;
		while (temp->next != NULL)
		{
			temp = temp->next;
		}
		temp->next = (node*)malloc(sizeof(node));
		temp = temp->next;

		temp->here = new_pagenum;
		temp->next = NULL;
	}
}

//level order를 위한 deque
pagenum_t dequeue()
{
	node * n = queue;
	queue = queue->next;
	n->next = NULL;
	return n->here;
}

//print를 위한 트리
void print_tree()
{
	pagenum_t page = get_root_internal_page();
	int rank = 0;
	int new_rank = 0;

	enqueue(page);
	while (queue != NULL)
	{
		page = dequeue();

		file_read_page(page, &page_data);
		if (page_data.base.internal_page.parent_page_number != NULL) {
			new_rank = path_to_root(page);
			if (new_rank != rank) {
				rank = new_rank;
				printf("\n");
			}
		}

		//해당 페이지 읽고 print
		file_read_page(page, &page_data);
		int32_t number_of_record = page_data.base.internal_page.number_of_keys;
		if (number_of_record == 0) continue;
		//internal page 일 때
		if (page_data.base.internal_page.is_leaf == 0)
		{
			enqueue(page_data.base.internal_page.left_most_page_number);
			for (int i = 0; i < number_of_record; ++i)
			{
				enqueue(page_data.base.internal_page.internal_record[i].page_number);

				printf("%lld ", page_data.base.internal_page.internal_record[i].key);
			}
			printf("{%lld}|", page);
		}
		else//리프페이지 일 때
		{
			for (int i = 0; i < number_of_record; ++i)
			{
				printf("%lld ", page_data.base.leaf_page.records[i].key);
			}
			printf("{%lld}|", page);
		}
	}
	printf("\n\n");
}







int main()
{
	srand(time(NULL));
	int table_id = open_table("file");

	page_t a;
	printf("%d\n", sizeof(a.base.free_page));
	printf("%d\n", sizeof(a.base.header_page));
	printf("%d\n", sizeof(a.base.internal_page));
	printf("%d\n", sizeof(a.base.leaf_page));

	printf("테이블 아이디 %d\n", table_id);

	int input_key;
	char* value = (char*)malloc(sizeof(char) * 120);
	char instruction = ' ';
	while (instruction != 'q')
	{
		scanf("%c", &instruction);
		switch (instruction)
		{
		case 'i':
			for (int i = 0; i < 1000; ++i)
			{
				//scanf("%d", &input_key);
				printf("%d\n", i);
				input_key = rand() % 1000000 + 1;
				db_insert(input_key, "ad");


			}
			print_tree();


			break;
		case 'f':
			scanf("%d", &input_key);

			printf("성공했을까? -> %d\n", db_find(input_key, value));
			printf("반환된 value : %s\n", value);
			break;
		case 'd':
			//scanf("%d", &input_key);

		//	for (int i = 0; i < 100; ++i)
			//{
			scanf("%d", &input_key);
			//input_key = rand() % 100 + 1;
			db_delete(input_key);
			printf("delete %d\n", input_key);
			print_tree();


			//				}
			break;
		case'p':
			print_tree();
		default:
			break;
		}


	}

	return 0;
}