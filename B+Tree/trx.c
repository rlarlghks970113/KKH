#include "bpt.h"

/*ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡTransactionㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ*/


int hash_func(const char * str, int max_number)
{
	int hash = 401;
	int c;

	while (*str != '\0') {
		hash = ((hash << 4) + (int)(*str)) % max_number;
		str++;
	}

	return hash % max_number;
}


int db_update(int table_id, int64_t key, char* values, int trx_id)
{

	if (trx_manager.trx_table[trx_id].state == IDLE)
	{
		return -1;
	}

	pagenum_t leaf_pagenum;
	buffer* cache = NULL;
	while (true)
	{
		//acquired buffer_pool latch
		pthread_mutex_lock(&buffer_pools.buf_pool_sys_mutex);

		leaf_pagenum = find_leafpage(table_id, key);

		cache = buffer_read_page(table_id, leaf_pagenum, cache);



		if (pthread_mutex_trylock(&cache->buf_sys_mutex) == 0) {
			//if success
			pthread_mutex_unlock(&buffer_pools.buf_pool_sys_mutex);
			break;
		}
		else
		{
			//if fail to acquire buffer_page latch
			//go to the first line of while loop
			buffer_unpinned(cache);
			pthread_mutex_unlock(&buffer_pools.buf_pool_sys_mutex);
			continue;
		}
	}

	/*if acquire buffer_page latch*/
	int i;
	for (i = 0; i < cache->frame.base.leaf_page.number_of_keys; ++i)
	{
		if (cache->frame.base.leaf_page.records[i].key == key)
		{
			break;
		}
	}

	//값이 없을 경우, 실패
	if (i == cache->frame.base.leaf_page.number_of_keys)
	{
		buffer_unpinned(cache);
		pthread_mutex_unlock(&cache->buf_sys_mutex);
		return -1;
	}

	//if there is value matching a key 
	enum return_value rvalue = acquire_record_lock(table_id, leaf_pagenum, key, trx_id, EXCLUSIVE, i);

	if (rvalue == DEADLOCK)
	{
		buffer_unpinned(cache);
		pthread_mutex_unlock(&cache->buf_sys_mutex);



		abort_trx(trx_id);
		end_trx(trx_id);
		//FAIL....
		return -1;
	}
	else if (rvalue == CONFLICT)
	{
		buffer_unpinned(cache);
		pthread_mutex_unlock(&cache->buf_sys_mutex);


		//after waken;

		return db_update(table_id, key, values, trx_id);
	}

	//if success to acquire lock, keep going

	//리프페이지 안에서 key값 찾기
	for (i = 0; i < cache->frame.base.leaf_page.number_of_keys; ++i)
	{
		//찾는게 있을 경우
		if (cache->frame.base.leaf_page.records[i].key == key)
		{


			undo_log_t* tmp = trx_manager.trx_table[trx_id].undo_log_list;

			trx_manager.trx_table[trx_id].undo_log_list = (undo_log_t*)malloc(sizeof(struct undo_log_t));

			trx_manager.trx_table[trx_id].undo_log_list->key = key;
			strcpy(trx_manager.trx_table[trx_id].undo_log_list->old_value, cache->frame.base.leaf_page.records[i].value);
			trx_manager.trx_table[trx_id].undo_log_list->table_id = table_id;

			trx_manager.trx_table[trx_id].undo_log_list->next_undo = tmp;

			strcpy(cache->frame.base.leaf_page.records[i].value, values);


			buffer_set_dirty(cache);
			buffer_unpinned(cache);
			pthread_mutex_unlock(&cache->buf_sys_mutex);

			return 0;

		}
	}

	pthread_mutex_unlock(&cache->buf_sys_mutex);


	//찾는게 없을 경우
	return -1;
}


