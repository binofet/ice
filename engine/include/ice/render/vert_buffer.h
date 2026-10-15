#pragma once

#include "ice/globals.h"

#include "ice/math/color.h"
#include "ice/math/vec2.h"
#include "ice/math/vec3.h"
namespace ice::render {

#define IC_VERT_USAGE_UNKOWN 0
#define IC_VERT_USAGE_DYNAMIC 1
#define IC_VERT_USAGE_STATIC 2

/// enable this to force 32 byte alignment on vert structures
#define ICPADVERTS

/*! Available primitives
 **/
enum prim {
	ic_prim_type_invalid = 0,
	ic_prim_plist, //! Point List
	prim_llist,    //! Line List
	prim_lstrip,   //! Line Strip
	prim_tlist,    //! Triangle List
	prim_tstrip,   //! Triangle Strip
	prim_tfan,     //! Trinalge Fan

	prim_force_dword = 0x7FFFFFFF, //! Force enumeration compile to 32 bit size
};

/*! Different vertex types
 **/
enum vert {
	ic_vert_ivalid = 0,
	ic_vert_2_dcol,
	ic_vert_2_d_dif,
	ic_vert_2_dcol_dif,
	ic_vert_col,
	ic_vert_dif,
	ic_vert_multitex2,
	ic_vert_multitex3,
	ic_vert_multitex4,
	ic_vert_nrmcol,
	ic_vert_nrmcolspc,
	ic_vert_nrm_dif,
	ic_vert_nrmcol_dif,
	ic_vert_nrmcolspc_dif,
	ic_vert_difnrm,
	ic_vert_col_difnrm,
	ic_vert_difnrmspc,
	ic_vert_col_difnrmspc,

	ic_vert_def_count,

