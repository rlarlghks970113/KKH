#include "bpt.h"

//FILE_STREAM
FILE* file_pointer;

node* queue = NULL;

int open_table(char* pathname)
{
	buffer* cache = NULL;
	int64_t file_size;
	int table_id = 0;
	int i;
	
	//테이블 id pool 한번 돌려서 찾기
	for (int i = 1; i <= table_id_pool.size; ++i)
	{
		if (strcmp(table_id_pool.table[i].path_name , pathname) == 0&& table_id_pool.table[i].used == true)
		{
			table_id = i;
			return table_id;
		}
	}

	//테이블 아이디 풀에 없으면 삽입
	if (table_id == 0)
	{
		int i;
		for (i = 1; i < table_id_pool.size; ++i)
		{
			if (table_id_pool.table[i].used == false)
			{
				strcpy(table_id_pool.table[i].path_name, pathname);
				table_id_pool.table[i].table_id = i;
				table_id_pool.table[i].used = true;
				break;
			}
		}
		table_id = i;
	}

	//fclose(file_pointer);
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
		cache = buffer_read_page(table_id, 0, cache);
		cache->frame.base.header_page.free_page_number = 1;
		cache->frame.base.header_page.root_page_number = NULL;
		//할당할 free page수 256 * 10(==10mb) + header page
		cache->frame.base.header_page.number_of_pages = 256 * 10 + 1;
		cache->ref = false;
		buffer_set_dirty(cache);
		buffer_unpinned(cache);

		//free page만들기
		pagenum_t i;
		for (i = 1; i < 256 * 10; ++i)
		{
			cache = buffer_read_page(table_id, i, cache);
			cache->frame.base.free_page.next_free_page_number = i + 1;
			cache->ref = false;
			buffer_set_dirty(cache);
			buffer_unpinned(cache);
		}
		//마지막 free page가 가르키는건 0으로 한다.
		cache = buffer_read_page(table_id, i, cache);
		cache->frame.base.free_page.next_free_page_number = 0;
		cache->ref = false;
		buffer_set_dirty(cache);
		buffer_unpinned(cache);
	}

	fclose(file_pointer);
	return table_id;
}

//삽입함수
int db_insert(int table_id, int64_t key, char* value, int trx_id)
{
	buffer* cache = NULL;

	pagenum_t root_internal_page = get_root_internal_page(table_id);
	//insert가 처음인 경우
	if (root_internal_page == NULL)
	{
		//첫 leaf page 쓰기
		pagenum_t first_leaf_page = buffer_alloc_page(table_id);
		cache = buffer_read_page(table_id, first_leaf_page, cache);
		record record_pair = *make_record(key, value);
		cache->frame.base.leaf_page.is_leaf = 1;
		cache->frame.base.leaf_page.parent_page_number = NULL;
		cache->frame.base.leaf_page.number_of_keys = 1;
		cache->frame.base.leaf_page.records[0] = record_pair;
		cache->frame.base.leaf_page.right_sibling_page_number = 0;
		buffer_set_dirty(cache);
		buffer_unpinned(cache);

		//헤더페이지에 루트페이지 입력
		cache = buffer_read_page(table_id, 0, cache);
		cache->frame.base.header_page.root_page_number = first_leaf_page;
		buffer_set_dirty(cache);
		buffer_unpinned(cache);
		return 0;
	}
	//이미 있을 경우
	char* new_ = (char*)malloc(sizeof(record));
	if (db_find(table_id, key, new_, trx_id) == 0) return -1;
	free(new_);
	pagenum_t leaf_pagenum = find_leafpage(table_id, key);

	cache = buffer_read_page(table_id, leaf_pagenum, cache);
	//ORDER - 2 이하 일때(=안 쪼개도 될 때)
	if (cache->frame.base.leaf_page.number_of_keys < ORDER - 1)
	{
		buffer_unpinned(cache);
		return insert_to_leaf_page(table_id, leaf_pagenum, *make_record(key, value));
	}

	buffer_unpinned(cache);
	return insert_to_leaf_page_after_split(table_id, leaf_pagenum, *make_record(key, value));
}