int begin_trx()
{
	//trx가 실행되는 순서 제어
	pthread_mutex_lock(&trx_manager.trx_sys_mutex);

	//만약 trx_id가 이미 할당되어 있다면 trx을 시작할수 없다고 0반환
	while (trx_manager.trx_table[trx_manager.next_trx_id].state != IDLE)
	{
		trx_manager.next_trx_id++;

		if (trx_manager.next_trx_id > MAX_TRX_NUM)
		{
			trx_manager.next_trx_id = 0;
		}
		pthread_mutex_unlock(&trx_manager.trx_sys_mutex);
		return 0;
	}



	//trx_id 를 할당할 수 있다면 할당
	int trx_id = trx_manager.next_trx_id++;



	//trx_structure을 할당
	trx_manager.trx_table[trx_id].trx_id = trx_id;
	pthread_cond_init(&trx_manager.trx_table[trx_id].trx_cond, NULL);
	pthread_mutex_init(&trx_manager.trx_table[trx_id].trx_mutex, NULL);

	trx_manager.trx_table[trx_id].trx_locks = NULL;
	trx_manager.trx_table[trx_id].undo_log_list = NULL;
	trx_manager.trx_table[trx_id].wait_lock = NULL;

	trx_manager.trx_table[trx_id].state = WAITING;

	//trx_sys_mutex를 release
	pthread_mutex_unlock(&trx_manager.trx_sys_mutex);

	return trx_id;
}

int end_trx(int trx_id)
{
	if (trx_manager.trx_table[trx_id].state == IDLE) return -1;


	pthread_mutex_lock(&lock_manager.lock_sys_mutex);

	pthread_cond_broadcast(&trx_manager.trx_table[trx_id].trx_cond);

	pthread_mutex_unlock(&lock_manager.lock_sys_mutex);





	//trx_mutex잡기
	pthread_mutex_lock(&trx_manager.trx_sys_mutex);

	lock_t* tmp_lock = trx_manager.trx_table[trx_id].trx_locks;
	lock_t* next_lock;

	trx_manager.trx_table[trx_id].state = IDLE;


	while (tmp_lock != NULL)
	{
		next_lock = tmp_lock->down;

		if (tmp_lock->next != NULL)
		{
			tmp_lock->next->prev = tmp_lock->prev;
		}

		if (tmp_lock->prev != NULL)
		{
			tmp_lock->prev->next = tmp_lock->next;
		}
		else
		{
			buffer* cache = NULL;

			cache = buffer_read_page(tmp_lock->table_id, tmp_lock->page_id, cache);
			int i;
			for (i = 0; i < cache->frame.base.leaf_page.number_of_keys; ++i)
			{
				if (cache->frame.base.leaf_page.records[i].key == tmp_lock->record_id)
				{
					break;
				}
			}
			buffer_unpinned(cache);



			char buf[50], buf1[50];
			sprintf(buf, "%lld", tmp_lock->page_id);

			sprintf(buf1, "%d", i);
			strcat(buf, "#");
			strcat(buf, buf1);
			strcat(buf, "#\0");

			int lock_table_index = hash_func(buf, MAX_LOCK_NUM);

			if (tmp_lock->next != NULL)
			{
				lock_manager.lock_table[lock_table_index]->head = tmp_lock->next;
				tmp_lock->next->prev = tmp_lock->prev;

			}
			else
			{
				lock_manager.lock_table[lock_table_index]->head = NULL;
				lock_manager.lock_table[lock_table_index]->tail = NULL;


			}
		}



		free(tmp_lock);

		tmp_lock = next_lock;
	}



	pthread_mutex_unlock(&trx_manager.trx_sys_mutex);


	return trx_id;
}

