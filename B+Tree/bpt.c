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
	
	//���̺� id pool �ѹ� ������ ã��
	for (int i = 1; i <= table_id_pool.size; ++i)
	{
		if (strcmp(table_id_pool.table[i].path_name , pathname) == 0&& table_id_pool.table[i].used == true)
		{
			table_id = i;
			return table_id;
		}
	}

	//���̺� ���̵� Ǯ�� ������ ����
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

	//���������� �ȸ����������
	if (file_pointer == NULL)
	{
		file_pointer = fopen(pathname, "wb+");
		if (file_pointer == NULL)
		{
			return -1;
		}
		//��������� �ʱ�ȭ
		cache = buffer_read_page(table_id, 0, cache);
		cache->frame.base.header_page.free_page_number = 1;
		cache->frame.base.header_page.root_page_number = NULL;
		//�Ҵ��� free page�� 256 * 10(==10mb) + header page
		cache->frame.base.header_page.number_of_pages = 256 * 10 + 1;
		cache->ref = false;
		buffer_set_dirty(cache);
		buffer_unpinned(cache);

		//free page�����
		pagenum_t i;
		for (i = 1; i < 256 * 10; ++i)
		{
			cache = buffer_read_page(table_id, i, cache);
			cache->frame.base.free_page.next_free_page_number = i + 1;
			cache->ref = false;
			buffer_set_dirty(cache);
			buffer_unpinned(cache);
		}
		//������ free page�� ����Ű�°� 0���� �Ѵ�.
		cache = buffer_read_page(table_id, i, cache);
		cache->frame.base.free_page.next_free_page_number = 0;
		cache->ref = false;
		buffer_set_dirty(cache);
		buffer_unpinned(cache);
	}

	fclose(file_pointer);
	return table_id;
}

