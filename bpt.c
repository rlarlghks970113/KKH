#include "bpt.h"


int open_table(char* pathname)
{
	int64_t file_size, table_id = unique_table_id++;

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
		file_read_page(0, &page_data);
		page_data.base.header_page.free_page_number = 1;
		page_data.base.header_page.root_page_number = NULL;
		//�Ҵ��� free page�� 256 * 10(==10mb) + header page
		page_data.base.header_page.number_of_pages = 256 * 10 + 1;
		file_write_page(0, &page_data);

		//free page�����
		pagenum_t i;
		for (i = 1; i < 256 * 10; ++i)
		{
			file_read_page(i, &page_data);
			page_data.base.free_page.next_free_page_number = i + 1;
			file_write_page(i, &page_data);
		}

		//������ free page�� ����Ű�°� 0���� �Ѵ�.
		file_read_page(i, &page_data);

		page_data.base.free_page.next_free_page_number = 0;
		file_write_page(i, &page_data);
	}

	return table_id;
}




//�����Լ�
int db_insert(int64_t key, char* value)
{
	pagenum_t root_internal_page = get_root_internal_page();

	//insert�� ó���� ���
	if (root_internal_page == NULL)
	{
		//ù leaf page ����
		pagenum_t first_leaf_page = file_alloc_page();
		file_read_page(first_leaf_page, &page_data);
		record record_pair = *make_record(key, value);

		page_data.base.leaf_page.is_leaf = 1;
		page_data.base.leaf_page.parent_page_number = NULL;
		page_data.base.leaf_page.number_of_keys = 1;

		page_data.base.leaf_page.records[0] = record_pair;
		page_data.base.leaf_page.right_sibling_page_number = 0;
		file_write_page(first_leaf_page, &page_data);


		//����������� ��Ʈ������ �Է�
		file_read_page(0, &page_data);
		page_data.base.header_page.root_page_number = first_leaf_page;
		file_write_page(0, &page_data);
		return 0;
	}
	//�̹� ���� ���
	char* new = (char*)malloc(sizeof(record));
	if (db_find(key, new) == 0) return -1;
	free(new);

	pagenum_t leaf_pagenum = find_leafpage(key);

	file_read_page(leaf_pagenum, &page_data);
	//ORDER - 2 ���� �϶�(=�� �ɰ��� �� ��)
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

	//insertion ��ġ
	int insertion_point = page_data.base.leaf_page.number_of_keys;


	//���ڵ� ����
	while (insertion_point > 0)
	{
		if (page_data.base.leaf_page.records[insertion_point - 1].key < new_record.key) break;

		//�����ʿ� ����
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

	//���������� �ű� �ڷ�� ����
	pagenum_t new_parent_pagenum = page_data.base.leaf_page.parent_page_number;
	int32_t new_number_of_keys = ORDER - 1 - cut();
	record* new_right_record = (record*)malloc(sizeof(record)*(ORDER - 1));
	pagenum_t new_right_sibling = page_data.base.leaf_page.right_sibling_page_number;

	int i = 0;
	for (int j = cut(); j < ORDER - 1;)
	{
		new_right_record[i++] = page_data.base.leaf_page.records[j++];
	}


	//���� ������ ����
	page_data.base.leaf_page.number_of_keys = cut();
	left_last = page_data.base.leaf_page.records[page_data.base.leaf_page.number_of_keys - 1].key;

	file_write_page(pagenum, &page_data);

	//������ ������ ����� �� ����
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

	//������������ ���ư��� right sibling �� ����
	file_read_page(pagenum, &page_data);
	page_data.base.leaf_page.right_sibling_page_number = new_right_pagenum;
	file_write_page(pagenum, &page_data);



	//������ key�� ����
	if (left_last < new_record.key)
	{
		file_read_page(new_right_pagenum, &page_data);
		//insertion ��ġ
		int insertion_point = page_data.base.leaf_page.number_of_keys;

		//���ڵ� ����
		while (insertion_point > 0)
		{
			if (page_data.base.leaf_page.records[insertion_point - 1].key < new_record.key) break;

			//�����ʿ� ����
			page_data.base.leaf_page.records[insertion_point] = page_data.base.leaf_page.records[insertion_point - 1];
			insertion_point--;
		}
		page_data.base.leaf_page.records[insertion_point] = new_record;
		page_data.base.leaf_page.number_of_keys++;

		for_insert_key = page_data.base.leaf_page.records[0].key;
		file_write_page(new_right_pagenum, &page_data);
	}
	else if (left_last > new_record.key)//���� ������ �� ����
	{
		file_read_page(pagenum, &page_data);
		//insertion ��ġ
		int insertion_point = page_data.base.leaf_page.number_of_keys;

		//���ڵ� ����
		while (insertion_point > 0)
		{
			if (page_data.base.leaf_page.records[insertion_point - 1].key < new_record.key) break;

			//�����ʿ� ����
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



		//�����ʿ� ����
		file_read_page(new_right_pagenum, &page_data);
		//insertion ��ġ
		insertion_point = page_data.base.leaf_page.number_of_keys;

		//���ڵ� ����
		while (insertion_point > 0)
		{
			if (page_data.base.leaf_page.records[insertion_point - 1].key < for_insert_key) break;

			//�����ʿ� ����
			page_data.base.leaf_page.records[insertion_point] = page_data.base.leaf_page.records[insertion_point - 1];
			insertion_point--;
		}
		page_data.base.leaf_page.records[insertion_point].key = for_insert_key;
		strcpy(page_data.base.leaf_page.records[insertion_point].value, value);

		page_data.base.leaf_page.number_of_keys++;

		for_insert_key = page_data.base.leaf_page.records[0].key;
		file_write_page(new_right_pagenum, &page_data);
	}




	//�ɰ������� �θ�������(=internal page)�� insert
	return insert_to_parent_page(new_parent_pagenum, for_insert_key, pagenum, new_right_pagenum);
}

pagenum_t insert_to_parent_page(pagenum_t parent_pagenum, int64_t key, pagenum_t child_left, pagenum_t child_right)
{
	//��Ʈ ������ �϶�
	if (parent_pagenum == NULL)
	{
		pagenum_t new_root_pagenum = file_alloc_page();

		file_read_page(new_root_pagenum, &page_data);
		//�� ��Ʈ ������ �� �Ҵ�(���� left_most�� �Ҵ� ����)
		page_data.base.internal_page.is_leaf = 0;
		page_data.base.internal_page.number_of_keys = 1;
		page_data.base.internal_page.parent_page_number = NULL;
		page_data.base.internal_page.internal_record[0].key = key;
		page_data.base.internal_page.left_most_page_number = child_left;
		page_data.base.internal_page.internal_record[0].page_number = child_right;
		file_write_page(new_root_pagenum, &page_data);

		//��� �ҷ���
		file_read_page(0, &page_data);
		page_data.base.header_page.root_page_number = new_root_pagenum;
		file_write_page(0, &page_data);

		//�ڽ� ���������� �θ� ������ �ٲ��ֱ�
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

	//���ڵ��� ������ ���� �־ ������ ���� ��
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

	//�ϴ� insert
	internal_record internal_record = { key, child_pagenum };

	//insertion ��ġ
	int insertion_point = page_data.base.leaf_page.number_of_keys;

	//���ڵ� ����
	while (insertion_point > 0)
	{

		if (page_data.base.internal_page.internal_record[insertion_point - 1].key < key) break;

		//�����ʿ� ����
		page_data.base.internal_page.internal_record[insertion_point] = page_data.base.internal_page.internal_record[insertion_point - 1];
		insertion_point--;
	}
	page_data.base.internal_page.internal_record[insertion_point] = internal_record;

	//������ ����
	page_data.base.internal_page.number_of_keys++;

	file_write_page(pagenum, &page_data);

	return 0;
}

//insert to internal page after split
pagenum_t insert_to_internal_page_after_split(pagenum_t pagenum, int64_t key, pagenum_t child_pagenum)
{
	//������ ù��°, ���� ������ ����
	int64_t right_first, left_last;

	file_read_page(pagenum, &page_data);

	//internal page�� ��� ������ 0~ (split-1), �������� (split+1)~(ORDER-1)
	//������ ������ ���� ���� ����
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
	//���� ������ ���� �� ����
	page_data.base.internal_page.number_of_keys = split;
	file_write_page(pagenum, &page_data);


	//������ ������ �Ҵ�
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


	//insert �κ�
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
		//��� �� ��
		right_left_most_pagenum = child_pagenum;
		for_insert_to_parent = key;


	}
	else if (left_last > key)
	{
		//�� �� �� �� ��
		file_read_page(pagenum, &page_data);
		internal_record internal_record_ = { key, child_pagenum };

		int insertion_point = page_data.base.internal_page.number_of_keys;
		while (insertion_point > 0)
		{
			if (page_data.base.internal_page.internal_record[insertion_point - 1].key < key) break;
			page_data.base.internal_page.internal_record[insertion_point] = page_data.base.internal_page.internal_record[insertion_point - 1];
			insertion_point--;
		}

		//�´� ��ġ�� ����
		page_data.base.internal_page.internal_record[insertion_point] = internal_record_;
		//���� �ø���
		page_data.base.internal_page.number_of_keys++;

		right_left_most_pagenum = page_data.base.internal_page.internal_record[page_data.base.internal_page.number_of_keys - 1].page_number;
		//�� ������ �� ����
		for_insert_to_parent = page_data.base.internal_page.internal_record[page_data.base.internal_page.number_of_keys - 1].key;
		page_data.base.internal_page.number_of_keys--;
		file_write_page(pagenum, &page_data);
	}
	else if (right_first < key)
	{
		//�� ������ �� ��
		internal_record internal_record_ = { key, child_pagenum };

		int insertion_point = page_data.base.internal_page.number_of_keys;
		while (insertion_point > 0)
		{
			if (page_data.base.internal_page.internal_record[insertion_point - 1].key < key) break;
			page_data.base.internal_page.internal_record[insertion_point] = page_data.base.internal_page.internal_record[insertion_point - 1];

			insertion_point--;
		}

		//�´� ��ġ�� ����
		page_data.base.internal_page.internal_record[insertion_point] = internal_record_;
		//���� �ø���
		page_data.base.internal_page.number_of_keys++;

		right_left_most_pagenum = page_data.base.internal_page.internal_record[0].page_number;
		for_insert_to_parent = page_data.base.internal_page.internal_record[0].key;

		//�� ù��°�� ����
		for (int i = 0; i < page_data.base.internal_page.number_of_keys - 1; ++i)
		{
			page_data.base.internal_page.internal_record[i] = page_data.base.internal_page.internal_record[i + 1];
		}
		page_data.base.internal_page.number_of_keys--;
		file_write_page(new_right_page, &page_data);
	}

	//������ ������ left most page �Է�
	file_read_page(new_right_page, &page_data);
	page_data.base.internal_page.left_most_page_number = right_left_most_pagenum;
	right_number_of_key = page_data.base.internal_page.number_of_keys;
	file_write_page(new_right_page, &page_data);

	//������ �������� �ڽ� �������� �θ������� �ٲ��ֱ�

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


	//�ɰ������� �θ�������(=internal page)�� insert
	return insert_to_parent_page(right_parent_page, for_insert_to_parent, pagenum, new_right_page);
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
pagenum_t find_leafpage(int64_t key)
{
	pagenum_t root = get_root_internal_page();

	while (1)
	{
		file_read_page(root, &page_data);
		int is_leaf = page_data.base.internal_page.is_leaf;
		int32_t number_of_key = page_data.base.internal_page.number_of_keys;
		//���� �������� �´ٸ�
		if (is_leaf == 1)
		{
			return root;
		}
		else//���������� �ƴ� ��
		{
			//left most �ΰ��
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
pagenum_t get_root_internal_page()
{
	pagenum_t root;
	//��� ������ �б�
	file_read_page(0, &page_data);

	root = page_data.base.header_page.root_page_number;
	return root;
}

//�ش� �������� height ���
int get_height(pagenum_t pagenum)
{
	pagenum_t root_pagenum = get_root_internal_page();
	int height = 0;

	while (root_pagenum != pagenum)
	{
		//root ������ �ѹ��� �ٸ��� �а�
		file_read_page(pagenum, &page_data);

		//�θ� ������ �ѹ��� �̵�
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
		//���� �������� �´ٸ�
		if (is_leaf == 1)
		{
			//���������� �ȿ��� key�� ã��
			int i;
			for (i = 0; i < number_of_key; ++i)
			{
				//ã�°� ���� ���
				if (page_data.base.leaf_page.records[i].key == key)
				{
					strcpy(ret_val, page_data.base.leaf_page.records[i].value);
					return 0;
				}
			}
			//ã�� ���� ���� ���
			return -1;
		}
		else//���������� �ƴ� ��
		{
			//left most page �� ��
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
	//ã�� ����
	return -1;
}

//������ ������
int db_delete(int64_t key)
{
	pagenum_t root_internal_page = get_root_internal_page();

	//���� root�� �������� �ʾ��� ���
	if (root_internal_page == NULL)
	{
		return -1;
	}
	//���� ���� ���
	char* new = (char*)malloc(sizeof(record));
	if (db_find(key, new) != 0) return -1;
	free(new);

	pagenum_t leaf_pagenum = find_leafpage(key);

	return delete_entry(leaf_pagenum, key);
}

int delete_entry(pagenum_t pagenum, int64_t key)
{
	file_read_page(pagenum, &page_data);

	//���� ������ �� �� ����
	if (page_data.base.internal_page.is_leaf == 1)
	{
		//leaf �������� Ű ����
		int32_t num_of_key = page_data.base.leaf_page.number_of_keys;
		int deletion_point;
		for (deletion_point = 0; deletion_point < num_of_key; ++deletion_point)
		{
			//�ش� Ű ������ �����ϰ� ����������
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
	else //���ͳ��������� �� ����
	{
		//leaf �������� Ű ����
		int32_t num_of_key = page_data.base.internal_page.number_of_keys;
		int deletion_point;
		for (deletion_point = 0; deletion_point < num_of_key; ++deletion_point)
		{
			//�ش� Ű ������ �����ϰ� ����������
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


	//��Ʈ�� ���

	file_read_page(pagenum, &page_data);

	if (page_data.base.internal_page.parent_page_number == NULL)
	{
		return adjust_root(pagenum);
	}


	//underflow�� ��
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
		//�θ������� ���
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
	else //not underflow�� ��
	{
		int64_t for_change_key = page_data.base.leaf_page.records[0].key;
		pagenum_t parent_page = page_data.base.leaf_page.parent_page_number;
		file_write_page(pagenum, &page_data);

		//�� �̻� ���� �� ���� �� 0
		int find = 1;
		while (find == 1)
		{
			//�θ� ���� Ž���ϸ鼭 �� ����
			file_read_page(parent_page, &page_data);
			int32_t num_of_key = page_data.base.internal_page.number_of_keys;

			//���� �� ������ ����
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

	//empty �� ���
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

	//page�� leftmost child�̸�
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

	//������������ �ƴϸ�
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

			//left most page�� ���̹��� ����Ű���� ����
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

//pagenum�� leftmost�� �� -2 ��ȯ
//-1�� leftmost�� ��Ÿ���� �ε����̴�.
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



//��Ʈ���������� ���� ��ȯ
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

		//�ش� ������ �а� print
		file_read_page(page, &page_data);
		int32_t number_of_record = page_data.base.internal_page.number_of_keys;
		if (number_of_record == 0) continue;
		//internal page �� ��
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
		else//���������� �� ��
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

	printf("���̺� ���̵� %d\n", table_id);

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

			printf("����������? -> %d\n", db_find(input_key, value));
			printf("��ȯ�� value : %s\n", value);
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