int abort_trx(int trx_id)
{

	while (true)
	{
		buffer* cache = NULL;


		undo_log_t* tmp_log = trx_manager.trx_table[trx_id].undo_log_list;

		pthread_mutex_lock(&buffer_pools.buf_pool_sys_mutex);

		pagenum_t leaf = find_leafpage(tmp_log->table_id, tmp_log->key);

		cache = buffer_read_page(tmp_log->table_id, leaf, cache);

		//lock을 잡는데 실패하면
		if (pthread_mutex_trylock(&cache->buf_sys_mutex) != 0)
		{
			buffer_unpinned(cache);
			pthread_mutex_unlock(&buffer_pools.buf_pool_sys_mutex);
			return abort_trx(trx_id);
		}

		pthread_mutex_unlock(&buffer_pools.buf_pool_sys_mutex);

		undo_log_t* undo = trx_manager.trx_table[trx_id].undo_log_list;

		//value값을 한단계뒤로 바꾸기
		for (int i = 0; i < cache->frame.base.leaf_page.number_of_keys; ++i)
		{
			//만약 찾으면 value값을 바꿔주기
			if (cache->frame.base.leaf_page.records[i].key == undo->key)
			{
				strcpy(cache->frame.base.leaf_page.records[i].value, undo->old_value);
				break;
			}
		}

		//마지막이면 END TRANSACTION
		if (undo->next_undo == NULL)
		{
			trx_manager.trx_table[trx_id].undo_log_list = NULL;
			free(undo);

			buffer_set_dirty(cache);
			buffer_unpinned(cache);
			pthread_mutex_unlock(&cache->buf_sys_mutex);
			return 0;
		}
		else
		{
			trx_manager.trx_table[trx_id].undo_log_list = undo->next_undo;
			free(undo);
			buffer_set_dirty(cache);
			buffer_unpinned(cache);
			pthread_mutex_unlock(&cache->buf_sys_mutex);
		}
	}

}

