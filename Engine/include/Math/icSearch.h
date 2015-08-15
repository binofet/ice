#ifndef __IC_SEARCH_H__
#define __IC_SEARCH_H__


/*! Finds the peak element in a unimodal array
 *
 *    Runtime is O(lgn)
 *
 *    @param[in]    _array        Unimodal array
 *    @param[in]    _length       The number of elements in _array
 *    @returns      T*            Pointer to the peak element
**/
template<class T>
inline T* icPeakSearch(T* const _array, const size_t _length)
{
    T* a = &_array[0];
    T* b = &_array[_length-1];
    while (a < b)
    {
        size_t mid = (b-a)/2;
        if (a[mid] < a[mid+1])
            a+=mid+1;  // we eliminate everything < a[mid]
        else
            b=&a[mid]; // we eleminate everything > a[mid]
    }
    return a;
}// END FUNCTION icPeakSearch(T* const _array, const size_t _size)

#endif //__IC_SEARCH_H__