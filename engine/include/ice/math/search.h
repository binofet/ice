#pragma once
namespace ice::math {

/*! Finds the peak element in a unimodal array
 *
 *    Runtime is O(lgn)
 *
 *    @param[in]    array        Unimodal array
 *    @param[in]    length       The number of elements in _array
 *    @returns      T*            Pointer to the peak element
 **/
template <class T> inline T *peak_search(T *const array, const size_t length) {
	T *a = &array[0];
	T *b = &array[length - 1];
	while (a < b) {
		size_t mid = (b - a) / 2;
		if (a[mid] < a[mid + 1])
			a += mid + 1; // we eliminate everything < a[mid]
		else
			b = &a[mid]; // we eleminate everything > a[mid]
	}
	return a;
}

} // namespace ice::math