int acquire_record_lock(int table_id, pagenum_t page_id, int64_t key, int trx_id, int lock_mode, int slot)
{

	pthread_mutex_lock(&lock_manager.lock_sys_mutex);
	//trx를 돌면서 해당 lock이 잡혀있는지 확인
	lock_t* tmp_lock = trx_manager.trx_table[trx_id].trx_locks;
	while (tmp_lock != NULL)
	{
		if (tmp_lock->table_id == table_id && tmp_lock->page_id == page_id && tmp_lock->record_id == key)
		{

			if (tmp_lock->acquired == true)
			{
				//새로운 락이 SHARED일 경우
				if (lock_mode == SHARED)
				{
					//앞서서 락을 얻은 노드가 SHARED || EXCLUSIVE일 경우
					if (tmp_lock->mode == SHARED || tmp_lock->mode == EXCLUSIVE)
					{
						pthread_mutex_unlock(&lock_manager.lock_sys_mutex);

						return SUCCESS;
					}

				}
				else if (lock_mode == EXCLUSIVE) //새로운 락이 EXCLUSIVE일 경우
				{
					//앞서서 락을 얻은 노드가 EXCLUSIVE일 경우
					if (tmp_lock->mode == EXCLUSIVE)
					{
						pthread_mutex_unlock(&lock_manager.lock_sys_mutex);

						return SUCCESS;
					}
				}
			}
			else
			{
				tmp_lock->acquired = true;
				pthread_mutex_unlock(&lock_manager.lock_sys_mutex);

				return SUCCESS;
			}
		}

		tmp_lock = tmp_lock->down;
	}



	char buf[50], buf1[50];
	sprintf(buf, "%lld", page_id);

	sprintf(buf1, "%d", slot);;

	strcat(buf, "#");
	strcat(buf, buf1);
	strcat(buf, "#\0");

	int lock_table_index = hash_func(buf, MAX_LOCK_NUM);

	
	fflush(stdout);


	//해당하는 lock_table에 접근하기
	locks* tmp = lock_manager.lock_table[lock_table_index];
	for (; tmp != NULL; tmp = tmp->next)
	{

		//해당하는 lock_table을 찾았다면
		if (tmp->table_id == table_id && tmp->page_id == page_id && tmp->slot == slot)
		{
			//아무것도 없을 때
			if (tmp->head == NULL)
			{

				tmp->head = (lock_t*)malloc(sizeof(lock_t));
				tmp->tail = tmp->head;

				tmp->head->mode = lock_mode;
				tmp->head->acquired = true;
				tmp->head->prev = NULL;
				tmp->head->next = NULL;
				tmp->head->down = NULL;
				tmp->head->page_id = page_id;
				tmp->head->record_id = key;
				tmp->head->table_id = table_id;
				tmp->head->trx = &trx_manager.trx_table[trx_id];

				//트랜잭션이 노드를 가르키도록 수정
				if (trx_manager.trx_table[trx_id].trx_locks == NULL)
				{
					trx_manager.trx_table[trx_id].trx_locks = tmp->head;
				}
				else
				{
					lock_t* trx_next = trx_manager.trx_table[trx_id].trx_locks;
					while (trx_next->down != NULL)
					{
						trx_next = trx_next->down;
					}

					trx_next->down = tmp->head;
				}


				pthread_mutex_unlock(&lock_manager.lock_sys_mutex);

				return SUCCESS;
			}
			else //하나라도 있을 때
			{

				//마지막 락에 해당하는 노드, 동적할당
				lock_t* last_lock = tmp->tail;

				last_lock->next = (lock_t*)malloc(sizeof(lock_t));

				last_lock = last_lock->next;

				last_lock->down = NULL;
				last_lock->mode = lock_mode;
				last_lock->acquired = false;
				last_lock->next = NULL;
				last_lock->page_id = page_id;
				last_lock->prev = tmp->tail;
				last_lock->record_id = key;
				last_lock->table_id = table_id;
				last_lock->trx = &trx_manager.trx_table[trx_id];

				tmp->tail = last_lock;


				//트랜잭션이 노드를 가르키도록 수정
				if (trx_manager.trx_table[trx_id].trx_locks == NULL)
				{
					trx_manager.trx_table[trx_id].trx_locks = last_lock;
				}
				else
				{
					lock_t* trx_next = trx_manager.trx_table[trx_id].trx_locks;
					while (trx_next->down != NULL)
					{
						trx_next = trx_next->down;
					}

					trx_next->down = last_lock;
				}

				//리스트를 탐색하면서
				//conflict, deadlock, success를 판별

				//S-mode일 경우

				if (last_lock->mode == SHARED)
				{
					//conflict가 나오는 lock 체크
					lock_t* check_lock = last_lock->prev;
					//cycle을 체크하기 위한 변수
					int last_trx_id = last_lock->trx->trx_id;

					while (check_lock != NULL)
					{
						//ex) X() <- S()
						//앞에 X-mode가 올때만 계산
						if (check_lock->mode == EXCLUSIVE)
						{
							lock_t* cycle_lock = check_lock;
							while (true)
							{
								//트랜잭션이 기다리고 있는 lock으로 이동
								cycle_lock = cycle_lock->trx->wait_lock;

								//사이클을 생성하지 않는다면
								if (cycle_lock == NULL)
								{
									//check_lock의 cond를 잡는다
									last_lock->trx->wait_lock = check_lock;



									int wait_trx_id = trx_manager.trx_table[trx_id].wait_lock->trx->trx_id;

									buffer* cache = NULL;
									cache = buffer_read_page(table_id, page_id, cache);

									pthread_mutex_unlock(&cache->buf_sys_mutex);

									buffer_unpinned(cache);

									pthread_mutex_unlock(&lock_manager.lock_sys_mutex);


									pthread_cond_wait(&trx_manager.trx_table[wait_trx_id].trx_cond, &trx_manager.trx_table[wait_trx_id].trx_mutex);
									pthread_mutex_unlock(&trx_manager.trx_table[wait_trx_id].trx_mutex);





									return CONFLICT;
								}

								//기다리고 있는 lock이 cycle을 생성한다면
								//deadlock!!
								if (cycle_lock->trx->trx_id == last_trx_id)
								{

									pthread_mutex_unlock(&lock_manager.lock_sys_mutex);

									return DEADLOCK;
								}
							}
						}

						check_lock = last_lock->prev;
					}

					if (check_lock == NULL)
					{
						last_lock->acquired = true;
						pthread_mutex_unlock(&lock_manager.lock_sys_mutex);

						return SUCCESS;
					}
				}
				else if (last_lock->mode == EXCLUSIVE) //X-mode일 경우
				{
					//conflict가 나오는 lock 체크
					lock_t* check_lock = last_lock->prev;
					//cycle을 체크하기 위한 변수
					int last_trx_id = last_lock->trx->trx_id;

					while (check_lock != NULL)
					{
						//ex) X() <- S()
						//앞에 X-mode가 올때만 계산
						if (check_lock->mode == EXCLUSIVE)
						{
							lock_t* cycle_lock = check_lock;
							while (true)
							{

								//트랜잭션이 기다리고 있는 lock으로 이동
								cycle_lock = cycle_lock->trx->wait_lock;

								//사이클을 생성하지 않는다면
								if (cycle_lock == NULL)
								{
									//기다리는 lock을 설정하고 conflict 반환
									last_lock->trx->wait_lock = check_lock;

									buffer* cache = NULL;
									cache = buffer_read_page(table_id, page_id, cache);

									pthread_mutex_unlock(&cache->buf_sys_mutex);

									buffer_unpinned(cache);


									int wait_trx_id = trx_manager.trx_table[trx_id].wait_lock->trx->trx_id;


									pthread_mutex_unlock(&lock_manager.lock_sys_mutex);

									pthread_cond_wait(&trx_manager.trx_table[wait_trx_id].trx_cond, &trx_manager.trx_table[wait_trx_id].trx_mutex);



									return CONFLICT;
								}

								//기다리고 있는 lock이 cycle을 생성한다면
								//deadlock!!
								if (cycle_lock->trx->trx_id == last_trx_id)
								{

									pthread_mutex_unlock(&lock_manager.lock_sys_mutex);

									return DEADLOCK;
								}
							}
						}

						check_lock = check_lock->prev;
					}
				}

			}
		}
	}

	//만약 해당하는 page id, record id가 할당되어 있지 않다면
	//할당
	if (lock_manager.lock_table[lock_table_index] == NULL)
	{
		lock_manager.lock_table[lock_table_index] = (locks*)malloc(sizeof(struct locks));
		lock_manager.lock_table[lock_table_index]->table_id = table_id;
		lock_manager.lock_table[lock_table_index]->page_id = page_id;
		lock_manager.lock_table[lock_table_index]->slot = slot;
		lock_manager.lock_table[lock_table_index]->next = NULL;

		//insert new lock node

		lock_manager.lock_table[lock_table_index]->head = (lock_t*)malloc(sizeof(lock_t));
		lock_manager.lock_table[lock_table_index]->tail = lock_manager.lock_table[lock_table_index]->head;

		lock_manager.lock_table[lock_table_index]->head->mode = lock_mode;
		lock_manager.lock_table[lock_table_index]->head->acquired = true;
		lock_manager.lock_table[lock_table_index]->head->prev = NULL;
		lock_manager.lock_table[lock_table_index]->head->next = NULL;
		lock_manager.lock_table[lock_table_index]->head->down = NULL;

		lock_manager.lock_table[lock_table_index]->head->page_id = page_id;
		lock_manager.lock_table[lock_table_index]->head->record_id = key;
		lock_manager.lock_table[lock_table_index]->head->table_id = table_id;
		lock_manager.lock_table[lock_table_index]->head->trx = &trx_manager.trx_table[trx_id];

		//트랜잭션이 노드를 가르키도록 수정
		if (trx_manager.trx_table[trx_id].trx_locks == NULL)
		{
			trx_manager.trx_table[trx_id].trx_locks = lock_manager.lock_table[lock_table_index]->head;
		}
		else
		{
			lock_t* trx_next = trx_manager.trx_table[trx_id].trx_locks;
			while (trx_next->down != NULL)
			{
				trx_next = trx_next->down;
			}

			trx_next->down = lock_manager.lock_table[lock_table_index]->head;
		}
	}

	//성공!
	pthread_mutex_unlock(&lock_manager.lock_sys_mutex);



	return SUCCESS;
}
