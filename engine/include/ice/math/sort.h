#pragma once
namespace ice::math {
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
} // namespace ice::math

#include "ice/math/common_math.h"
namespace ice::math {

/*!    Insertion sort in ascending order
 *
 *    This is really good for small data sets or
 *    data sets which are already mostly sorted.
 *
 *    best    -    O(n)
 *    avg        -    O(n^2)
 *    worst    -    O(n^2)
 *
 *    @param[in][out]    array        Array to sort
 *    @param[in]        length        Length of the array
 **/
template <class T> inline void insert_sort_up(T *array, size_t length) {
	for (size_t j = 1; j < length; ++j) {
		int i;
		T   key = array[j];
		for (i = j - 1; i >= 0 && array[i] > key; --i)
			array[i + 1] = array[i];
		array[i + 1] = key;
	}
}

/*!    Insertion sort in descending order
 *
 *    This is really good for small data sets or
 *    data sets which are already mostly sorted.
 *
 *    best    -    n
 *    avg        -    n^2
 *    worst    -    n^2
 *
 *    @param[in][out]    array        Array to sort
 *    @param[in]        length        Length of the array
 **/
template <class T> inline void insert_sort_down(T *array, size_t length) {
	for (size_t j = 1; j < length; ++j) {
		int i;
		T   key = array[j];
		for (i = j - 1; i >= 0 && array[i] < key; --i)
			array[i + 1] = array[i];
		array[i + 1] = key;
	}
}

/*! Merge sort
 *
 **/
template <class T> inline void merge_sort_up(T *array, size_t length) {}

/*! Merge sort
 *
 **/
template <class T> inline void merge_sort_down(T *array, size_t length) {}

/*!    Simle bubble sort in ascending order
 *
 *    This is really not good for anything
 *
 *    best    -    O(n^2)
 *    avg        -    O(n^2)
 *    worst    -    O(n^2)
 *
 *    @param[in][out]    array        Array to sort
 *    @param[in]        length        Length of the array
 **/
template <class T> inline void bubble_sort_up(T *array, size_t length) {
	for (size_t i = 0; i < length; ++i)
		for (size_t j = length - 1; j > i; --j)
			if (array[j] < array[j - 1])
				swap(array[j], array[j - 1]);
}

/*!    Simple bubble sort in descending order
 *
 *    This is really not good for anything
 *
 *    best    -    O(n^2)
 *    avg        -    O(n^2)
 *    worst    -    O(n^2)
 *
 *    @param[in][out]    array        Array to sort
 *    @param[in]        length        Length of the array
 **/
template <class T> inline void bubble_sort_down(T *array, size_t length) {
	for (size_t i = 0; i < length; ++i)
		for (size_t j = length - 1; j > i; --j)
			if (array[j] > array[j - 1])
				swap(array[j], array[j - 1]);
}

/*!    Modified bubble sort in ascending order
 *
 *    This is really not good for anything
 *
 *    best    -    O(n^2)
 *    avg        -    O(n^2)
 *    worst    -    O(n^2)
 *
 *    @param[in][out]    array        Array to sort
 *    @param[in]        length        Length of the array
 **/
template <class T> inline void double_bubble_up(T *array, size_t length) {
	size_t end   = length - 1;
	bool   lto_r = true;
	for (size_t i = 0; i < end; lto_r = !lto_r) {
		if (lto_r) {
			/// This loop bubbles larger values from L -> R
			for (size_t j = i; j < end; ++j) {
				if (array[j + 1] < array[j])
					swap(array[j + 1], array[j]);
			}
			/// decrement the end 1 because we know that the largest
			/// value from i->end was just put there
			--end;
		} else {
			/// This loop bubbles smaller values from R -> L
			for (size_t j = end; j > i; --j) {
				if (array[j] < array[j - 1])
					swap(array[j], array[j - 1]);
			}
			/// increment i because we know that the smallest value
			/// from i->end was just put there
			++i;
		}
	}
}

/*!    Modified bubble sort in descending order
 *
 *    This is really not good for anything
 *
 *    best    -    O(n^2)
 *    avg        -    O(n^2)
 *    worst    -    O(n^2)
 *
 *    @param[in][out]    array        Array to sort
 *    @param[in]        length        Length of the array
 **/
template <class T> inline void double_bubble_down(T *array, size_t length) {}

/*! Quick sort
 *
 **/
template <class T> inline void quick_sort_up(T *arrary, size_t p, size_t r) {
	if (p < r) {
	}
};

/*! Quick sort
 *
 **/
template <class T> inline void quick_sort_down(T *arrary, size_t length) {};

/*! Counting sort algorithm
 *  TODO: specialize this template to prevent using wrong data types
 *
 *  @param[in]      array_in    The unsorted array
 *  @param[out]     array_out   The sorted array
 *  @param          length     The length of _arrayIn AND _arrayOut
 *  @param          max_val     The maximum value of _arrayIn
 **/
template <class T>
inline void count_sort(T *array_in, T *array_out, size_t length, size_t max_val) {
	/// get a temporary array
	T *c = (T *)malloc(sizeof(T) * (max_val + 1));
	if (c) {
		size_t i;

		/// initialize our temp array to all 0s
		memset(c, 0, sizeof(T) * (max_val + 1));

		/// make C[i] == # of elements that are == to i
		for (i = 0; i < length; ++i)
			++c[array_in[i]];

		/// make C[i] == # of elements that are <= to i
		for (i = 1; i <= max_val; ++i)
			c[i] += c[i - 1];

		/// fill the output array with the sorted data
		for (i = length - 1; i >= 0; --i)
			array_out[c[array_in[i]]-- - 1] = array_in[i];

		/// release the temporary array
		free(c);
	}
}
//                          size_t _length, size_t _maxVal)

#endif //__IC_SORT_H__

} // namespace ice::math
