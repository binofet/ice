/*
 *  icSort.h
 *  ICEngineOSX
 *
 *  Created by Joel Larsen on 1/22/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef __IC_SORT_H__
#define __IC_SORT_H__

#include "icCommonMath.h"

/*!    Insertion sort in ascending order
 *
 *    This is really good for small data sets or
 *    data sets which are already mostly sorted.
 *
 *    best    -    O(n)
 *    avg        -    O(n^2)
 *    worst    -    O(n^2)
 *
 *    @param[in][out]    _arrary        Array to sort
 *    @param[in]        _length        Length of the array
**/
template<class T>
inline void icInsertSortUp(T* _array, size_t _length)
{
    for (size_t j=1; j<_length; ++j)
    {
        int i;
        T key = _array[j];
        for (i=j-1; i>=0 && _array[i]>key; --i)
            _array[i+1]=_array[i];
        _array[i+1] = key;
    }
}// END FUNCTION icInsertSortUp(T* _array, size_t _lenth)


/*!    Insertion sort in descending order
 *
 *    This is really good for small data sets or
 *    data sets which are already mostly sorted.
 *
 *    best    -    n
 *    avg        -    n^2
 *    worst    -    n^2
 *
 *    @param[in][out]    _arrary        Array to sort
 *    @param[in]        _length        Length of the array
 **/
template<class T>
inline void icInsertSortDown(T* _array, size_t _length)
{
    for (size_t j=1; j<_length; ++j)
    {
        int i;
        T key = _array[j];
        for (i=j-1; i>=0 && _array[i]<key; --i)
            _array[i+1]=_array[i];
        _array[i+1] = key;
    }
}// END FUNCTION icInsertSortDown(T* _array, size_t _length)


/*! Merge sort
 *
**/
template<class T>
inline void icMergeSortUp(T* _array, size_t _length)
{
}// END FUNCTION icMergeSortUp(T* _array, size_t _length)


/*! Merge sort
 *
 **/
template<class T>
inline void icMergeSortDown(T* _array, size_t _length)
{
}// END FUNCTION icMergeSortDown(T* _array, size_t _length)


/*!    Simle bubble sort in ascending order
 *
 *    This is really not good for anything
 *
 *    best    -    O(n^2)
 *    avg        -    O(n^2)
 *    worst    -    O(n^2)
 *
 *    @param[in][out]    _arrary        Array to sort
 *    @param[in]        _length        Length of the array
 **/
template<class T>
inline void icBubbleSortUp(T* _array, size_t _length)
{
    for (size_t i=0; i<_length; ++i)
        for (size_t j=_length-1; j>i; --j)
            if (_array[j] < _array[j-1])
                icSwap(_array[j], _array[j-1]);
}// END FUNCTION icBubbleSortUp(T* _arrary, size_t _length)


/*!    Simple bubble sort in descending order
 *
 *    This is really not good for anything
 *
 *    best    -    O(n^2)
 *    avg        -    O(n^2)
 *    worst    -    O(n^2)
 *
 *    @param[in][out]    _arrary        Array to sort
 *    @param[in]        _length        Length of the array
 **/
template<class T>
inline void icBubbleSortDown(T* _array, size_t _length)
{
    for (size_t i=0; i<_length; ++i)
        for (size_t j=_length-1; j>i; --j)
            if (_array[j] > _array[j-1])
                icSwap(_array[j], _array[j-1]);
}// END FUNCTION icBubbleSortDown(T* _array, size_t _length)


/*!    Modified bubble sort in ascending order
 *
 *    This is really not good for anything
 *
 *    best    -    O(n^2)
 *    avg        -    O(n^2)
 *    worst    -    O(n^2)
 *
 *    @param[in][out]    _arrary        Array to sort
 *    @param[in]        _length        Length of the array
 **/
template<class T>
inline void icDoubleBubbleUp(T* _array, size_t _length)
{
    size_t _end = _length-1;
    bool bLtoR = true;
    for (size_t i=0; i<_end; bLtoR = !bLtoR)
    {
        if (bLtoR)
        {
            // This loop bubbles larger values from L -> R
            for (size_t j=i; j<_end; ++j)
            {
                if (_array[j+1] < _array[j])
                    icSwap(_array[j+1],_array[j]);
            }
            // decrement the end 1 because we know that the largest
            // value from i->end was just put there
            --_end;
        }
        else
        {
            // This loop bubbles smaller values from R -> L
            for (size_t j=_end; j>i; --j)
            {
                if (_array[j] < _array[j-1])
                    icSwap(_array[j], _array[j-1]);
            }
            // increment i because we know that the smallest value
            // from i->end was just put there
            ++i;
        }
    }
}// END FUNCTION icDoubleBubbleUp(T* _arrary, size_t _length)


/*!    Modified bubble sort in descending order
 *
 *    This is really not good for anything
 *
 *    best    -    O(n^2)
 *    avg        -    O(n^2)
 *    worst    -    O(n^2)
 *
 *    @param[in][out]    _arrary        Array to sort
 *    @param[in]        _length        Length of the array
 **/
template<class T>
inline void icDoubleBubbleDown(T* _array, size_t _length)
{
}// END FUNCTION icDoubleBubbleDown(T* _arrary, size_t _length)


/*! Quick sort
 *
**/
template<class T>
inline void icQuickSortUp(T* _arrary, size_t _p, size_t _r)
{
    if (_p < _r)
    {

    }
};

/*! Quick sort
 *
**/
template<class T>
inline void icQuickSortDown(T* _arrary, size_t _length)
{
};


/*! Counting sort algorithm
 *  TODO: specialize this template to prevent using wrong data types
 *
 *  @param[in]      _arrayIn    The unsorted array
 *  @param[out]     _arrayOut   The sorted array
 *  @param          _length     The length of _arrayIn AND _arrayOut
 *  @param          _maxVal     The maximum value of _arrayIn
**/
template<class T>
inline void icCountSort(T* _arrayIn, T* _arrayOut,
                        size_t _length, size_t _maxVal)
{
    // get a temporary array
    T* C = (T*)malloc(sizeof(T)*(_maxVal+1));
    if (C)
    {
        size_t i;

        // initialize our temp array to all 0s
        memset(C,0,sizeof(T)*(_maxVal+1));

        // make C[i] == # of elements that are == to i
        for (i=0; i<_length; ++i)
            ++C[_arrayIn[i]];

        // make C[i] == # of elements that are <= to i
        for (i=1; i<=_maxVal; ++i)
            C[i] += C[i-1];

        // fill the output array with the sorted data
        for (i=_length-1; i>=0; --i)
            _arrayOut[C[_arrayIn[i]]-- - 1] = _arrayIn[i];

        // release the temporary array
        free(C);
    }
}// END FUNCTION icCountSort(T* _arrayIn, T* _arrayOut,
 //                          size_t _length, size_t _maxVal)

#endif //__IC_SORT_H__

