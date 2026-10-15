#pragma once

#include "ice/math/common_math.h"
namespace ice::memory {

#define ICE_HEAP_PARENT(x) ((x - 1) / 2) //! Parent index of current index
#define ICE_HEAP_LEFT(x) ((2 * x) + 1)   //! Left index of current index
#define ICE_HEAP_RIGHT(x) (2 * (x + 1))  //! Right index of current index

/*! Satisfies the heap condition
 *
 *  @param[in]      heap       The heap
 *  @param          index      Index of the node to heapify
 *  @param          size       Size of the heap
 **/
template <class T> inline void max_heapify(T *heap, size_t index, size_t size) {
	bool   heapify;
	size_t largest;
	do {
		heapify  = false;
		size_t l = ICE_HEAP_LEFT(index);
		size_t r = ICE_HEAP_RIGHT(index);

		if (l <= size && heap[l] > heap[index])
			largest = l;
		else
			largest = index;

		if (r <= size && heap[r] > heap[largest])
			largest = r;

		if (largest != index) {
			ice::math::swap(heap[index], heap[largest]);
			index   = largest;
			heapify = true;
		}
	} while (heapify);
}

/*! Converts an array into a max-heap
 *
 *  @param[in]      array       The array
 *  @param          size       The size of the array
 **/
template <class T> inline void build_max_heap(T *array, size_t size) {
	for (size_t i = (size - 1) / 2; i >= 0; --i)
		max_heapify(array, i, size);
}

/*! Sorts the heap
 *
 *  @param[in]      heap   The heap
 *  @param          size   Size of the heap
 **/
template <class T> inline void max_heap_sort(T *heap, size_t size) {
	for (size_t i = size - 1; i > 0; --i) {
		ice::math::swap(heap[0], heap[i]);
		max_heapify(heap, 0, --size);
	}
}

/*! Removes the maximum element from the heap
 *
 *  Running time - O(logn)
 *
 *  @param[in]      heap   The heap
 *  @param          size   Size of the heap
 *  @returns        T       Max element
 **/
template <class T> inline T heap_extract_max(T *heap, size_t size) {
	T max   = heap[0];
	heap[0] = heap[size - 1];
	max_heapify(heap, 0, size - 2);
	return max;
}

/*! Increases an element in the heap to the new key
 *
 *  @param[in]      heap   The heap
 *  @param          index  Index to element to increase
 *  @param[in]      key    The new key
 **/
template <class T> inline void heap_increase_key(T *heap, size_t index, T &key) {
	assert(key > heap[index]);
	heap[index] = key;
	while (index > 0 && heap[ICE_HEAP_PARENT(index)] < heap[index]) {
		ice::math::swap(heap[index], heap[ICE_HEAP_PARENT(index)]);
		index = ICE_HEAP_PARENT(index);
	}
}

/// Insert key at the end of a max heap containing size elements, then restore heap order.
/// Caller storage must hold at least size + 1 elements.
template <class T> inline void max_heap_insert(T *heap, size_t size, T &key) {
	heap[size] = key;
	size_t i   = size;
	while (i > 0 && heap[ICE_HEAP_PARENT(i)] < heap[i]) {
		ice::math::swap(heap[i], heap[ICE_HEAP_PARENT(i)]);
		i = ICE_HEAP_PARENT(i);
	}
}

} // namespace ice::memory
