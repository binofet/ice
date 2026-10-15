#pragma once

#include "ice/globals.h"
namespace ice::render {

#define IC_INDEX_USAGE_UNKOWN 0
#define IC_INDEX_USAGE_DYNAMIC 1
#define IC_INDEX_USAGE_STATIC 2

/// Legacy 16-bit or 32-bit index element width.
enum index_size { ic_index_size_unkown = 0, ic_index_size_16, ic_index_size_32 };

/// Index count, element width, topology, and usage for a buffer.
struct index_def {
	/// Legacy allocation/usage hint bit mask.
	ice::uchar usage;
	/// Number of index elements in the buffer.
	ice::uint num_index;
	/// Byte-width selection for index elements.
	ice::render::index_size index_size;
};

/// Borrowed writable index lock region; valid only until the matching unlock.
struct index_lock {
	/// Number of index elements in the buffer.
	ice::uint num_index;
	/// Byte-width selection for index elements.
	ice::render::index_size index_size;
	/// Borrowed lock bytes; valid only for the documented lock lifetime.
	void *data;
};

/// Owned legacy index buffer; locks borrow storage until unlock.
class index_buf {
public:
	/// Construct inactive state; initialize explicitly before active use.
	index_buf(void) {
		index_buf_ = NULL;
		locked_    = false;
	};
	/// Destroy this instance under the ownership and native-context lifetime rules documented by
	/// its type.
	virtual ~index_buf(void) { cleanup(); };

	/// Release resources owned by this subsystem; borrowed dependencies must remain alive until
	/// cleanup completes.
	virtual ice::result cleanup(void) { return IC_OK; };

	/// Borrow a writable CPU region for the resource; unlock before drawing, releasing, or
	/// relocking it.
	virtual ice::result lock(index_lock *index_lock) = 0;
	/// Commit/release the active CPU lock; borrowed lock pointers become invalid.
	virtual ice::result unlock(void) = 0;

	//! Gets the index buffer object
	//\returns void** Pointer to pointer of index object
	inline void **get_index_buffer(void) { return &index_buf_; };

	//! Gets a pointer to the index buffer definition struct
	//\returns icIndexDef* Pointer to index definition
	inline index_def *get_index_def(void) { return &description_; };

protected:
	void     *index_buf_;   //! Pointer to index buffer data
	bool      locked_;      //! Tells if index buffer is locked
	index_def description_; //! Defines this index buffer
};

} // namespace ice::render