//insert to leaf page
pagenum_t insert_to_leaf_page(int table_id, pagenum_t pagenum, record new_record)
{
	buffer* cache = NULL;

	cache = buffer_read_page(table_id, pagenum, cache);
	//insertion 위치
	int insertion_point = cache->frame.base.leaf_page.number_of_keys;
	//레코드 삽입
	while (insertion_point > 0)
	{
		if (cache->frame.base.leaf_page.records[insertion_point - 1].key < new_record.key) break;
		//오른쪽에 복사
		cache->frame.base.leaf_page.records[insertion_point] = cache->frame.base.leaf_page.records[insertion_point - 1];
		insertion_point--;
	}
	cache->frame.base.leaf_page.records[insertion_point] = *make_record(new_record.key, new_record.value);
	(cache->frame.base.leaf_page.number_of_keys)++;

	buffer_set_dirty(cache);
	buffer_unpinned(cache);
	return 0;
}
//insert to leaf page after split, return new page
pagenum_t insert_to_leaf_page_after_split(int table_id, pagenum_t pagenum, record new_record)
{
	buffer* cache = NULL;
	int64_t left_last, right_first;
	char value[120];
	int64_t for_insert_key = 0;

	cache = buffer_read_page(table_id, pagenum, cache);
	//오른쪽으로 옮길 자료들 복사
	pagenum_t new_parent_pagenum = cache->frame.base.leaf_page.parent_page_number;
	int32_t new_number_of_keys = ORDER - 1 - cut();
	record* new_right_record = (record*)malloc(sizeof(record)*(ORDER - 1));
	pagenum_t new_right_sibling = cache->frame.base.leaf_page.right_sibling_page_number;
	int i = 0;
	for (int j = cut(); j < ORDER - 1;)
	{
		new_right_record[i++] = cache->frame.base.leaf_page.records[j++];
	}
	//왼쪽 데이터 정리
	cache->frame.base.leaf_page.number_of_keys = cut();
	left_last = cache->frame.base.leaf_page.records[cache->frame.base.leaf_page.number_of_keys - 1].key;
	buffer_set_dirty(cache);
	buffer_unpinned(cache);

	//오른쪽 페이지 만들고 값 쓰기
	pagenum_t new_right_pagenum = buffer_alloc_page(table_id);

	cache = buffer_read_page(table_id, new_right_pagenum, cache);
	cache->frame.base.leaf_page.parent_page_number = new_parent_pagenum;
	cache->frame.base.leaf_page.is_leaf = 1;
	cache->frame.base.leaf_page.number_of_keys = new_number_of_keys;
	cache->frame.base.leaf_page.right_sibling_page_number = new_right_sibling;
	for (int i = 0; i < new_number_of_keys; ++i)
	{
		cache->frame.base.leaf_page.records[i] = new_right_record[i];
	}
	right_first = cache->frame.base.leaf_page.records[0].key;
	buffer_set_dirty(cache);
	buffer_unpinned(cache);

	//왼쪽페이지로 돌아가서 right sibling 값 조절
	cache = buffer_read_page(table_id, pagenum, cache);
	cache->frame.base.leaf_page.right_sibling_page_number = new_right_pagenum;
	buffer_set_dirty(cache);
	buffer_unpinned(cache);

	//오른쪽 key값 삽입
	if (left_last < new_record.key)
	{
		cache = buffer_read_page(table_id, new_right_pagenum, cache);
		//insertion 위치
		int insertion_point = cache->frame.base.leaf_page.number_of_keys;
		//레코드 삽입
		while (insertion_point > 0)
		{
			if (cache->frame.base.leaf_page.records[insertion_point - 1].key < new_record.key) break;
			//오른쪽에 복사
			cache->frame.base.leaf_page.records[insertion_point] = cache->frame.base.leaf_page.records[insertion_point - 1];
			insertion_point--;
		}
		cache->frame.base.leaf_page.records[insertion_point] = new_record;
		cache->frame.base.leaf_page.number_of_keys++;
		for_insert_key = cache->frame.base.leaf_page.records[0].key;
		buffer_set_dirty(cache);
		buffer_unpinned(cache);
	}
	else if (left_last > new_record.key)//왼쪽 마지막 값 삽입
	{
		
		cache = buffer_read_page(table_id, pagenum, cache);
		//insertion 위치
		int insertion_point = cache->frame.base.leaf_page.number_of_keys;
		//레코드 삽입
		while (insertion_point > 0)
		{
			if (cache->frame.base.leaf_page.records[insertion_point - 1].key < new_record.key) break;
			//오른쪽에 복사
			cache->frame.base.leaf_page.records[insertion_point] = cache->frame.base.leaf_page.records[insertion_point - 1];
			insertion_point--;
		}
		cache->frame.base.leaf_page.records[insertion_point] = new_record;
		cache->frame.base.leaf_page.number_of_keys++;
		int number_of_key = cache->frame.base.leaf_page.number_of_keys;
		for_insert_key = cache->frame.base.leaf_page.records[number_of_key - 1].key;
		strcpy(value, cache->frame.base.leaf_page.records[number_of_key - 1].value);
		(cache->frame.base.leaf_page.number_of_keys)--;
		buffer_set_dirty(cache);
		buffer_unpinned(cache);

		//오른쪽에 삽입
		cache = buffer_read_page(table_id, new_right_pagenum, cache);
		//insertion 위치
		insertion_point = cache->frame.base.leaf_page.number_of_keys;
		//레코드 삽입
		while (insertion_point > 0)
		{
			if (cache->frame.base.leaf_page.records[insertion_point - 1].key < for_insert_key) break;
			//오른쪽에 복사
			cache->frame.base.leaf_page.records[insertion_point] = cache->frame.base.leaf_page.records[insertion_point - 1];
			insertion_point--;
		}
		cache->frame.base.leaf_page.records[insertion_point].key = for_insert_key;
		strcpy(cache->frame.base.leaf_page.records[insertion_point].value, value);
		cache->frame.base.leaf_page.number_of_keys++;
		for_insert_key = cache->frame.base.leaf_page.records[0].key;
		buffer_set_dirty(cache);
		buffer_unpinned(cache);
	}
	//쪼개졌으니 부모페이지(=internal page)에 insert
	return insert_to_parent_page(table_id, new_parent_pagenum, for_insert_key, pagenum, new_right_pagenum);
}
pagenum_t insert_to_parent_page(int table_id, pagenum_t parent_pagenum, int64_t key, pagenum_t child_left, pagenum_t child_right)
{
	buffer* cache = NULL;

	//루트 페이지 일때
	if (parent_pagenum == NULL)
	{
		pagenum_t new_root_pagenum = buffer_alloc_page(table_id);
		cache = buffer_read_page(table_id, new_root_pagenum, cache);
		//새 루트 페이지 값 할당(아직 left_most는 할당 안함)
		cache->frame.base.internal_page.is_leaf = 0;
		cache->frame.base.internal_page.number_of_keys = 1;
		cache->frame.base.internal_page.parent_page_number = NULL;
		cache->frame.base.internal_page.internal_record[0].key = key;
		cache->frame.base.internal_page.left_most_page_number = child_left;
		cache->frame.base.internal_page.internal_record[0].page_number = child_right;
		buffer_set_dirty(cache);
		buffer_unpinned(cache);

		//헤더 불러옴
		cache = buffer_read_page(table_id, 0, cache);
		cache->frame.base.header_page.root_page_number = new_root_pagenum;
		buffer_set_dirty(cache);
		buffer_unpinned(cache);

		//자식 페이지들의 부모 페이지 바꿔주기
		cache = buffer_read_page(table_id, child_left, cache);
		cache->frame.base.internal_page.parent_page_number = new_root_pagenum;
		buffer_set_dirty(cache);
		buffer_unpinned(cache);

		cache = buffer_read_page(table_id, child_right, cache);
		cache->frame.base.internal_page.parent_page_number = new_root_pagenum;
		buffer_set_dirty(cache);
		buffer_unpinned(cache);
		return 0;
	}
	cache = buffer_read_page(table_id, parent_pagenum, cache);
	int64_t number_of_key = cache->frame.base.internal_page.number_of_keys;
	//레코드의 갯수가 아직 넣어도 꽉차지 않을 때
	if (number_of_key < ORDER - 1)
	{
		buffer_unpinned(cache);
		return insert_to_internal_page(table_id, parent_pagenum, key, child_right);
	}
	buffer_unpinned(cache);
	return insert_to_internal_page_after_split(table_id, parent_pagenum, key, child_right);
}