	ic_vert_force_dword = 0x7FFFFFFF, //! Force enumeration compile to 32 bit size
};

/*! Defines paramters for a vertex buffer
 **/
struct vert_def {
	/// Byte stride of one legacy vertex record.
	ice::uint  vert_size; //! Size of each vertex (in bytes)
	ice::uint  num_verts; //! Number of verticies
	ice::uint  num_prims; //! The number of primitives
	ice::uchar usage;     //! How this buffer is to be used
	prim       prim_type; //! Type of primitive
	vert       vert_type;
};

/*! Used to lock a vertex buffer
 **/
struct vert_lock {
	/// Byte stride of one legacy vertex record.
	ice::uint vert_size; //! Size of verts in locked buffer
	ice::uint num_verts; //! Number of verts locked
	void     *data;      //! Pointer to locked vertex data
};

/// ICE VERTEX TYPES

struct vrt2_dcol {
	/// Position in the coordinate space documented by the owning record.
	ice::math::vec2 pos;
	/// Color value used by this legacy state record.
	ice::math::color color;
};

/// Packed legacy vertex layout; field order and scalar widths define GPU upload offsets.
struct vrt2_d_dif {
	/// Position in the coordinate space documented by the owning record.
	ice::math::vec2 pos; // 8
	/// Texture coordinate sets; orientation is interpreted by the owning material/renderer.
	ice::math::vec2 uv; // 8
};

/// Packed legacy vertex layout; field order and scalar widths define GPU upload offsets.
struct vrt2_dcol_dif {
	/// Position in the coordinate space documented by the owning record.
	ice::math::vec2 pos; // 8
	/// Color value used by this legacy state record.
	ice::math::color color; // 4
	/// Texture coordinate sets; orientation is interpreted by the owning material/renderer.
	ice::math::vec2 uv; // 8
#ifdef ICPADVERTS
	/// Reserved serialized bytes; do not interpret as host pointers.
	char padding[12];
#endif
};

/// Packed legacy vertex layout; field order and scalar widths define GPU upload offsets.
struct vrtcol {
	/// Position in the coordinate space documented by the owning record.
	ice::math::vec3 pos; // 12
	/// Color value used by this legacy state record.
	ice::math::color color; // 4
	/// Reserved serialized bytes; do not interpret as host pointers.
	char padding[16];
};

/// Packed legacy vertex layout; field order and scalar widths define GPU upload offsets.
struct vrt_dif {
	/// Position in the coordinate space documented by the owning record.
	ice::math::vec3 pos; // 12
	/// Texture coordinate sets; orientation is interpreted by the owning material/renderer.
	ice::math::vec2 uv; // 8
#ifdef ICPADVERTS
	/// Reserved serialized bytes; do not interpret as host pointers.
	char padding[12];
#endif
};

/// Packed legacy vertex layout; field order and scalar widths define GPU upload offsets.
struct vrt_multitex2 {
	/// Position in the coordinate space documented by the owning record.
	ice::math::vec3 pos;
	/// First texture-coordinate set.
	ice::math::vec2 uv1;
	/// Second texture-coordinate set.
	ice::math::vec2 uv2;
};

/// Packed legacy vertex layout; field order and scalar widths define GPU upload offsets.
struct vrt_multitex3 {
	/// Position in the coordinate space documented by the owning record.
	ice::math::vec3 pos;
	/// First texture-coordinate set.
	ice::math::vec2 uv1;
	/// Second texture-coordinate set.
	ice::math::vec2 uv2;
	/// Third texture-coordinate set.
	ice::math::vec2 uv3;
};

/// Packed legacy vertex layout; field order and scalar widths define GPU upload offsets.
struct vrt_multitex4 {
	/// Position in the coordinate space documented by the owning record.
	ice::math::vec3 pos;
	/// First texture-coordinate set.
	ice::math::vec2 uv1;
	/// Second texture-coordinate set.
	ice::math::vec2 uv2;
	/// Third texture-coordinate set.
	ice::math::vec2 uv3;
	/// Fourth texture-coordinate set.
	ice::math::vec2 uv4;
};

/// Packed legacy vertex layout; field order and scalar widths define GPU upload offsets.
struct vrtnrmcol {
	/// Position in the coordinate space documented by the owning record.
	ice::math::vec3 pos;
	/// Legacy mesh-local vertex normal.
	ice::math::vec3 norm;
	/// Color value used by this legacy state record.
	ice::math::color color;
#ifdef ICPADVERTS
	/// Reserved serialized bytes; do not interpret as host pointers.
	char padding[4];
#endif
};

/// Packed legacy vertex layout; field order and scalar widths define GPU upload offsets.
struct vrtnrmcolspc {
	/// Position in the coordinate space documented by the owning record.
	ice::math::vec3 pos;
	/// Legacy mesh-local vertex normal.
	ice::math::vec3 norm;
	/// Color value used by this legacy state record.
	ice::math::color color;
	/// Legacy packed/specular vertex color.
	float spec;
};

/// Packed legacy vertex layout; field order and scalar widths define GPU upload offsets.
struct vrtnrm_dif {
	/// Position in the coordinate space documented by the owning record.
	ice::math::vec3 pos;
	/// Legacy mesh-local vertex normal.
	ice::math::vec3 norm;
	/// Texture coordinate sets; orientation is interpreted by the owning material/renderer.
	ice::math::vec2 uv;
};

/// Packed legacy vertex layout; field order and scalar widths define GPU upload offsets.
struct vrtnrmcol_dif {
	/// Position in the coordinate space documented by the owning record.
	ice::math::vec3 pos;
	/// Legacy mesh-local vertex normal.
	ice::math::vec3 norm;
	/// Color value used by this legacy state record.
	ice::math::color color;
	/// Texture coordinate sets; orientation is interpreted by the owning material/renderer.
	ice::math::vec2 uv;
};

/// Packed legacy vertex layout; field order and scalar widths define GPU upload offsets.
struct vrtnrmcolspc_dif {
	/// Position in the coordinate space documented by the owning record.
	ice::math::vec3 pos;
	/// Legacy mesh-local vertex normal.
	ice::math::vec3 norm;
	/// Color value used by this legacy state record.
	ice::math::color color;
	/// Legacy packed/specular vertex color.
	float spec;
	/// Texture coordinate sets; orientation is interpreted by the owning material/renderer.
	ice::math::vec2 uv;
};

/// Packed legacy vertex layout; field order and scalar widths define GPU upload offsets.
struct vrt_difnrm {
	/// Position in the coordinate space documented by the owning record.
	ice::math::vec3 pos;
	/// Diffuse-map texture coordinates.
	ice::math::vec2 uv_dif;
	/// Normal-map texture coordinates.
	ice::math::vec2 uv_norm;
};

/// Packed legacy vertex layout; field order and scalar widths define GPU upload offsets.
struct vrtcol_difnrm {
	/// Position in the coordinate space documented by the owning record.
	ice::math::vec3 pos;
	/// Color value used by this legacy state record.
	ice::math::color color;
	/// Diffuse-map texture coordinates.
	ice::math::vec2 uv_dif;
	/// Normal-map texture coordinates.
	ice::math::vec2 uv_norm;
};

/// Packed legacy vertex layout; field order and scalar widths define GPU upload offsets.
struct vrt_difnrmspc {
	/// Position in the coordinate space documented by the owning record.
	ice::math::vec3 pos;
	/// Diffuse-map texture coordinates.
	ice::math::vec2 uv_dif;
	/// Normal-map texture coordinates.
	ice::math::vec2 uv_norm;
	/// Specular-map texture coordinates.
	ice::math::vec2 uv_spec;
};

/// Packed legacy vertex layout; field order and scalar widths define GPU upload offsets.
struct vrtcol_difnrmspc {
	/// Position in the coordinate space documented by the owning record.
	ice::math::vec3 pos;
	/// Color value used by this legacy state record.
	ice::math::color color;
	/// Diffuse-map texture coordinates.
	ice::math::vec2 uv_dif;
	/// Normal-map texture coordinates.
	ice::math::vec2 uv_norm;
	/// Specular-map texture coordinates.
	ice::math::vec2 uv_spec;
};

/*! Provides an interface for vertex buffers
 **/
class vert_buffer {
public:
	/// Construct inactive state; initialize explicitly before active use.
	vert_buffer(void) {
		ptr_vert_buff_ = NULL;
		locked_        = false;
	};
	/// Destroy this instance under the ownership and native-context lifetime rules documented by
	/// its type.
	virtual ~vert_buffer(void) { cleanup(); };

	/// Release resources owned by this subsystem; borrowed dependencies must remain alive until
	/// cleanup completes.
	virtual ice::result cleanup(void) { return IC_OK; };

	/// Borrow a writable CPU region for the resource; unlock before drawing, releasing, or
	/// relocking it.
	virtual ice::result lock(vert_lock *vert_lock) = 0;
	/// Commit/release the active CPU lock; borrowed lock pointers become invalid.
	virtual ice::result unlock(void) = 0;

	//! Gets a pointer to the vertex buffer object
	//\returns void** Pointer to pointer of vb object
	inline void **get_vert_buff(void) { return &ptr_vert_buff_; };

	//! Gets a pointer to the vertex buffer definition struct
	//\returns icIndexDef* Pointer to vertex definition
	inline vert_def *get_vert_def(void) { return &description_; };

	//! Queries to see if buffer is currently locked
	//\returns      bool        true if locked.
	inline bool is_locked(void) { return locked_; };

protected:
	void    *ptr_vert_buff_; //! Pointer to vertex buffer data
	bool     locked_;        //! Tells if vertex buffer is locked
	vert_def description_;   //! Defines this vertex buffer
};

} // namespace ice::render