//�����Լ�
int db_insert(int table_id, int64_t key, char* value, int trx_id)
{
	buffer* cache = NULL;

	pagenum_t root_internal_page = get_root_internal_page(table_id);
	//insert�� ó���� ���
	if (root_internal_page == NULL)
	{
		//ù leaf page ����
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

		//����������� ��Ʈ������ �Է�
		cache = buffer_read_page(table_id, 0, cache);
		cache->frame.base.header_page.root_page_number = first_leaf_page;
		buffer_set_dirty(cache);
		buffer_unpinned(cache);
		return 0;
	}
	//�̹� ���� ���
	char* new_ = (char*)malloc(sizeof(record));
	if (db_find(table_id, key, new_, trx_id) == 0) return -1;
	free(new_);
	pagenum_t leaf_pagenum = find_leafpage(table_id, key);

	cache = buffer_read_page(table_id, leaf_pagenum, cache);
	//ORDER - 2 ���� �϶�(=�� �ɰ��� �� ��)
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
	//insertion ��ġ
	int insertion_point = cache->frame.base.leaf_page.number_of_keys;
	//���ڵ� ����
	while (insertion_point > 0)
	{
		if (cache->frame.base.leaf_page.records[insertion_point - 1].key < new_record.key) break;
		//�����ʿ� ����
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
	//���������� �ű� �ڷ�� ����
	pagenum_t new_parent_pagenum = cache->frame.base.leaf_page.parent_page_number;
	int32_t new_number_of_keys = ORDER - 1 - cut();
	record* new_right_record = (record*)malloc(sizeof(record)*(ORDER - 1));
	pagenum_t new_right_sibling = cache->frame.base.leaf_page.right_sibling_page_number;
	int i = 0;
	for (int j = cut(); j < ORDER - 1;)
	{
		new_right_record[i++] = cache->frame.base.leaf_page.records[j++];
	}
	//���� ������ ����
	cache->frame.base.leaf_page.number_of_keys = cut();
	left_last = cache->frame.base.leaf_page.records[cache->frame.base.leaf_page.number_of_keys - 1].key;
	buffer_set_dirty(cache);
	buffer_unpinned(cache);

	//������ ������ ����� �� ����
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

	//������������ ���ư��� right sibling �� ����
	cache = buffer_read_page(table_id, pagenum, cache);
	cache->frame.base.leaf_page.right_sibling_page_number = new_right_pagenum;
	buffer_set_dirty(cache);
	buffer_unpinned(cache);

	//������ key�� ����
	if (left_last < new_record.key)
	{
		cache = buffer_read_page(table_id, new_right_pagenum, cache);
		//insertion ��ġ
		int insertion_point = cache->frame.base.leaf_page.number_of_keys;
		//���ڵ� ����
		while (insertion_point > 0)
		{
			if (cache->frame.base.leaf_page.records[insertion_point - 1].key < new_record.key) break;
			//�����ʿ� ����
			cache->frame.base.leaf_page.records[insertion_point] = cache->frame.base.leaf_page.records[insertion_point - 1];
			insertion_point--;
		}
		cache->frame.base.leaf_page.records[insertion_point] = new_record;
		cache->frame.base.leaf_page.number_of_keys++;
		for_insert_key = cache->frame.base.leaf_page.records[0].key;
		buffer_set_dirty(cache);
		buffer_unpinned(cache);
	}
	else if (left_last > new_record.key)//���� ������ �� ����
	{
		
		cache = buffer_read_page(table_id, pagenum, cache);
		//insertion ��ġ
		int insertion_point = cache->frame.base.leaf_page.number_of_keys;
		//���ڵ� ����
		while (insertion_point > 0)
		{
			if (cache->frame.base.leaf_page.records[insertion_point - 1].key < new_record.key) break;
			//�����ʿ� ����
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

		//�����ʿ� ����
		cache = buffer_read_page(table_id, new_right_pagenum, cache);
		//insertion ��ġ
		insertion_point = cache->frame.base.leaf_page.number_of_keys;
		//���ڵ� ����
		while (insertion_point > 0)
		{
			if (cache->frame.base.leaf_page.records[insertion_point - 1].key < for_insert_key) break;
			//�����ʿ� ����
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
	//�ɰ������� �θ�������(=internal page)�� insert
	return insert_to_parent_page(table_id, new_parent_pagenum, for_insert_key, pagenum, new_right_pagenum);
}
pagenum_t insert_to_parent_page(int table_id, pagenum_t parent_pagenum, int64_t key, pagenum_t child_left, pagenum_t child_right)
{
	buffer* cache = NULL;

	//��Ʈ ������ �϶�
	if (parent_pagenum == NULL)
	{
		pagenum_t new_root_pagenum = buffer_alloc_page(table_id);
		cache = buffer_read_page(table_id, new_root_pagenum, cache);
		//�� ��Ʈ ������ �� �Ҵ�(���� left_most�� �Ҵ� ����)
		cache->frame.base.internal_page.is_leaf = 0;
		cache->frame.base.internal_page.number_of_keys = 1;
		cache->frame.base.internal_page.parent_page_number = NULL;
		cache->frame.base.internal_page.internal_record[0].key = key;
		cache->frame.base.internal_page.left_most_page_number = child_left;
		cache->frame.base.internal_page.internal_record[0].page_number = child_right;
		buffer_set_dirty(cache);
		buffer_unpinned(cache);

		//��� �ҷ���
		cache = buffer_read_page(table_id, 0, cache);
		cache->frame.base.header_page.root_page_number = new_root_pagenum;
		buffer_set_dirty(cache);
		buffer_unpinned(cache);

		//�ڽ� ���������� �θ� ������ �ٲ��ֱ�
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
	//���ڵ��� ������ ���� �־ ������ ���� ��
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
	//�ϴ� insert
	internal_record internal_record = { key, child_pagenum };
	//insertion ��ġ
	int insertion_point = cache->frame.base.leaf_page.number_of_keys;
	//���ڵ� ����
	while (insertion_point > 0)
	{
		if (cache->frame.base.internal_page.internal_record[insertion_point - 1].key < key) break;
		//�����ʿ� ����
		cache->frame.base.internal_page.internal_record[insertion_point] = cache->frame.base.internal_page.internal_record[insertion_point - 1];
		insertion_point--;
	}
	cache->frame.base.internal_page.internal_record[insertion_point] = internal_record;
	//������ ����
	cache->frame.base.internal_page.number_of_keys++;
	buffer_set_dirty(cache);
	buffer_unpinned(cache);
	return 0;
}
//insert to internal page after split
pagenum_t insert_to_internal_page_after_split(int table_id, pagenum_t pagenum, int64_t key, pagenum_t child_pagenum)
{
	buffer* cache = NULL;

	//������ ù��°, ���� ������ ����
	int64_t right_first, left_last;
	cache = buffer_read_page(table_id, pagenum, cache);
	//internal page�� ��� ������ 0~ (split-1), �������� (split+1)~(ORDER-1)
	//������ ������ ���� ���� ����
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
	//���� ������ ���� �� ����
	cache->frame.base.internal_page.number_of_keys = split;
	buffer_set_dirty(cache);
	buffer_unpinned(cache);

	//������ ������ �Ҵ�
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

	//insert �κ�
	
	pagenum_t right_left_most_pagenum;
	pagenum_t for_insert_to_parent;
	if (key == left_last || key == right_first)
	{
		return -1;
	}
	if (left_last < key && key < right_first)
	{
		//��� �� ��
		right_left_most_pagenum = child_pagenum;
		for_insert_to_parent = key;
	}
	else if (left_last > key)
	{
		//�� �� �� �� ��
		cache = buffer_read_page(table_id, pagenum, cache);
		internal_record internal_record_ = { key, child_pagenum };
		int insertion_point = cache->frame.base.internal_page.number_of_keys;
		while (insertion_point > 0)
		{
			if (cache->frame.base.internal_page.internal_record[insertion_point - 1].key < key) break;
			cache->frame.base.internal_page.internal_record[insertion_point] = cache->frame.base.internal_page.internal_record[insertion_point - 1];
			insertion_point--;
		}
		//�´� ��ġ�� ����
		cache->frame.base.internal_page.internal_record[insertion_point] = internal_record_;
		//���� �ø���
		cache->frame.base.internal_page.number_of_keys++;
		right_left_most_pagenum = cache->frame.base.internal_page.internal_record[cache->frame.base.internal_page.number_of_keys - 1].page_number;
		//�� ������ �� ����
		for_insert_to_parent = cache->frame.base.internal_page.internal_record[cache->frame.base.internal_page.number_of_keys - 1].key;
		cache->frame.base.internal_page.number_of_keys--;
		buffer_set_dirty(cache);
		buffer_unpinned(cache);
	}
	else if (right_first < key)
	{
		cache = buffer_read_page(table_id, new_right_page, cache);

		//�� ������ �� ��
		internal_record internal_record_ = { key, child_pagenum };
		int insertion_point = cache->frame.base.internal_page.number_of_keys;
		while (insertion_point > 0)
		{
			if (cache->frame.base.internal_page.internal_record[insertion_point - 1].key < key) break;
			cache->frame.base.internal_page.internal_record[insertion_point] = cache->frame.base.internal_page.internal_record[insertion_point - 1];
			insertion_point--;
		}
		//�´� ��ġ�� ����
		cache->frame.base.internal_page.internal_record[insertion_point] = internal_record_;
		//���� �ø���
		cache->frame.base.internal_page.number_of_keys++;
		right_left_most_pagenum = cache->frame.base.internal_page.internal_record[0].page_number;
		for_insert_to_parent = cache->frame.base.internal_page.internal_record[0].key;
		//�� ù��°�� ����
		for (int i = 0; i < cache->frame.base.internal_page.number_of_keys - 1; ++i)
		{
			cache->frame.base.internal_page.internal_record[i] = cache->frame.base.internal_page.internal_record[i + 1];
		}
		cache->frame.base.internal_page.number_of_keys--;
		buffer_set_dirty(cache);
		buffer_unpinned(cache);
	}

	//������ ������ left most page �Է�
	cache = buffer_read_page(table_id, new_right_page, cache);
	cache->frame.base.internal_page.left_most_page_number = right_left_most_pagenum;
	right_number_of_key = cache->frame.base.internal_page.number_of_keys;
	buffer_set_dirty(cache);
	buffer_unpinned(cache);

	//������ �������� �ڽ� �������� �θ������� �ٲ��ֱ�
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
	//�ɰ������� �θ�������(=internal page)�� insert
	return insert_to_parent_page(table_id, right_parent_page, for_insert_to_parent, pagenum, new_right_page);
}

//���� ��� ������ �迭�� ùĭ ��ȯ
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
//key�� �����־�� �ϴ� leaf page ã�Ƽ� ��ȯ
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
		//���� �������� �´ٸ�
		if (is_leaf == 1)
		{
			buffer_unpinned(cache);
			return root;
		}
		else//���������� �ƴ� ��
		{
			//left most �ΰ��
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
	//ã�� ����
	return -1;
}
//���ڵ� �����
record *make_record(int64_t key, char* value)
{
	record* new_record = (char*)malloc(sizeof(record));
	new_record->key = key;
	strcpy(new_record->value, value);
	return new_record;
}
//internal root page�� ������ �ѹ� ���
pagenum_t get_root_internal_page(int table_id)
{
	buffer* cache = NULL;

	pagenum_t root;
	//��� ������ �б�
	cache = buffer_read_page(table_id, 0, cache);
	root = cache->frame.base.header_page.root_page_number;
	buffer_unpinned(cache);

	return root;
}
//�ش� �������� height ���
int get_height(int table_id, pagenum_t pagenum)
{
	buffer* cache = NULL;
	pagenum_t root_pagenum = get_root_internal_page(table_id);
	int height = 0;
	while (root_pagenum != pagenum)
	{
		//root ������ �ѹ��� �ٸ��� �а�
		cache = buffer_read_page(table_id, pagenum, cache);
		//�θ� ������ �ѹ��� �̵�
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
		//���� �������� �´ٸ�
		if (is_leaf == 1)
		{
			//���������� �ȿ��� key�� ã��
			int i;
			for (i = 0; i < number_of_key; ++i)
			{
				//ã�°� ���� ���
				if (cache->frame.base.leaf_page.records[i].key == key)
				{
					buffer_unpinned(cache);
					
					

					//buffer�� ���� mutex�� ����� ���
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

						//record lock�� ���������� �������� find
						cache = buffer_read_page(table_id, root, cache);

						//���������� �ȿ��� key�� ã��
						int i;
						for (i = 0; i < number_of_key; ++i)
						{
							//ã�°� ���� ���
							if (cache->frame.base.leaf_page.records[i].key == key)
							{
								strcpy(ret_val, cache->frame.base.leaf_page.records[i].value);

								buffer_unpinned(cache);
								pthread_mutex_unlock(&cache->buf_sys_mutex);

								return 0;

							}
						}

						//ã�°� ���� ���
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

			//ã�� ���� ���� ���
			buffer_unpinned(cache);
			pthread_mutex_unlock(&buffer_pools.buf_pool_sys_mutex);

			return -1;
		}
		else//���������� �ƴ� ��
		{
			//left most page �� ��
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
	//ã�� ����
	return -1;
}
//������ ������
int db_delete(int table_id, int64_t key, int trx_id)
{
	buffer* cache;

	pagenum_t root_internal_page = get_root_internal_page(table_id);
	//���� root�� �������� �ʾ��� ���
	if (root_internal_page == NULL)
	{
		return -1;
	}
	//���� ���� ���
	char* new_ = (char*)malloc(sizeof(record));
	if (db_find(table_id, key, new_, trx_id) != 0) return -1;
	free(new_);
	pagenum_t leaf_pagenum = find_leafpage(table_id, key);
	return delete_entry(table_id, leaf_pagenum, key);
}


//entry�ȿ� ���ִ�
int delete_entry(int table_id, pagenum_t pagenum, int64_t key)
{
	buffer* cache = NULL;

	cache = buffer_read_page(table_id, pagenum, cache);
	//���� ������ �� �� ����
	if (cache->frame.base.internal_page.is_leaf == 1)
	{
		//leaf �������� Ű ����
		int32_t num_of_key = cache->frame.base.leaf_page.number_of_keys;
		int deletion_point;
		for (deletion_point = 0; deletion_point < num_of_key; ++deletion_point)
		{
			//�ش� Ű ������ �����ϰ� ����������
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
	else //���ͳ��������� �� ����
	{
		//leaf �������� Ű ����
		int32_t num_of_key = cache->frame.base.internal_page.number_of_keys;
		int deletion_point;
		for (deletion_point = 0; deletion_point < num_of_key; ++deletion_point)
		{
			//�ش� Ű ������ �����ϰ� ����������
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


	//��Ʈ�� ���
	cache = buffer_read_page(table_id, pagenum, cache);
	if (cache->frame.base.internal_page.parent_page_number == NULL)
	{
		buffer_unpinned(cache);
		return adjust_root(table_id, pagenum);
	}
	buffer_unpinned(cache);

	//underflow�� ��
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
		//�θ������� ���

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
	else //not underflow�� ��
	{
		int64_t for_change_key = cache->frame.base.leaf_page.records[0].key;
		pagenum_t parent_page = cache->frame.base.leaf_page.parent_page_number;
		buffer_unpinned(cache);

		//�� �̻� ���� �� ���� �� 0
		int find = 1;
		while (find == 1)
		{
			//�θ� ���� Ž���ϸ鼭 �� ����
			cache = buffer_read_page(table_id, parent_page, cache);
			int32_t num_of_key = cache->frame.base.internal_page.number_of_keys;
			//���� �� ������ ����
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
	//empty �� ���
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
	//page�� leftmost child�̸�
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
	//������������ �ƴϸ�
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

			//left most page�� ���̹��� ����Ű���� ����
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

//pagenum�� leftmost�� �� -2 ��ȯ
//-1�� leftmost�� ��Ÿ���� �ε����̴�.
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
//��Ʈ���������� ���� ��ȯ
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
//level order�� ���� queue ����
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
//level order�� ���� deque
pagenum_t dequeue()
{
	node * n = queue;
	queue = queue->next;
	n->next = NULL;
	return n->here;
}
//print�� ���� Ʈ��
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

		//�ش� ������ �а� print
		cache = buffer_read_page(table_id, page, cache);
		int32_t number_of_record = cache->frame.base.internal_page.number_of_keys;
		if (number_of_record == 0)
		{
			buffer_unpinned(cache);
			continue;
		}
		//internal page �� ��
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
		else//���������� �� ��
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

/* �ѤѤѤѤѤѤѤѤѤѤѤ����� API �ѤѤѤѤѤѤѤѤѤѤѤѤѤ�*/
pagenum_t file_alloc_page(int table_id)
{
	//���ۿ� ����Ű�� ������
	buffer* cache = NULL;

	pagenum_t first_free_page_number, second_free_page_number;

	//��� �������� free page �б�
	cache = buffer_read_page(table_id, 0, cache);
	first_free_page_number = cache->frame.base.header_page.free_page_number;
	buffer_unpinned(cache);

	//���� �� �Ҵ�������
	if (first_free_page_number == 0)
	{
		printf("���������� �߰� �Ҵ�\n");
		cache = buffer_read_page(table_id, 0, cache);
		pagenum_t num_of_page = cache->frame.base.header_page.number_of_pages;
		//����������� ����Ű�� ���������� �Ҵ�
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
		//������ free page�� ����Ű�°� 0���� �Ѵ�.
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
	


	//�ι�° free page num �ҷ�����
	cache = buffer_read_page(table_id, first_free_page_number, cache);
	second_free_page_number = cache->frame.base.free_page.next_free_page_number;
	buffer_unpinned(cache);

	//��� �������� second_free_page_number�� ����Ű�� ����
	cache = buffer_read_page(table_id, 0, cache);
	cache->frame.base.header_page.free_page_number = second_free_page_number;
	buffer_set_dirty(cache);
	buffer_unpinned(cache);

	//�Ҵ��� free_page �Ѱ��ֱ�
	return first_free_page_number;
}

void file_free_page(int table_id, pagenum_t pagenum)
{
	buffer* cache = NULL;

	pagenum_t next_page_num = 0, first_page_number = 0;
	//����� ����Ű�� ù��° free page ����
	cache = buffer_read_page(table_id, 0, cache);
	first_page_number = cache->frame.base.header_page.free_page_number;
	//����������� ����Ű�� free page�� pagenum�� ����Ű�� ��
	cache->frame.base.header_page.free_page_number = pagenum;
	buffer_set_dirty(cache);
	buffer_unpinned(cache);

	//����ִ� 4096Byte�� ���ڿ�
	page_t empty;
	file_write_page(table_id, pagenum, &empty);

	//�ش� �������� first_page_number�� ����Ű�� �ϱ�
	cache = buffer_read_page(table_id, pagenum, cache);
	cache->frame.base.free_page.next_free_page_number = first_page_number;
	buffer_set_dirty(cache);
	buffer_unpinned(cache);
}

void file_read_page(int table_id, pagenum_t pagenum, page_t* dest)
{

	/* �տ� �ش� table_id�� �ش��ϴ� ������ ����� �Ѵ�*/
	file_pointer = fopen(table_id_pool.table[table_id].path_name, "rb+");
	if (file_pointer == NULL)
	{
		printf("table_id %d, pagenum %d\n", table_id, pagenum);
		printf("������ pathname : %s\n", table_id_pool.table[table_id].path_name);
		printf("������ ������ �ʾҽ��ϴ�. (file_read_page)\n");
		return;
	}
	fseek(file_pointer, PAGE_SIZE * pagenum, SEEK_SET);
	fread(dest, sizeof(page_t), 1, file_pointer);
	fclose(file_pointer);
}

void file_write_page(int table_id, pagenum_t pagenum, const page_t* src)
{
	/* �տ� �ش� table_id�� �ش��ϴ� ������ ����� �Ѵ�*/
	file_pointer = fopen(table_id_pool.table[table_id].path_name, "rb+");
	if (file_pointer == NULL)
	{
		printf("������ ������ �ʾҽ��ϴ�. (file_read_page)\n");
		return;
	}

	//�ش� �������� �����ؼ� src�� ����
	fseek(file_pointer, PAGE_SIZE * pagenum, SEEK_SET);
	fwrite(src, sizeof(page_t), 1, file_pointer);
	fclose(file_pointer);
}
/*�ѤѤѤѤѤѤѤѤѤѤѤ����� API �� �ѤѤѤѤѤѤѤѤѤѤѤѤѤ�*/


/*�ѤѤѤѤѤѤѤѤѤѤѤѹ��� API ���ۤѤѤѤѤѤѤѤѤѤѤѤѤѤ�*/
int init_db(int num_buf)
{

	//���� pool �ʱ�ȭ
	int i;
	buffer_pools.buffer_pool = (buffer*)malloc(num_buf * sizeof(buffer));

	//������ �ȵǾ�����
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
	
	

	//���̺� id pool �ʱ�ȭ
	table_id_pool.size = 100;

	for (int i = 1; i < table_id_pool.size; ++i)
	{
		table_id_pool.table[i].table_id = i;
		table_id_pool.table[i].used = false;
	}

	return 0;
}

/*pinned���¸� �ϳ� �����ش�*/
void buffer_unpinned(buffer* src)
{
	src->is_pinned -= 1;
}

/*dirty�� ���·� �����*/
void buffer_set_dirty(buffer* src)
{
	src->is_dirty = true;
}

//table_id�� pagenum�� �ش��ϴ� ���۸� ã�� dest�� �Ҵ� ��Ų��.
buffer* buffer_read_page(int table_id, pagenum_t pagenum, buffer* dest)
{

	/*printf("table_id = %d, pagenum = %d\n", table_id, pagenum);
	for (int i = 0; i < buffer_pools.size; ++i)
	{
		printf("%d, unpinned %d, used %d\n", i, buffer_pools.buffer_pool[i].is_pinned,buffer_pools.buffer_pool[i].table_id);
	}
	printf("\n");*/
	//dest�� null�� �ʱ�ȭ
	dest = NULL;

	//ã�� ���� ����Ǯ�ȿ��� ã��
	int i;
	for (i = 1; i < buffer_pools.size; ++i)
	{
		//ã���� dest�� �ش� frame���� ����Ű��
		if (buffer_pools.buffer_pool[i].page_num == pagenum && buffer_pools.buffer_pool[i].table_id == table_id)
		{
			buffer_pools.buffer_pool[i].is_pinned += 1;
			return &buffer_pools.buffer_pool[i];
		}
	}

	//���ۿ� �´� �������� ������ ����Ǯ�� �� ������ �о� ����
	if (dest == NULL)
	{
		//Clock Policy
		//victim�� ã���� ���� ������.
		while (dest == NULL)
		{
			//������� �ʴ� ���� �߰��Ѵٸ�
			if (buffer_pools.selected->is_pinned == 0)
			{
				//ref�� �����ִٸ�
				if (buffer_pools.selected->ref == true)
				{
					//ref�� ����� �������� �Ѿ��.
					buffer_pools.selected->ref = false;
					buffer_pools.selected = &buffer_pools.buffer_pool[buffer_pools.selected->next_buffer];
					continue;
				}

				//dirty�� ������ ���������
				if (buffer_pools.selected->is_dirty == true)
				{
					//clean �������� ������
					file_write_page(buffer_pools.selected->table_id, buffer_pools.selected->page_num, &(buffer_pools.selected->frame));
					buffer_pools.selected->is_dirty = false;
				}

				//clean page�̰� ref�� �����ִٸ� �ҷ�����
				dest = buffer_pools.selected;
			}
			else
			{
				//���� ����ϴ� ���� �߰��ϸ� �������� �Ѿ��
				buffer_pools.selected = &buffer_pools.buffer_pool[buffer_pools.selected->next_buffer];
			}
		}

		//victim�� �ش��ϴ� ������ ����
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
	//�Ҵ��� ���� �������ѹ�
	pagenum_t allocated_pagenum = file_alloc_page(table_id);

	return allocated_pagenum;
}

void buffer_free_page(int table_id, pagenum_t pagenum)
{
	file_free_page(table_id, pagenum);
}

int close_table(int table_id)
{
	//����Ǯ�� �ش��ϴ� table_id�� ������ �ִ��� Ȯ��
	for (int i = 0; i < buffer_pools.size; ++i)
	{
		//ã����
		if (buffer_pools.buffer_pool[i].table_id == table_id)
		{
			//���� pinned ������ ���۰� ������
			if (buffer_pools.buffer_pool[i].is_pinned > 0)
			{
				//close �ȉ�ٰ� ��ȯ
				return -1;
			}

			//ref����
			buffer_pools.buffer_pool[i].ref = 0;
			if (buffer_pools.buffer_pool[i].is_dirty == 1)
			{
				//clean page�� �����
				file_write_page(table_id, buffer_pools.buffer_pool[i].page_num, &(buffer_pools.buffer_pool[i].frame));
				buffer_pools.buffer_pool[i].is_dirty = false;
			}
		}
	}

	//table_id ����
	table_id_pool.table[table_id].used = false;

	return 0;
}

int shutdown_db(void)
{
	//����Ǯ ���ư��鼭 üũ
	for (int i = 0; i < buffer_pools.size; ++i)
	{
		//���� pinned ������ ���۰� ������
		if (buffer_pools.buffer_pool[i].is_pinned > 0)
		{
			//close �ȉ�ٰ� ��ȯ
			return -1;
		}

		//ref����
		buffer_pools.buffer_pool[i].ref = 0;
		if (buffer_pools.buffer_pool[i].is_dirty == 1)
		{
			//clean page�� �����
			file_write_page(buffer_pools.buffer_pool[i].table_id, buffer_pools.buffer_pool[i].page_num, &buffer_pools.buffer_pool[i].frame);
			buffer_pools.buffer_pool[i].is_dirty = false;
		}
	}

	buffer_pools.size = 0;
	buffer_pools.selected = NULL;
	free(buffer_pools.buffer_pool);
	return 0;
}

/*�ѤѤѤѤѤѤѤѤѤѤѤѹ��� API ���ѤѤѤѤѤѤѤѤѤѤѤѤѤ�*/

/*�ѤѤѤѤѤѤѤѤѤѤѤ����� API �ѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤ�*/

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
		//���� �������� �´ٸ�
		if (is_leaf == 1)
		{
			buffer_unpinned(cache);
			return root;
		}
		else//���������� �ƴ� ��
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



/*�ѤѤѤѤѤѤѤѤѤѤѤ����� API ���ѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤ�*/



/*�ѤѤѤѤѤѤѤѤѤѤѤ�Transaction ���ѤѤѤѤѤѤѤѤѤѤѤѤѤ�*/

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
			printf("������ ���̺� ���̵� = %d\n", table_id);
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
				printf("�ݾƾ� �� ���̺��� �����ϴ�.\n");
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
				db_insert(table_id, key, "�׽�Ʈ ���Դϴ�.", 0);

				if ((i + 1) % 100 == 0)
				{
					printf("%d ��° %d inserted\n", i + 1, key);

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


				printf("������1 ���ۻ��� %d\n", pthread_create(&pthreads[0], NULL, &pthread1, (void *)table_id));
	
				printf("������2 ���ۻ��� %d\n", pthread_create(&pthreads[1], NULL, &pthread2, (void *)table_id));

				printf("������ 1 ������� %d\n", pthread_join(pthreads[0], (void**)return1));

				printf("������ 2 ������� %d\n", pthread_join(pthreads[1], (void**)return2));


			}


			break;
		}
		default:
			break;
		}
	}

	return 0;
}