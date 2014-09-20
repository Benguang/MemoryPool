#ifndef __MEMORY_POOL_HPP
#define __MEMORY_POOL_HPP

/**
 * Copyright (c) 2014, Benguang Zhao
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>

/**
 * The list header. 
 */
struct List {
	int header_node_index;
};

/**
 * The list node to be embedded into the factory.
 */
struct ListNode {
	bool used;
	int prev_node_index;
	int next_node_index;
};


template <typename T>
class Factory {
public:
	/**
	 * Construct a factory with a given capacity.
	 */
	Factory(long capacity);

	/**
	 * Destruct the given factory.
	 */
	~Factory() {
		free(list_meta_data);
		free(mm_pool);
	}

	/**
	 * Produce an object. The methods produce() and recylce() are thread-safe.
	 */
	inline T *produce();
	
	/**
	 * Recycle the given object. 
	 */
	inline void recycle(T* &ptr);

private:
	/**
	 * Get a list node out of the given list. 
	 */
	inline ListNode *get_from_list(List &list);
	
	/**
	 * Put a list node into the given list.
	 */
	inline void put_into_list(List &list, ListNode *node);
	
	/**
	 * Remove a list node from the given list.
	 */
	inline void remove_from_list(List &list, ListNode *node);

	/**
	 * There is a one-one correspondence between the list nodes and the objects or between the list nodes and the indices.
	 */
	
	/**
	 * Get the index of a given list node.
	 */
	long list_to_index(ListNode const*ptr) {
		return (ptr == NULL ? -1 : ptr - list_meta_data);
	}

	/**
	 * Get the index of an object, given its pointer
	 */	
	long mm_to_index(T const* ptr) {
		return (ptr == NULL ? -1 : ptr - mm_pool);
	}

	/**
	 * Get the pointer of a node, given its index
	 */
	ListNode *index_to_list_node(long index) {
		return (index == -1 ? NULL : list_meta_data + index);
	}
	
	/**
	 * Get the object of an given index.
	 */
	T *index_to_mm_pointer(long index) {
		return (index == -1 ? NULL : mm_pool + index);
	}
	
	/**
	 * Check whether an object is managed by the factory
	 */
	bool check_mm_pointer_range(T const*ptr) {
		return (ptr == NULL ? true : (mm_pool <= ptr && ptr < mm_pool + capacity));
	}
	
	/**
	 * Get the object of a given list node.
	 */
	T *list_to_mm_pointer(ListNode const*node) {
		return index_to_mm_pointer(list_to_index(node));
	}
	
	/**
	 * Get the list node of a given object. 
	 */
	ListNode *mm_to_list_pointer(T const*ptr) {
		return index_to_list_node(mm_to_index(ptr));
	}

	/**
	 * Get the next node of a given list node.
	 */	
	ListNode *list_next_node(ListNode const*node) {
		return (node->next_node_index == -1 ? NULL : index_to_list_node(node->next_node_index));
	}
	
	/**
	 * Get the previous node of a given list node.
	 */
	ListNode *list_prev_node(ListNode const*node) {
		return (node->prev_node_index == -1 ? NULL : index_to_list_node(node->prev_node_index));
	}

	/**
	 * Set a list node to be the header of a given list.
	 */
	void set_list_header(List &list, ListNode const* node) {
		list.header_node_index  = (node == NULL ? -1 : list_to_index(node));
	}
	
	/**
	 * Get the header list node of a given list.
	 */
	ListNode *get_list_header(List const&list) {
		return index_to_list_node(list.header_node_index);
	}
	
private:
	/**
	 * The mutex lock to synchronize the produce() and recycle() methods. 
	 */
	pthread_mutex_t lock;

	/**
	 * The capacity of the factory.
	 */
	long capacity;

	/**
	 * @mm_free is the list of free memory blocks. Only the header index is stored here.
	 */
	List mm_free;

	/**
	 * @mm_in_use is the list of memory blocks in use. Only the header index is stored here.
	 */
	List mm_in_use;