//insert to internal page
pagenum_t insert_to_internal_page(int table_id, pagenum_t pagenum, int64_t key, pagenum_t child_pagenum)
{
	buffer* cache = NULL;

	cache = buffer_read_page(table_id, pagenum, cache);
	int number_of_keys = cache->frame.base.internal_page.number_of_keys;
	//일단 insert
	internal_record internal_record = { key, child_pagenum };
	//insertion 위치
	int insertion_point = cache->frame.base.leaf_page.number_of_keys;
	//레코드 삽입
	while (insertion_point > 0)
	{
		if (cache->frame.base.internal_page.internal_record[insertion_point - 1].key < key) break;
		//오른쪽에 복사
		cache->frame.base.internal_page.internal_record[insertion_point] = cache->frame.base.internal_page.internal_record[insertion_point - 1];
		insertion_point--;
	}
	cache->frame.base.internal_page.internal_record[insertion_point] = internal_record;
	//데이터 수정
	cache->frame.base.internal_page.number_of_keys++;
	buffer_set_dirty(cache);
	buffer_unpinned(cache);
	return 0;
}
//insert to internal page after split
pagenum_t insert_to_internal_page_after_split(int table_id, pagenum_t pagenum, int64_t key, pagenum_t child_pagenum)
{
	buffer* cache = NULL;

	//오른쪽 첫번째, 왼쪽 마지막 저장
	int64_t right_first, left_last;
	cache = buffer_read_page(table_id, pagenum, cache);
	//internal page의 경우 왼쪽은 0~ (split-1), 오른쪽은 (split+1)~(ORDER-1)
	//오른쪽 페이지 담을 정보 저장
	int split = cut();
	pagenum_t right_parent_page = cache->frame.base.internal_page.parent_page_number;
	int32_t right_number_of_key = ORDER - 1 - split;
	internal_record right_internal_records[ORDER - 1];
	int j = 0;
	for (int i = split; i <= ORDER - 1; ++i)
	{
		right_internal_records[j++] = cache->frame.base.internal_page.internal_record[i];
	}
	left_last = cache->frame.base.internal_page.internal_record[split - 1].key;
	//왼쪽 페이지 수정 후 저장
	cache->frame.base.internal_page.number_of_keys = split;
	buffer_set_dirty(cache);
	buffer_unpinned(cache);

	//오른쪽 페이지 할당
	pagenum_t new_right_page = buffer_alloc_page(table_id);
	cache = buffer_read_page(table_id, new_right_page, cache);
	cache->frame.base.internal_page.is_leaf = 0;
	cache->frame.base.internal_page.parent_page_number = right_parent_page;
	cache->frame.base.internal_page.number_of_keys = right_number_of_key;
	for (int i = 0; i < right_number_of_key; ++i)
	{
		cache->frame.base.internal_page.internal_record[i] = right_internal_records[i];
	}
	right_first = right_internal_records[0].key;
	buffer_set_dirty(cache);
	buffer_unpinned(cache);

	//insert 부분
	
	pagenum_t right_left_most_pagenum;
	pagenum_t for_insert_to_parent;
	if (key == left_last || key == right_first)
	{
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
		cache = buffer_read_page(table_id, pagenum, cache);
		internal_record internal_record_ = { key, child_pagenum };
		int insertion_point = cache->frame.base.internal_page.number_of_keys;
		while (insertion_point > 0)
		{
			if (cache->frame.base.internal_page.internal_record[insertion_point - 1].key < key) break;
			cache->frame.base.internal_page.internal_record[insertion_point] = cache->frame.base.internal_page.internal_record[insertion_point - 1];
			insertion_point--;
		}
		//맞는 위치에 삽입
		cache->frame.base.internal_page.internal_record[insertion_point] = internal_record_;
		//갯수 올리기
		cache->frame.base.internal_page.number_of_keys++;
		right_left_most_pagenum = cache->frame.base.internal_page.internal_record[cache->frame.base.internal_page.number_of_keys - 1].page_number;
		//맨 오른쪽 값 선택
		for_insert_to_parent = cache->frame.base.internal_page.internal_record[cache->frame.base.internal_page.number_of_keys - 1].key;
		cache->frame.base.internal_page.number_of_keys--;
		buffer_set_dirty(cache);
		buffer_unpinned(cache);
	}
	else if (right_first < key)
	{
		cache = buffer_read_page(table_id, new_right_page, cache);

		//맨 오른쪽 일 때
		internal_record internal_record_ = { key, child_pagenum };
		int insertion_point = cache->frame.base.internal_page.number_of_keys;
		while (insertion_point > 0)
		{
			if (cache->frame.base.internal_page.internal_record[insertion_point - 1].key < key) break;
			cache->frame.base.internal_page.internal_record[insertion_point] = cache->frame.base.internal_page.internal_record[insertion_point - 1];
			insertion_point--;
		}
		//맞는 위치에 삽입
		cache->frame.base.internal_page.internal_record[insertion_point] = internal_record_;
		//갯수 올리기
		cache->frame.base.internal_page.number_of_keys++;
		right_left_most_pagenum = cache->frame.base.internal_page.internal_record[0].page_number;
		for_insert_to_parent = cache->frame.base.internal_page.internal_record[0].key;
		//맨 첫번째값 빼기
		for (int i = 0; i < cache->frame.base.internal_page.number_of_keys - 1; ++i)
		{
			cache->frame.base.internal_page.internal_record[i] = cache->frame.base.internal_page.internal_record[i + 1];
		}
		cache->frame.base.internal_page.number_of_keys--;
		buffer_set_dirty(cache);
		buffer_unpinned(cache);
	}

	//오른쪽 페이지 left most page 입력
	cache = buffer_read_page(table_id, new_right_page, cache);
	cache->frame.base.internal_page.left_most_page_number = right_left_most_pagenum;
	right_number_of_key = cache->frame.base.internal_page.number_of_keys;
	buffer_set_dirty(cache);
	buffer_unpinned(cache);

	//오른쪽 페이지의 자식 페이지들 부모페이지 바꿔주기
	cache = buffer_read_page(table_id, right_left_most_pagenum, cache);
	cache->frame.base.internal_page.parent_page_number = new_right_page;
	buffer_set_dirty(cache);
	buffer_unpinned(cache);

	for (int i = 0; i < right_number_of_key; ++i)
	{
		cache = buffer_read_page(table_id, new_right_page, cache);
		pagenum_t next = cache->frame.base.internal_page.internal_record[i].page_number;
		buffer_unpinned(cache);

		cache = buffer_read_page(table_id, next, cache);
		cache->frame.base.internal_page.parent_page_number = new_right_page;
		buffer_set_dirty(cache);
		buffer_unpinned(cache);
	}
	//쪼개졌으니 부모페이지(=internal page)에 insert
	return insert_to_parent_page(table_id, right_parent_page, for_insert_to_parent, pagenum, new_right_page);
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
pagenum_t find_leafpage(int table_id, int64_t key)
{
	buffer* cache = NULL;

	pagenum_t root = get_root_internal_page(table_id);
	if (root == 0)
	{
		return 0;
	}
	while (1)
	{
		cache = buffer_read_page(table_id, root, cache);
		int is_leaf = cache->frame.base.internal_page.is_leaf;
		int32_t number_of_key = cache->frame.base.internal_page.number_of_keys;
		//리프 페이지가 맞다면
		if (is_leaf == 1)
		{
			buffer_unpinned(cache);
			return root;
		}
		else//리프페이지 아닐 때
		{
			//left most 인경우
			if (key < cache->frame.base.internal_page.internal_record[0].key)
			{
				root = cache->frame.base.internal_page.left_most_page_number;
				buffer_unpinned(cache);
			}
			else
			{
				int i;
				for (i = 0; i < number_of_key - 1; ++i)
				{
					if (cache->frame.base.internal_page.internal_record[i + 1].key > key)
					{
						root = cache->frame.base.internal_page.internal_record[i].page_number;
						break;
					}
				}
				if (i == number_of_key - 1) root = cache->frame.base.internal_page.internal_record[i].page_number;
				buffer_unpinned(cache);

			}
		}
	}

	printf("cannot find leaft error\n");
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
pagenum_t get_root_internal_page(int table_id)
{
	buffer* cache = NULL;

	pagenum_t root;
	//헤더 페이지 읽기
	cache = buffer_read_page(table_id, 0, cache);
	root = cache->frame.base.header_page.root_page_number;
	buffer_unpinned(cache);

	return root;
}
//해당 페이지의 height 얻기
int get_height(int table_id, pagenum_t pagenum)
{
	buffer* cache = NULL;
	pagenum_t root_pagenum = get_root_internal_page(table_id);
	int height = 0;
	while (root_pagenum != pagenum)
	{
		//root 페이지 넘버랑 다르면 읽고
		cache = buffer_read_page(table_id, pagenum, cache);
		//부모 페이지 넘버로 이동
		pagenum = cache->frame.base.internal_page.parent_page_number;
		height++;

		buffer_unpinned(cache);
	}
	return height;
}

int db_find(int table_id, int64_t key, char* ret_val, int trx_id)
{
	if (trx_manager.trx_table[trx_id].state == IDLE)
	{
		return -1;
	}



	//Acquire the buffer pool latch.
	pthread_mutex_lock(&buffer_pools.buf_pool_sys_mutex);

	buffer* cache = NULL;

	pagenum_t root = get_root_internal_page(table_id);
	while (root != 0)
	{
		cache = buffer_read_page(table_id, root, cache);
		int is_leaf = cache->frame.base.internal_page.is_leaf;
		int32_t number_of_key = cache->frame.base.internal_page.number_of_keys;
		//리프 페이지가 맞다면
		if (is_leaf == 1)
		{
			//리프페이지 안에서 key값 찾기
			int i;
			for (i = 0; i < number_of_key; ++i)
			{
				//찾는게 있을 경우
				if (cache->frame.base.leaf_page.records[i].key == key)
				{
					buffer_unpinned(cache);
					
					

					//buffer에 대한 mutex를 잡았을 경우
					if (pthread_mutex_trylock(&cache->buf_sys_mutex) == 0)
					{
						pthread_mutex_unlock(&buffer_pools.buf_pool_sys_mutex);

						enum return_value rvalue = acquire_record_lock(table_id, cache->page_num, key, trx_id, SHARED, i);

						if (rvalue == DEADLOCK)
						{
							buffer_unpinned(cache);
							pthread_mutex_unlock(&cache->buf_sys_mutex);

							abort_trx(trx_id);
							end_trx(trx_id);

							//return fail
							return -1;
						}
						else if (rvalue == CONFLICT)
						{
							pthread_mutex_unlock(&cache->buf_sys_mutex);

							pthread_cond_wait(&trx_manager.trx_table[trx_id].wait_lock->trx->trx_cond, &trx_manager.trx_table[trx_id].wait_lock->trx->trx_mutex);

							//after waken;
							return db_find(table_id, key, ret_val, trx_id);
						}

						//record lock이 성공적으로 잡혔으면 find
						cache = buffer_read_page(table_id, root, cache);

						//리프페이지 안에서 key값 찾기
						int i;
						for (i = 0; i < number_of_key; ++i)
						{
							//찾는게 있을 경우
							if (cache->frame.base.leaf_page.records[i].key == key)
							{
								strcpy(ret_val, cache->frame.base.leaf_page.records[i].value);

								buffer_unpinned(cache);
								pthread_mutex_unlock(&cache->buf_sys_mutex);

								return 0;

							}
						}

						//찾는게 없을 경우
						return -1;
					}
					else
					{
						pthread_mutex_unlock(&buffer_pools.buf_pool_sys_mutex);
						return db_find(table_id, key, ret_val, trx_id);
					}

					return 0;
				}
			}

			//찾는 값이 없을 경우
			buffer_unpinned(cache);
			pthread_mutex_unlock(&buffer_pools.buf_pool_sys_mutex);

			return -1;
		}
		else//리프페이지 아닐 때
		{
			//left most page 일 때
			if (key < cache->frame.base.internal_page.internal_record[0].key)
			{
				root = cache->frame.base.internal_page.left_most_page_number;
			}
			else
			{
				int i;
				for (i = 0; i < number_of_key - 1; ++i)
				{
					if (cache->frame.base.internal_page.internal_record[i + 1].key > key)
					{
						root = cache->frame.base.internal_page.internal_record[i].page_number;
						break;
					}
				}
				if (i == number_of_key - 1) root = cache->frame.base.internal_page.internal_record[i].page_number;
			}
			buffer_unpinned(cache);
		}
	}
	//찾기 실패
	return -1;
}
//마스터 딜리션
int db_delete(int table_id, int64_t key, int trx_id)
{
	buffer* cache;

	pagenum_t root_internal_page = get_root_internal_page(table_id);
	//아직 root도 생성되지 않았을 경우
	if (root_internal_page == NULL)
	{
		return -1;
	}
	//값이 없을 경우
	char* new_ = (char*)malloc(sizeof(record));
	if (db_find(table_id, key, new_, trx_id) != 0) return -1;
	free(new_);
	pagenum_t leaf_pagenum = find_leafpage(table_id, key);
	return delete_entry(table_id, leaf_pagenum, key);
}


//entry안에 답있다
int delete_entry(int table_id, pagenum_t pagenum, int64_t key)
{
	buffer* cache = NULL;

	cache = buffer_read_page(table_id, pagenum, cache);
	//리프 페이지 일 때 삭제
	if (cache->frame.base.internal_page.is_leaf == 1)
	{
		//leaf 페이지의 키 갯수
		int32_t num_of_key = cache->frame.base.leaf_page.number_of_keys;
		int deletion_point;
		for (deletion_point = 0; deletion_point < num_of_key; ++deletion_point)
		{
			//해당 키 만나면 삭제하고 빠져나오기
			if (cache->frame.base.leaf_page.records[deletion_point].key == key)
			{
				for (int i = deletion_point; i < num_of_key - 1; ++i)
				{
					cache->frame.base.leaf_page.records[i] = cache->frame.base.leaf_page.records[i + 1];
				}
				cache->frame.base.leaf_page.number_of_keys--;
				break;
			}
		}
	}
	else //인터널페이지일 때 삭제
	{
		//leaf 페이지의 키 갯수
		int32_t num_of_key = cache->frame.base.internal_page.number_of_keys;
		int deletion_point;
		for (deletion_point = 0; deletion_point < num_of_key; ++deletion_point)
		{
			//해당 키 만나면 삭제하고 빠져나오기
			if (cache->frame.base.internal_page.internal_record[deletion_point].key == key)
			{
				//	if (deletion_point == 0)
					//{
				//		page_data.base.internal_page.left_most_page_number = page_data.base.internal_page.internal_record[0].page_number;
				//	}
				for (int i = deletion_point; i < num_of_key - 1; ++i)
				{
					cache->frame.base.internal_page.internal_record[i] = cache->frame.base.internal_page.internal_record[i + 1];
				}
				cache->frame.base.internal_page.number_of_keys--;
				break;
			}
		}
	}
	buffer_set_dirty(cache);
	buffer_unpinned(cache);


	//루트일 경우
	cache = buffer_read_page(table_id, pagenum, cache);
	if (cache->frame.base.internal_page.parent_page_number == NULL)
	{
		buffer_unpinned(cache);
		return adjust_root(table_id, pagenum);
	}
	buffer_unpinned(cache);

	//underflow일 때
	cache = buffer_read_page(table_id, pagenum, cache);
	pagenum_t parent_page, neighbor_page;
	int32_t neighbor_index;
	int32_t k_prime_index, k_prime;
	if (cache->frame.base.leaf_page.number_of_keys < 1)
	{
		buffer_unpinned(cache);
		neighbor_index = get_neighbor_index(table_id, pagenum);
		k_prime_index = neighbor_index == -1 ? 0 : neighbor_index + 1;
		if (neighbor_index == -2)
		{
			k_prime_index = 0;
		}
		//부모페이지 얻기

		cache = buffer_read_page(table_id, pagenum, cache);
		parent_page = cache->frame.base.internal_page.parent_page_number;
		buffer_unpinned(cache);

		cache = buffer_read_page(table_id, parent_page, cache);
		k_prime = cache->frame.base.internal_page.internal_record[k_prime_index].key;
		if (neighbor_index == -2)
		{
			neighbor_page = cache->frame.base.internal_page.internal_record[0].page_number;
		}
		else if (neighbor_index == -1)
		{
			neighbor_page = cache->frame.base.internal_page.left_most_page_number;
		}
		else
		{
			neighbor_page = cache->frame.base.internal_page.internal_record[neighbor_index].page_number;
		}

		buffer_unpinned(cache);
		return coalesce(table_id, pagenum, neighbor_page, neighbor_index, k_prime);
	}
	else //not underflow일 때
	{
		int64_t for_change_key = cache->frame.base.leaf_page.records[0].key;
		pagenum_t parent_page = cache->frame.base.leaf_page.parent_page_number;
		buffer_unpinned(cache);

		//더 이상 같은 값 없을 때 0
		int find = 1;
		while (find == 1)
		{
			//부모 노드들 탐색하면서 값 변경
			cache = buffer_read_page(table_id, parent_page, cache);
			int32_t num_of_key = cache->frame.base.internal_page.number_of_keys;
			//같은 값 있으면 변경
			for (int i = 0; i < num_of_key; ++i)
			{
				if (cache->frame.base.internal_page.internal_record[i].key == key)
				{
					cache->frame.base.internal_page.internal_record[i].key = for_change_key;
					break;
				}
				if (i == num_of_key - 1) find = 0;
			}
			parent_page = cache->frame.base.internal_page.parent_page_number;

			buffer_set_dirty(cache);
			buffer_unpinned(cache);
			if (parent_page == NULL) break;
		}
		return 0;
	}
}
int adjust_root(int table_id, pagenum_t root)
{
	buffer* cache = NULL;

	cache = buffer_read_page(table_id, root, cache);
	if (cache->frame.base.internal_page.number_of_keys > 0)
	{
		buffer_unpinned(cache);
		return 0;
	}
	//empty 인 경우
	if (!cache->frame.base.leaf_page.is_leaf)
	{
		pagenum_t newroot = cache->frame.base.internal_page.left_most_page_number;
		buffer_unpinned(cache);

		cache = buffer_read_page(table_id, 0, cache);
		cache->frame.base.header_page.root_page_number = newroot;
		buffer_set_dirty(cache);
		buffer_unpinned(cache);

		cache = buffer_read_page(table_id, 0, cache);
		cache->frame.base.internal_page.parent_page_number = NULL;
		buffer_set_dirty(cache);
		buffer_unpinned(cache);
		buffer_free_page(table_id, root);
	}
	else
	{
		buffer_unpinned(cache);
	}
	return 0;
}
int coalesce(int table_id, pagenum_t pagenum, pagenum_t neighbor, int neigbor_index, int k_prime)
{
	buffer* cache = NULL;

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
	cache = buffer_read_page(table_id, neighbor, cache);
	neighbor_insertion_index = cache->frame.base.internal_page.number_of_keys;
	parent = cache->frame.base.internal_page.parent_page_number;
	//리프페이지가 아니면
	if (cache->frame.base.internal_page.is_leaf == 0)
	{
		if (neigbor_index == -2)
		{
			buffer_set_dirty(cache);
			buffer_unpinned(cache);

			buffer* n = NULL;
			n = buffer_read_page(table_id, pagenum, n);
			cache = buffer_read_page(table_id, neighbor, cache);
			cache->frame.base.internal_page.internal_record[0].key = k_prime;
			cache->frame.base.internal_page.internal_record[0].page_number = n->frame.base.internal_page.left_most_page_number;
			cache->frame.base.internal_page.number_of_keys++;
			for (int i = 0; i < n->frame.base.internal_page.number_of_keys; ++i)
			{
				cache->frame.base.internal_page.internal_record[i + 1] = n->frame.base.internal_page.internal_record[i];
				cache->frame.base.internal_page.number_of_keys++;
			}
			buffer_set_dirty(cache);
			buffer_unpinned(cache);

			cache = buffer_read_page(table_id, n->frame.base.internal_page.left_most_page_number, cache);
			cache->frame.base.internal_page.parent_page_number = neighbor;
			buffer_set_dirty(cache);
			buffer_unpinned(cache);

			for (int i = 0; i < n->frame.base.internal_page.number_of_keys; ++i)
			{
				cache = buffer_read_page(table_id, n->frame.base.internal_page.internal_record[i].page_number, cache);
				cache->frame.base.internal_page.parent_page_number = neighbor;
				buffer_set_dirty(cache);
				buffer_unpinned(cache);
			}

			buffer_set_dirty(n);
			buffer_unpinned(n);
		}
		else
		{
			//neighbor, pagenum
			cache->frame.base.internal_page.internal_record[neighbor_insertion_index].key = k_prime;
			buffer_set_dirty(cache);
			buffer_unpinned(cache);

			cache = buffer_read_page(table_id, pagenum, cache);
			pagenum_t left_most = cache->frame.base.internal_page.left_most_page_number;
			buffer_unpinned(cache);

			cache = buffer_read_page(table_id, neighbor, cache);
			cache->frame.base.internal_page.internal_record[neighbor_insertion_index].page_number = left_most;
			cache->frame.base.internal_page.number_of_keys++;
			buffer_set_dirty(cache);
			buffer_unpinned(cache);

			//left most page가 네이버를 가르키도록 설정
			cache = buffer_read_page(table_id, left_most, cache);
			cache->frame.base.internal_page.parent_page_number = neighbor;
			buffer_set_dirty(cache);
			buffer_unpinned(cache);
		}
	}
	else
	{
		buffer_set_dirty(cache);
		buffer_unpinned(cache);

		cache = buffer_read_page(table_id, pagenum, cache);
		int32_t num_of_key = cache->frame.base.leaf_page.number_of_keys;
		buffer_set_dirty(cache);
		buffer_unpinned(cache);

		buffer* n = NULL;
		cache = buffer_read_page(table_id, neighbor, cache);
		n = buffer_read_page(table_id, pagenum, n);
		for (i = neighbor_insertion_index, j = 0; j < num_of_key; i++, j++) {
			cache->frame.base.leaf_page.records[i] = n->frame.base.leaf_page.records[j];
			cache->frame.base.leaf_page.number_of_keys++;
		}
		cache->frame.base.leaf_page.right_sibling_page_number = n->frame.base.leaf_page.right_sibling_page_number;
		buffer_set_dirty(cache);
		buffer_unpinned(cache);

		buffer_set_dirty(n);
		buffer_unpinned(n);
	}
	buffer_free_page(table_id, pagenum);
	return delete_entry(table_id, parent, k_prime);
}

//pagenum이 leftmost일 때 -2 반환
//-1은 leftmost를 나타내는 인덱스이다.
int get_neighbor_index(int table_id, pagenum_t pagenum)
{
	buffer* cache = NULL;

	cache = buffer_read_page(table_id, pagenum, cache);
	pagenum_t parent_page = cache->frame.base.internal_page.parent_page_number;
	buffer_unpinned(cache);

	cache = buffer_read_page(table_id, parent_page, cache);
	int i;
	for (i = 0; i < cache->frame.base.internal_page.number_of_keys; i++)
	{
		if (cache->frame.base.internal_page.internal_record[i].page_number == pagenum)
		{
			buffer_unpinned(cache);
			return i - 1;
		}
	}
	buffer_unpinned(cache);
	return -2;
}
//루트페이지까지 길이 반환
int path_to_root(int table_id, pagenum_t child)
{
	buffer* cache = NULL;

	int length = 0;
	pagenum_t root_page = get_root_internal_page(table_id);
	while (child != root_page) {
		cache = buffer_read_page(table_id, child, cache);
		child = cache->frame.base.internal_page.parent_page_number;
		length++;
		buffer_unpinned(cache);
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
void print_tree(int table_id)
{
	buffer* cache = NULL;

	pagenum_t page = get_root_internal_page(table_id);
	int rank = 0;
	int new_rank = 0;
	enqueue(page);
	while (queue != NULL)
	{
		page = dequeue();
		cache = buffer_read_page(table_id, page, cache);
		//??
		buffer_unpinned(cache);
		if (cache->frame.base.internal_page.parent_page_number != NULL) {
			new_rank = path_to_root(table_id, page);
			if (new_rank != rank) {
				rank = new_rank;
				printf("\n");
			}
		}

		//해당 페이지 읽고 print
		cache = buffer_read_page(table_id, page, cache);
		int32_t number_of_record = cache->frame.base.internal_page.number_of_keys;
		if (number_of_record == 0)
		{
			buffer_unpinned(cache);
			continue;
		}
		//internal page 일 때
		if (cache->frame.base.internal_page.is_leaf == 0)
		{
			enqueue(cache->frame.base.internal_page.left_most_page_number);
			for (int i = 0; i < number_of_record; ++i)
			{
				enqueue(cache->frame.base.internal_page.internal_record[i].page_number);
				printf("%lld ", cache->frame.base.internal_page.internal_record[i].key);
			}
			printf("{%lld}|", page);
		}
		else//리프페이지 일 때
		{
			for (int i = 0; i < number_of_record; ++i)
			{
				printf("%lld ", cache->frame.base.leaf_page.records[i].key);
			}
			printf("{%lld}|", page);
		}
		buffer_unpinned(cache);
	}
	printf("\n\n");
}

/* ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ파일 API ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ*/
pagenum_t file_alloc_page(int table_id)
{
	//버퍼에 가르키는 포인터
	buffer* cache = NULL;

	pagenum_t first_free_page_number, second_free_page_number;

	//헤더 페이지의 free page 읽기
	cache = buffer_read_page(table_id, 0, cache);
	first_free_page_number = cache->frame.base.header_page.free_page_number;
	buffer_unpinned(cache);

	//만약 다 할당했으면
	if (first_free_page_number == 0)
	{
		printf("프리페이지 추가 할당\n");
		cache = buffer_read_page(table_id, 0, cache);
		pagenum_t num_of_page = cache->frame.base.header_page.number_of_pages;
		//헤더페이지가 가르키느 프리페이지 할당
		cache->frame.base.header_page.free_page_number = num_of_page;
		buffer_set_dirty(cache);
		buffer_unpinned(cache);

		int i;
		for (i = 1; i < 256 * 10; ++i, ++num_of_page)
		{
			cache = buffer_read_page(table_id, num_of_page, cache);
			cache->frame.base.free_page.next_free_page_number = num_of_page + 1;
			buffer_set_dirty(cache);
			buffer_unpinned(cache);
		}
		//마지막 free page가 가르키는건 0으로 한다.
		cache = buffer_read_page(table_id, num_of_page, cache);
		cache->frame.base.free_page.next_free_page_number = 0;
		buffer_set_dirty(cache);
		buffer_unpinned(cache);

		cache = buffer_read_page(table_id, 0, cache);
		cache->frame.base.header_page.number_of_pages += 256 * 10;
		buffer_set_dirty(cache);

		first_free_page_number = cache->frame.base.header_page.free_page_number;
		
		buffer_unpinned(cache);
	}
	


	//두번째 free page num 불러오기
	cache = buffer_read_page(table_id, first_free_page_number, cache);
	second_free_page_number = cache->frame.base.free_page.next_free_page_number;
	buffer_unpinned(cache);

	//헤더 페이지가 second_free_page_number를 가르키게 설정
	cache = buffer_read_page(table_id, 0, cache);
	cache->frame.base.header_page.free_page_number = second_free_page_number;
	buffer_set_dirty(cache);
	buffer_unpinned(cache);

	//할당할 free_page 넘겨주기
	return first_free_page_number;
}

void file_free_page(int table_id, pagenum_t pagenum)
{
	buffer* cache = NULL;

	pagenum_t next_page_num = 0, first_page_number = 0;
	//헤더가 가르키는 첫번째 free page 저장
	cache = buffer_read_page(table_id, 0, cache);
	first_page_number = cache->frame.base.header_page.free_page_number;
	//헤더페이지가 가르키는 free page가 pagenum을 가르키게 함
	cache->frame.base.header_page.free_page_number = pagenum;
	buffer_set_dirty(cache);
	buffer_unpinned(cache);

	//비어있는 4096Byte의 문자열
	page_t empty;
	file_write_page(table_id, pagenum, &empty);

	//해당 페이지가 first_page_number를 가르키게 하기
	cache = buffer_read_page(table_id, pagenum, cache);
	cache->frame.base.free_page.next_free_page_number = first_page_number;
	buffer_set_dirty(cache);
	buffer_unpinned(cache);
}

void file_read_page(int table_id, pagenum_t pagenum, page_t* dest)
{

	/* 앞에 해당 table_id에 해당하는 파일을 열어야 한다*/
	file_pointer = fopen(table_id_pool.table[table_id].path_name, "rb+");
	if (file_pointer == NULL)
	{
		printf("table_id %d, pagenum %d\n", table_id, pagenum);
		printf("열려는 pathname : %s\n", table_id_pool.table[table_id].path_name);
		printf("파일이 열리지 않았습니다. (file_read_page)\n");
		return;
	}
	fseek(file_pointer, PAGE_SIZE * pagenum, SEEK_SET);
	fread(dest, sizeof(page_t), 1, file_pointer);
	fclose(file_pointer);
}

void file_write_page(int table_id, pagenum_t pagenum, const page_t* src)
{
	/* 앞에 해당 table_id에 해당하는 파일을 열어야 한다*/
	file_pointer = fopen(table_id_pool.table[table_id].path_name, "rb+");
	if (file_pointer == NULL)
	{
		printf("파일이 열리지 않았습니다. (file_read_page)\n");
		return;
	}

	//해당 페이지에 접근해서 src에 저장
	fseek(file_pointer, PAGE_SIZE * pagenum, SEEK_SET);
	fwrite(src, sizeof(page_t), 1, file_pointer);
	fclose(file_pointer);
}
/*ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ파일 API 끝 ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ*/


/*ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ버퍼 API 시작ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ*/
int init_db(int num_buf)
{

	//버퍼 pool 초기화
	int i;
	buffer_pools.buffer_pool = (buffer*)malloc(num_buf * sizeof(buffer));

	//생성이 안되었으면
	if (buffer_pools.buffer_pool == NULL)
	{
		return -1;
	}

	buffer_pools.size = num_buf;
	buffer_pools.selected = buffer_pools.buffer_pool;
	buffer_pools.buf_pool_sys_mutex = PTHREAD_MUTEX_INITIALIZER;

	trx_manager.trx_sys_mutex = PTHREAD_MUTEX_INITIALIZER;
	trx_manager.next_trx_id = 0;



	lock_manager.lock_sys_mutex = PTHREAD_MUTEX_INITIALIZER;

	for (i = 0; i < buffer_pools.size; ++i)
	{
		page_t empty;

		buffer_pools.buffer_pool[i].frame = empty;
		buffer_pools.buffer_pool[i].is_dirty = 0;
		buffer_pools.buffer_pool[i].page_num = 0;
		buffer_pools.buffer_pool[i].ref = 0;
		buffer_pools.buffer_pool[i].table_id = 0;
		buffer_pools.buffer_pool[i].next_buffer = i + 1;
		buffer_pools.buffer_pool[i].is_pinned = 0;

		buffer_pools.buffer_pool[i].buf_sys_mutex = PTHREAD_MUTEX_INITIALIZER;

		if (i == buffer_pools.size - 1)
		{
			buffer_pools.buffer_pool[i].next_buffer = 0;
		}
	}
	
	

	//테이블 id pool 초기화
	table_id_pool.size = 100;

	for (int i = 1; i < table_id_pool.size; ++i)
	{
		table_id_pool.table[i].table_id = i;
		table_id_pool.table[i].used = false;
	}

	return 0;
}

/*pinned상태를 하나 내려준다*/
void buffer_unpinned(buffer* src)
{
	src->is_pinned -= 1;
}

/*dirty한 상태로 만든다*/
void buffer_set_dirty(buffer* src)
{
	src->is_dirty = true;
}

//table_id의 pagenum에 해당하는 버퍼를 찾아 dest에 할당 시킨다.
buffer* buffer_read_page(int table_id, pagenum_t pagenum, buffer* dest)
{

	/*printf("table_id = %d, pagenum = %d\n", table_id, pagenum);
	for (int i = 0; i < buffer_pools.size; ++i)
	{
		printf("%d, unpinned %d, used %d\n", i, buffer_pools.buffer_pool[i].is_pinned,buffer_pools.buffer_pool[i].table_id);
	}
	printf("\n");*/
	//dest를 null로 초기화
	dest = NULL;

	//찾는 값을 버퍼풀안에서 찾기
	int i;
	for (i = 1; i < buffer_pools.size; ++i)
	{
		//찾으면 dest를 해당 frame으로 가르키자
		if (buffer_pools.buffer_pool[i].page_num == pagenum && buffer_pools.buffer_pool[i].table_id == table_id)
		{
			buffer_pools.buffer_pool[i].is_pinned += 1;
			return &buffer_pools.buffer_pool[i];
		}
	}

	//버퍼에 맞는 페이지가 없으면 버퍼풀로 그 페이지 읽어 오기
	if (dest == NULL)
	{
		//Clock Policy
		//victim을 찾을때 까지 돌린다.
		while (dest == NULL)
		{
			//사용하지 않는 것을 발견한다면
			if (buffer_pools.selected->is_pinned == 0)
			{
				//ref이 켜져있다면
				if (buffer_pools.selected->ref == true)
				{
					//ref을 지우고 다음으로 넘어간다.
					buffer_pools.selected->ref = false;
					buffer_pools.selected = &buffer_pools.buffer_pool[buffer_pools.selected->next_buffer];
					continue;
				}

				//dirty한 상태의 페이지라면
				if (buffer_pools.selected->is_dirty == true)
				{
					//clean 페이지로 만들자
					file_write_page(buffer_pools.selected->table_id, buffer_pools.selected->page_num, &(buffer_pools.selected->frame));
					buffer_pools.selected->is_dirty = false;
				}

				//clean page이고 ref이 꺼져있다면 불러오기
				dest = buffer_pools.selected;
			}
			else
			{
				//만약 사용하는 것을 발견하면 다음으로 넘어가기
				buffer_pools.selected = &buffer_pools.buffer_pool[buffer_pools.selected->next_buffer];
			}
		}

		//victim에 해당하는 페이지 쓰기
		file_read_page(table_id, pagenum, &(dest->frame));

		dest->is_dirty = false;
		dest->is_pinned = 1;
		dest->page_num = pagenum;
		dest->ref = true;
		dest->table_id = table_id;

		return buffer_pools.selected;
	}
}

pagenum_t buffer_alloc_page(int table_id)
{
	//할당을 받은 페이지넘버
	pagenum_t allocated_pagenum = file_alloc_page(table_id);

	return allocated_pagenum;
}

void buffer_free_page(int table_id, pagenum_t pagenum)
{
	file_free_page(table_id, pagenum);
}

int close_table(int table_id)
{
	//버퍼풀에 해당하는 table_id의 값들이 있는지 확인
	for (int i = 0; i < buffer_pools.size; ++i)
	{
		//찾으면
		if (buffer_pools.buffer_pool[i].table_id == table_id)
		{
			//만약 pinned 상태인 버퍼가 있으면
			if (buffer_pools.buffer_pool[i].is_pinned > 0)
			{
				//close 안됬다고 반환
				return -1;
			}

			//ref끄기
			buffer_pools.buffer_pool[i].ref = 0;
			if (buffer_pools.buffer_pool[i].is_dirty == 1)
			{
				//clean page로 만들기
				file_write_page(table_id, buffer_pools.buffer_pool[i].page_num, &(buffer_pools.buffer_pool[i].frame));
				buffer_pools.buffer_pool[i].is_dirty = false;
			}
		}
	}

	//table_id 정리
	table_id_pool.table[table_id].used = false;

	return 0;
}

int shutdown_db(void)
{
	//버퍼풀 돌아가면서 체크
	for (int i = 0; i < buffer_pools.size; ++i)
	{
		//만약 pinned 상태인 버퍼가 있으면
		if (buffer_pools.buffer_pool[i].is_pinned > 0)
		{
			//close 안됬다고 반환
			return -1;
		}

		//ref끄기
		buffer_pools.buffer_pool[i].ref = 0;
		if (buffer_pools.buffer_pool[i].is_dirty == 1)
		{
			//clean page로 만들기
			file_write_page(buffer_pools.buffer_pool[i].table_id, buffer_pools.buffer_pool[i].page_num, &buffer_pools.buffer_pool[i].frame);
			buffer_pools.buffer_pool[i].is_dirty = false;
		}
	}

	buffer_pools.size = 0;
	buffer_pools.selected = NULL;
	free(buffer_pools.buffer_pool);
	return 0;
}

/*ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ버퍼 API 끝ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ*/

/*ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ조인 API ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ*/

pagenum_t find_first_leafpage(int table_id)
{
	buffer* cache = NULL;

	pagenum_t root = get_root_internal_page(table_id);
	if (root == 0)
	{
		return 0;
	}
	while (1)
	{
		
		cache = buffer_read_page(table_id, root, cache);
		int is_leaf = cache->frame.base.internal_page.is_leaf;
		int32_t number_of_key = cache->frame.base.internal_page.number_of_keys;
		//리프 페이지가 맞다면
		if (is_leaf == 1)
		{
			buffer_unpinned(cache);
			return root;
		}
		else//리프페이지 아닐 때
		{
			root = cache->frame.base.internal_page.left_most_page_number;
			buffer_unpinned(cache);
		}
	}
}

int join_table(int table_id_1, int table_id_2, char* pathname)
{


	buffer* lcache = NULL;
	buffer* rcache = NULL;

	pagenum_t left_leafpage = find_first_leafpage(table_id_1);
	if (left_leafpage == 0)
	{
		return -1;
	}
	pagenum_t right_leafpage;
	
	while (1)
	{
		lcache = buffer_read_page(table_id_1, left_leafpage, lcache);
		
		for (int i = 0; i < (lcache->frame.base.leaf_page.number_of_keys); ++i)
		{


			bool contact = false;
			bool isend = false;
			int64_t leftkey = lcache->frame.base.leaf_page.records[i].key;
			char leftvalue[120];
			strcpy(leftvalue, lcache->frame.base.leaf_page.records[i].value);

			
			right_leafpage = find_leafpage(table_id_2, leftkey);
			if (left_leafpage == 0)
			{
				return -1;
			}

			while (isend == false && right_leafpage != 0)
			{

				/*file_pointer = fopen(pathname, "a");
				if (file_pointer == NULL)
				{
					file_pointer = fopen(pathname, "a");

				}*/

				rcache = buffer_read_page(table_id_2, right_leafpage, rcache);

				for (int j = 0; j < rcache->frame.base.leaf_page.number_of_keys; ++j)
				{
					if (leftkey == rcache->frame.base.leaf_page.records[j].key)
					{
						file_pointer = fopen(pathname, "a");
						contact = true;
						char buff[256] = "";
						char str[256] = "";

						sprintf(buff, "%d,", leftkey);

						sprintf(str,"%s", leftvalue);
						strcat(buff, str);

						sprintf(str, ",%d,", rcache->frame.base.leaf_page.records[j].key);
						strcat(buff, str);


						sprintf(str, "%s", rcache->frame.base.leaf_page.records[j].value);
						strcat(buff, str);

						fwrite(buff, sizeof(buff), 1, file_pointer);
						fprintf(file_pointer, "\n");
						fclose(file_pointer);

					}
					else if (leftkey != rcache->frame.base.leaf_page.records[j].key && contact == true)
					{
						isend = true;
						break;
					}
					
				}

				right_leafpage = rcache->frame.base.leaf_page.right_sibling_page_number;
				buffer_unpinned(rcache);

				
			}
			
		}

		pagenum_t next = lcache->frame.base.leaf_page.right_sibling_page_number;
		if (next != 0)
		{
			left_leafpage = next;
		}
		else
		{
			buffer_unpinned(lcache);
			break;
		}
	

		buffer_unpinned(lcache);
	}

	return 0;
}



/*ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ조인 API 끝ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ*/



/*ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡTransaction 끝ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ*/

pthread_t pthreads[2];

void* pthread1(int table_id)
{
	int trx_id = begin_trx();
	char str[120];
	
	Sleep(2);
	fflush(stdout);

	printf("trx_id : %d, update 1, value = %s\n", trx_id, "pthread1");

	db_update(table_id, 1, "pthread1", trx_id);

	printf("trx_id : %d, find 2, value = %s\n", trx_id, "pthread1");

	db_find(table_id, 2, str, trx_id);
	printf("trx_id : %d, update 3, value = %s\n", trx_id, "pthread1");

	db_update(table_id, 3, "pthread1", trx_id);
	printf("trx_id : %d, update 4, value = %s\n", trx_id, "pthread1");

	db_update(table_id, 4, "pthread1", trx_id);

	
	



	end_trx(trx_id);
}

void* pthread2(int table_id)
{
	int trx_id = begin_trx();
	char str[120];


	fflush(stdout);
	printf("trx_id : %d, update 2, value = %s\n", trx_id, "pthread2");

	db_update(table_id, 2, "pthread2", trx_id);

	Sleep(10);

	printf("trx_id : %d, find 1, value = %s\n", trx_id, "pthread2");

	db_find(table_id, 1, str, trx_id);

	printf("trx_id : %d, update 3, value = %s\n", trx_id, "pthread2");

	db_update(table_id, 3, "pthread2", trx_id);

	printf("trx_id : %d, update 4, value = %s\n", trx_id, "pthread2");
	db_update(table_id, 4, "pthread2", trx_id);




	end_trx(trx_id);
}



int main()
{

	init_db(100);
	int table_id;
	int table_id1;
	int table_id2;

	char p = 'a';
	int input;
	char str[50] , str1[50];
	while (p != 'q')
	{
		scanf("%c", &p);
		fflush(stdin);
		switch (p)
		{
		case 'o':
			scanf("%s", &str);
			fflush(stdin);
			table_id = open_table(str);
			print_tree(table_id);
			printf("생성된 테이블 아이디 = %d\n", table_id);
			break;
		case 'i':
		{	scanf("%d", &input);
		fflush(stdin);
		int trx_id = begin_trx();
	

		if (db_insert(table_id, input, str, trx_id) == 0)
		{
			printf("key = %d, value : %s\n", input, str);
			print_tree(table_id);

		}
		else
		{
			printf("Fail to find...\n");
			print_tree(table_id);

		}

		end_trx(trx_id);
		break;
		}
		case 'd':
			scanf("%d", &input);
			fflush(stdin);

			db_delete(table_id, input, 0);
			print_tree(table_id);

			break;
		case 'c':
			scanf("%s", &str);
			fflush(stdin);

			int for_close, i;
			for (i = 1; i < table_id_pool.size; ++i)
			{
				if (!strcmp(table_id_pool.table[i].path_name, str))
				{
					for_close = table_id_pool.table[i].table_id;
					break;
				}
			}
			if (i == table_id_pool.size)
			{
				printf("닫아야 될 테이블이 없습니다.\n");
				break;
			}
			printf("%d\n", for_close);
			close_table(for_close);
			break;
		case 'f':
			scanf("%d", &input);
			fflush(stdin);

			char* str = malloc(sizeof(char) * 100);
			int trx_id = begin_trx();
			if (db_find(table_id, input, str, trx_id) == 0)
			{
				printf("key = %d, value : %s\n", input, str);
			}
			else
			{
				printf("Fail to find...\n");
			}

			end_trx(trx_id);
			break;
		case 'j':
			scanf("%d", &table_id1);
			fflush(stdin);

			scanf("%d", &table_id2);
			fflush(stdin);

			scanf("%s", &str1);
			fflush(stdin);
			join_table(table_id1, table_id2, str1);
			break;
		case 'm':
			for (int i = 0; i < 50000; ++i)
			{
				int key = rand() % 1000000 + 1;
				db_insert(table_id, key, "테스트 중입니다.", 0);

				if ((i + 1) % 100 == 0)
				{
					printf("%d 번째 %d inserted\n", i + 1, key);

				}
			}
			print_tree(table_id);
			break;

		case't': //test lock_manager
		{
			int a = 100;

			while (a--)
			{
				int* return1 = NULL, return2 = NULL;;

				char buf[50];


				printf("쓰레드1 시작상태 %d\n", pthread_create(&pthreads[0], NULL, &pthread1, (void *)table_id));
	
				printf("쓰레드2 시작상태 %d\n", pthread_create(&pthreads[1], NULL, &pthread2, (void *)table_id));

				printf("쓰레드 1 종료상태 %d\n", pthread_join(pthreads[0], (void**)return1));

				printf("쓰레드 2 종료상태 %d\n", pthread_join(pthreads[1], (void**)return2));


			}


			break;
		}
		default:
			break;
		}
	}

	return 0;
}