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
template <class T> FORCE_INLINE void min_heapify(T *heap, size_t index, size_t size) {
	bool   heapify;
	size_t min;
	do {
		heapify  = false;
		size_t l = ICE_HEAP_LEFT(index);
		size_t r = ICE_HEAP_RIGHT(index);

		min = index;
		if (l < size && heap[l] < heap[index])
			min = l;
		if (r < size && heap[r] < heap[min])
			min = r;

		if (min != index) {
			ice::math::swap(heap[index], heap[min]);
			index   = min;
			heapify = true;
		}
	} while (heapify);
}

/*! Converts an array into a max-heap
 *
 *  @param[in]      array       The array
 *  @param          size       The size of the array
 **/
template <class T> FORCE_INLINE void min_heap_build(T *array, size_t size) {
	for (size_t i = size / 2; i > 0; --i)
		min_heapify(array, i - 1, size);
}

/*! Sorts the heap
 *
 *  @param[in]      heap   The heap
 *  @param          size   Size of the heap
 **/
template <class T> FORCE_INLINE void min_heap_sort(T *heap, size_t size) {
	for (size_t j = 1; j < size; ++j) {
		T      key = heap[j];
		size_t i   = j;
		for (; i > 0 && heap[i - 1] > key; --i)
			heap[i] = heap[i - 1];
		heap[i] = key;
	}
}

/*! Finds the element in the heap (if it exists)
 *
 *  @param          heap       The heap
 *  @param          size       The size of the heap
 *  @param          key        The item you want to find
 *  @returns        int         -1 if not found, index otherwise
 **/
template <class T> FORCE_INLINE int min_heap_find(T *heap, size_t size, const T &key) {
	if (size == 0)
		return -1;

	if (size == 1)
		return *heap == key ? 0 : -1;

	for (size_t i = 0; i < size; ++i) {
		if (heap[i] == key)
			return i;
	}

	/// size_t i = _size - 1;
	/// while (i > 0)
	//{
	//	if (_heap[i] == _key)
	//	{
	//		return i;
	//	}
	//	else if (_heap[icHeapParent(i)] < _key)
	//	{
	//		break;
	//	}

	//	i = icHeapParent(i);
	//}

	return -1;
}

/*! Removes the minimum element from the heap
 *
 *  Running time - O(logn)
 *
 *  @param[in]      heap   The heap
 *  @param          size   Size of the heap
 *  @returns        T       Min element
 **/
template <class T> FORCE_INLINE T min_heap_dequeue(T *heap, size_t size) {
	assert(size > 0);

	T min   = heap[0];
	heap[0] = heap[size - 1];

#ifdef _DEBUG
	memset(&_heap[_size - 1], 0xcdcdcdcd, sizeof(T));
#endif
	min_heapify(heap, 0, size - 1);
	return min;
}

/*! Decreases an element in the heap to the new key
 *
 *  @param[in]      heap   The heap
 *  @param          index  Index to element to decrease
 *  @param[in]      key    The new key
 **/
template <class T> FORCE_INLINE void min_heap_decrease_key(T *heap, size_t index, const T &key) {
	if (key > heap[index])
		return;
	heap[index] = key;
	while (index > 0 && heap[ICE_HEAP_PARENT(index)] > heap[index]) {
		ice::math::swap(heap[index], heap[ICE_HEAP_PARENT(index)]);
		index = ICE_HEAP_PARENT(index);
	}
}

/// Insert key at the end of a min heap containing size elements, then restore heap order.
/// Caller storage must hold at least size + 1 elements.
template <class T> FORCE_INLINE void min_heap_insert(T *heap, size_t size, const T &key) {
	size_t i   = size;
	heap[size] = key;
	while (i > 0 && heap[ICE_HEAP_PARENT(i)] > heap[i]) {
		ice::math::swap(heap[i], heap[ICE_HEAP_PARENT(i)]);
		i = ICE_HEAP_PARENT(i);
	}
}

} // namespace ice::memory
