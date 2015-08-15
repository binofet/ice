#ifndef __IC_MIN_HEAP_H__
#define __IC_MIN_HEAP_H__

#include "Math/icCommonMath.h"


#define icHeapParent(x)     ((x-1)/2)     //! Parent index of current index
#define icHeapLeft(x)       ((2*x)+1)     //! Left index of current index
#define icHeapRight(x)      (2*(x+1))     //! Right index of current index


/*! Satisfies the heap condition
*
*  @param[in]      _heap       The heap
*  @param          _index      Index of the node to heapify
*  @param          _size       Size of the heap
**/
template<class T>
FORCE_INLINE void icMinHeapify(T* _heap, size_t _index, size_t _size)
{
	bool heapify;
	size_t min;
	do
	{
		heapify = false;
		size_t l = icHeapLeft(_index);
		size_t r = icHeapRight(_index);

		min = _index;
		if (l < _size && _heap[l] < _heap[_index])
			min = l;
		if (r < _size && _heap[r] < _heap[min])
			min = r;

		if (min != _index)
		{
			icSwap(_heap[_index], _heap[min]);
			_index = min;
			heapify = true;
		}
	}while(heapify);
}// END FUNCTION icMinHeapify(T* _heap, size_t _index, size_t _size)


/*! Converts an array into a max-heap
*
*  @param[in]      _array       The array
*  @param          _size       The size of the array
**/
template<class T>
FORCE_INLINE void icMinHeapBuild(T* _array, size_t _size)
{
	int i;
	for (i=(_size-1)/2; i>=0; --i)
		icMinHeapify(_array,i,_size);
}// END FUNCTION icBuildMaxHeap(T* _heap, size_t _size)


/*! Sorts the heap
*
*  @param[in]      _heap   The heap
*  @param          _size   Size of the heap
**/
template<class T>
FORCE_INLINE void icMinHeapSort(T* _heap, size_t _size)
{
	int i,j = 0;
	for (; j<_size; ++j)
	{
		T key = _heap[j];
		for (i=j-1; i>=0 && _heap[i]>key; --i)
			_heap[i+1]=_heap[i];
		_heap[i+1] = key;
	}
}// END FUNCTION icHeapSort(T* _heap, size_t _size)


/*! Finds the element in the heap (if it exists)
*
*  @param          _heap       The heap
*  @param          _size       The size of the heap
*  @param          _key        The item you want to find
*  @returns        int         -1 if not found, index otherwise
**/
template<class T>
FORCE_INLINE int icMinHeapFind(T* _heap, size_t _size, const T& _key)
{
	if (_size == 0)
		return -1;

	if (_size == 1)
		return *_heap == _key ? 0 : -1;

	for (size_t i = 0; i < _size; ++i)
	{
		if (_heap[i] == _key)
			return i;
	}

	//size_t i = _size - 1;
	//while (i > 0)
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
}// END FUNCTION icMinHeapFind(T* _heap, size_t _size, const T& _key)


/*! Removes the minimum element from the heap
*
*  Running time - O(logn)
*
*  @param[in]      _heap   The heap
*  @param          _size   Size of the heap
*  @returns        T       Min element
**/
template<class T>
FORCE_INLINE T icMinHeapDequeue(T* _heap, size_t _size)
{
	assert(_size>0);

	T min = _heap[0];
	_heap[0] = _heap[_size-1];

#ifdef _DEBUG
	memset(&_heap[_size-1],0xcdcdcdcd,sizeof(T));
#endif
	icMinHeapify(_heap,0,_size-1);
	return min;
}// END FUNCTION icHeapExtractMin(T* _heap, size_t _size)


/*! Decreases an element in the heap to the new key
*
*  @param[in]      _heap   The heap
*  @param          _index  Index to element to decrease
*  @param[in]      _key    The new key
**/
template<class T>
FORCE_INLINE void icMinHeapDecreaseKey(T* _heap, size_t _index,const T& _key)
{
	if (_key > _heap[_index])
		return;
	_heap[_index] = _key;
	while (_index > 0 && _heap[icHeapParent(_index)] > _heap[_index])
	{
		icSwap(_heap[_index], _heap[icHeapParent(_index)]);
		_index = icHeapParent(_index);
	}
}// END FUNCTION icHeapDecreaseKey(T* _heap, size_t _index, T& _key)


/*! Inserts a new element into the min heap
*
*  @param[in]      _heap   The heap
*  @param[in]      _key    The new element
**/
template<class T>
FORCE_INLINE void icMinHeapInsert(T* _heap, size_t _size,const T& _key)
{
	size_t i = _size;
	_heap[_size] = _key;
	while (i > 0 && _heap[icHeapParent(i)] > _heap[i])
	{
		icSwap(_heap[i], _heap[icHeapParent(i)]);
		i = icHeapParent(i);
	}
}// END FUNCTION icMinHeapInsert(T* _heap, T& _key)

#endif //__IC_MIN_HEAP_H__