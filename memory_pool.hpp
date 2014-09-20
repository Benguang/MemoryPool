#ifndef __MEMORY_POOL_HPP
#define __MEMORY_POOL_HPP

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>

struct List {
	int header_node_index;
};

struct ListNode {
	bool used;
	int prev_node_index;
	int next_node_index;
};

#define NIL -1

template <typename T>
class Factory {
public:
	Factory(long capacity);

	~Factory() {
		free(list_meta_data);
		free(mm_pool);
	}

	inline T *produce();
	
	inline void recycle(T* &ptr);

private:
	inline void recycle_alone(T const*ptr);
	
	inline ListNode *get_from_list(List &list);
	
	inline void put_to_list(List &list, ListNode *node);
	
	inline void remove_from_list(List &list, ListNode *node);

	long list_to_index(ListNode const*ptr) {
		return (ptr == NULL ? NIL : ptr - list_meta_data);
	}
	
	long mm_to_index(T const* ptr) {
		return (ptr == NULL ? NIL : ptr - mm_pool);
	}

	ListNode *list_to_pointer(long index) {
		return (index == NIL ? NULL : list_meta_data + index);
	}
	
	T *mm_to_pointer(long index) {
		return (index == NIL ? NULL : mm_pool + index);
	}
	
	bool check_mm_pointer_range(T const*ptr) {
		return (ptr == NULL ? true : (mm_pool <= ptr && ptr < mm_pool + capacity));
	}
	
	T *list_to_mm_pointer(ListNode const*node) {
		return mm_to_pointer(list_to_index(node));
	}
	
	ListNode *mm_to_list_pointer(T const*ptr) {
		return list_to_pointer(mm_to_index(ptr));
	}
	
	ListNode *list_next_node(ListNode const*node) {
		return (node->next_node_index == NIL ? NULL : list_to_pointer(node->next_node_index));
	}
	
	ListNode *list_prev_node(ListNode const*node) {
		return (node->prev_node_index == NIL ? NULL : list_to_pointer(node->prev_node_index));
	}

	void set_list_header(List &list, ListNode const* node) {
		list.header_node_index  = (node == NULL ? NIL : list_to_index(node));
	}
	
	ListNode *get_list_header(List const&list) {
		return list_to_pointer(list.header_node_index);
	}
	
private:
	pthread_mutex_t lock;
	long capacity;
	List mm_free;
	List mm_in_use;
	ListNode *list_meta_data;
	T *mm_pool;
};

template <typename T>
void *operator new(size_t size, T*& ptr) throw()
{
	return ptr;
}

template <typename T>
Factory<T>::Factory(long capacity) 
	: lock(PTHREAD_MUTEX_INITIALIZER), capacity(capacity)
{
	assert(capacity > 0);
	
	mm_pool = (T*)malloc(sizeof(T)*capacity);
	assert(mm_pool != NULL);
	
	list_meta_data = (ListNode *)malloc(sizeof(ListNode)*capacity);
	assert(list_meta_data != NULL);
	
	// Initialize the list list_meta_data for mm_free and mm_in_use
	for (long i = 0; i < capacity; i++) {
		ListNode *node = list_to_pointer(i);
		assert(node != NULL);
		node->used = false;
		node->prev_node_index = i - 1;
		node->next_node_index = i + 1;
	}
	list_to_pointer(0)->prev_node_index = NIL;
	list_to_pointer(capacity-1)->next_node_index = NIL;
	set_list_header(mm_free, list_to_pointer(0));
	set_list_header(mm_in_use, NULL);
}

template <typename T>
T *Factory<T>::produce() {
	T *ptr = NULL;
	pthread_mutex_lock(&lock);
	ListNode *node = get_from_list(mm_free);
	if (node != NULL) {
		put_to_list(mm_in_use, node);
		node->used = true;
		ptr = list_to_mm_pointer(node);
		assert(ptr != NULL);
		new(ptr) T;
	}
	pthread_mutex_unlock(&lock);
	return ptr;
}

template <typename T>	
void Factory<T>::recycle(T *&ptr) {
	recycle_alone(ptr);
	ptr = NULL;
}

template <typename T>	
void Factory<T>::recycle_alone(T const*ptr) {
	if(check_mm_pointer_range(ptr) == false) {
		printf("warning: The pointer 0x%p is out of range.\n", ptr);
		return;
	}
	pthread_mutex_lock(&lock);
	if (ptr != NULL) {
		ListNode *node = mm_to_list_pointer(ptr);
		assert(node != NULL);
		if (node->used) {
			ptr->~T();
			remove_from_list(mm_in_use, node);
			put_to_list(mm_free, node);
			node->used = false;
		} else {
			printf("warning: The pointer 0x%p is recycled twice.\n", ptr);
		}
	}
	pthread_mutex_unlock(&lock);
}

template <typename T>
ListNode *Factory<T>::get_from_list(List &list)
{
	ListNode *header = get_list_header(list);
	if (header != NULL) {
		ListNode *next_node = list_next_node(header);
		if (next_node != NULL) {
			next_node->prev_node_index = NIL;
		}
		set_list_header(list, next_node);
		header->prev_node_index = header->next_node_index = NIL;
	}
	return header;
}

template <typename T>
void Factory<T>::put_to_list(List &list, ListNode *node) {
	assert(node != NULL);
	ListNode *header = get_list_header(list);
	node->prev_node_index = NIL;
	node->next_node_index = list_to_index(header);
	if (header != NULL) {
		header->prev_node_index = list_to_index(node);
	}
	set_list_header(list, node);
}

template <typename T>
void Factory<T>::remove_from_list(List &list, ListNode *node)
{
	ListNode *header = get_list_header(list);
	if (header == NULL) {
		return;
	}
	assert(node != NULL);
	
	if (node->prev_node_index == NIL) {
		assert(header == node);
		ListNode *next_node = list_next_node(node);
		if (next_node != NULL) {
			next_node->prev_node_index = NIL;
		}
		set_list_header(list, next_node);
	}
	else {
		ListNode *prev_node = list_prev_node(node);
		assert(prev_node != NULL);
		ListNode *next_node = list_next_node(node);
		assert(prev_node != next_node);
		prev_node->next_node_index = list_to_index(next_node);
		if (next_node != NULL) {
			next_node->prev_node_index = list_to_index(prev_node);
		}
	}
	node->prev_node_index = node->next_node_index = NIL;
}

#endif /*__MEMORY_POOL_HPP*/