	/**
	 * @list_meta_data is the large array of memory blocks pre-allocated to store the information of nodes of 
	 * the two lists @mm_free and @mm_in_use.
	 */
	ListNode *list_meta_data;

	/**
	 * @mm_pool is the large array of memory blocks pre-allocated to store the objects managed by the factory.
	 */
	T *mm_pool;
};

/**
 * The operator new overloaded to be used by the postional constructor.
 * @size - the size of the memory block
 * @ptr - the pointer to the object to be constructed
 * @return - the pointer to the memory block
 */
template <typename T>
void *operator new(size_t size, T*& ptr) throw()
{
	return ptr;
}

/**
 * Construct a factory with a given capacity.
 * @capacity - the capacity given.
 */
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
		ListNode *node = index_to_list_node(i);
		assert(node != NULL);
		node->used = false;
		node->prev_node_index = i - 1;
		node->next_node_index = i + 1;
	}
	index_to_list_node(0)->prev_node_index = -1;
	index_to_list_node(capacity-1)->next_node_index = -1;
	set_list_header(mm_free, index_to_list_node(0));
	set_list_header(mm_in_use, NULL);
}

/**
 * Produce an object from the factory.
 * @return - the object produced.
 */
template <typename T>
T *Factory<T>::produce() {
	T *ptr = NULL;
	pthread_mutex_lock(&lock);
	ListNode *node = get_from_list(mm_free);
	if (node != NULL) {
		put_into_list(mm_in_use, node);
		node->used = true;
		ptr = list_to_mm_pointer(node);
		assert(ptr != NULL);
		new(ptr) T;
	}
	pthread_mutex_unlock(&lock);
	return ptr;
}

/**
 * Recycle an object produced by the factory.
 * @ptr - the pointer to the object
 * @return - void
 */
template <typename T>	
void Factory<T>::recycle(T* &ptr) {
	pthread_mutex_lock(&lock);
	if(check_mm_pointer_range(ptr) == false) {
		printf("warning: The pointer 0x%p is out of range.\n", ptr);
	} 
	else if (ptr != NULL) {
		ListNode *node = mm_to_list_pointer(ptr);
		assert(node != NULL);
		if (node->used) {
			ptr->~T();
			remove_from_list(mm_in_use, node);
			put_into_list(mm_free, node);
			node->used = false;
		} else {
			printf("warning: The pointer 0x%p is recycled twice.\n", ptr);
		}
		ptr = NULL;
	}
	pthread_mutex_unlock(&lock);
}

/**
 * Get the header from the list
 * @list - the list to get the header from.
 * @return - the header got.
 */
template <typename T>
ListNode *Factory<T>::get_from_list(List &list)
{
	ListNode *header = get_list_header(list);
	if (header != NULL) {
		ListNode *next_node = list_next_node(header);
		if (next_node != NULL) {
			next_node->prev_node_index = -1;
		}
		set_list_header(list, next_node);
		header->prev_node_index = header->next_node_index = -1;
	}
	return header;
}

/**
 * Put the node to the end of the list.
 * @list - the list to put the node into
 * @node - the node to put
 * @return - void
 */
template <typename T>
void Factory<T>::put_into_list(List &list, ListNode *node) {
	assert(node != NULL);
	ListNode *header = get_list_header(list);
	node->prev_node_index = -1;
	node->next_node_index = list_to_index(header);
	if (header != NULL) {
		header->prev_node_index = list_to_index(node);
	}
	set_list_header(list, node);
}

/**
 * Remove the node from the list
 * @list - the list to remove the node from
 * @node - the node to remove
 * @return - void
 */
template <typename T>
void Factory<T>::remove_from_list(List &list, ListNode *node)
{
	ListNode *header = get_list_header(list);
	if (header == NULL) {
		return;
	}
	assert(node != NULL);
	
	if (node->prev_node_index == -1) {
		assert(header == node);
		ListNode *next_node = list_next_node(node);
		if (next_node != NULL) {
			next_node->prev_node_index = -1;
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
	node->prev_node_index = node->next_node_index = -1;
}

#endif /*__MEMORY_POOL_HPP*/

