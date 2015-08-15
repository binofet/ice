#ifndef __IC_MAX_HEAP_H__
#define __IC_MAX_HEAP_H__

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
inline void icMaxHeapify(T* _heap, size_t _index, size_t _size)
{
    bool heapify;
    size_t largest;
    do
    {
        heapify = false;
        size_t l = icHeapLeft(_index);
        size_t r = icHeapRight(_index);

        if (l <= _size && _heap[l] > _heap[_index])
            largest = l;
        else
            largest = _index;

        if (r <= _size && _heap[r] > _heap[largest])
            largest = r;

        if (largest != _index)
        {
            icSwap(_heap[_index], _heap[largest]);
            _index = largest;
            heapify = true;
        }
    }while(heapify);
}// END FUNCTION icMaxHeapify(T* _heap, size_t _index, size_t _size)


/*! Converts an array into a max-heap
 *
 *  @param[in]      _array       The array
 *  @param          _size       The size of the array
**/
template<class T>
inline void icBuildMaxHeap(T* _array, size_t _size)
{
    for (size_t i=(_size-1)/2; i>=0; --i)
        icMaxHeapify(_array,i,_size);
}// END FUNCTION icBuildMaxHeap(T* _heap, size_t _size)


/*! Sorts the heap
 *
 *  @param[in]      _heap   The heap
 *  @param          _size   Size of the heap
**/
template<class T>
inline void icMaxHeapSort(T* _heap, size_t _size)
{
    for (size_t i=_size-1; i > 0; --i)
    {
        icSwap(_heap[0],_heap[i]);
        icMaxHeapify(_heap,0,--_size);
    }
}// END FUNCTION icHeapSort(T* _heap, size_t _size)


/*! Removes the maximum element from the heap
 *
 *  Running time - O(logn)
 *
 *  @param[in]      _heap   The heap
 *  @param          _size   Size of the heap
 *  @returns        T       Max element
**/
template<class T>
inline T icHeapExtractMax(T* _heap, size_t _size)
{
    T max = _heap[0];
    _heap[0] = _heap[_size-1];
    icMaxHeapify(_heap,0,_size-2);
    return max;
}// END FUNCTION icHeapExtractMax(T* _heap, size_t _size)


/*! Increases an element in the heap to the new key
 *
 *  @param[in]      _heap   The heap
 *  @param          _index  Index to element to increase
 *  @param[in]      _key    The new key
**/
template<class T>
inline void icHeapIncreaseKey(T* _heap, size_t _index, T& _key)
{
    assert(_key > _heap[_index]);
    _heap[_index] = _key;
    while (_index > 0 && _heap[icHeapParent(_index)] < _heap[_index])
    {
        icSwap(_heap[_index], _heap[icHeapParent(_index)]);
        _index = icHeapParent(_index);
    }
}// END FUNCTION icHeapIncreaseKey(T* _heap, size_t _index, T& _key)


/*! Inserts a new element into the heap
 *
 *  @param[in]      _heap   The heap
 *  @param[in]      _key    The new element
**/
template<class T>
inline void icMaxHeapInsert(T* _heap, size_t _size, T& _key)
{
    _heap[_size] = _key;
    size_t i = _size;
    while (i > 0 && _heap[icHeapParent(i)] < _heap[i])
    {
        icSwap(_heap[i], _heap[icHeapParent(i)]);
        i = icHeapParent(i);
    }
}// END FUNCTION icMaxHeapInsert(T* _heap, T& _key)

#endif //__IC_HEAP_H__