#pragma once
namespace ice::assets {
class content_loader;
}

#include "ice/globals.h"

#include "ice/math/mat4.h"
namespace ice::render {

#define ICE_MODEL_FORMAT 102

/// Legacy binary model header; values describe serialized vertex/index payloads, not native
/// pointers.
struct e_model {
	/// Serialized legacy model format version.
	ice::uint model_version;
	/// Serialized legacy model identifier bytes.
	char model_name[64];
	/// Serialized/source diffuse-texture name.
	char dif_tex[128];
	/// Serialized/source normal-texture name.
	char norm_tex[128];
	/// Serialized/source specular-texture name.
	char spec_tex[128];
	/// Number of vertex records.
	ice::uint num_verts;
	/// Number of serialized index elements.
	ice::uint num_ind;
	/// Legacy packed vertex-layout code.
	ice::uint vert_type;
};

/// Version-100 legacy model header extension and serialized layout/count metadata.
struct e_model_100 {
	/// Serialized legacy model format version.
	ice::uint model_version;
	/// Serialized legacy model identifier bytes.
	char model_name[64];
	/// Serialized/source diffuse-texture name.
	char dif_tex[128];
	/// Serialized/source normal-texture name.
	char norm_tex[128];
	/// Serialized/source specular-texture name.
	char spec_tex[128];
	/// Number of vertex records.
	ice::uint num_verts;
	/// Number of serialized index elements.
	ice::uint num_ind;
};

class texture;
class vert_buffer;
class index_buf;
class graphics_device;

/*! Provides an object for interacting with models
 **/
class model {
public:
	/// Construct inactive state; initialize explicitly before active use.
	model(void);
	/// Destroy this instance under the ownership and native-context lifetime rules documented by
	/// its type.
	~model(void);

	/// Render using the current owning graphics context; inspect the result before continuing a
	/// failed frame.
	ice::result render(const ice::math::mat4 &trans);

	/// Compare the value/ordering represented by this type; references and resources remain owned
	/// by their original objects.
	int operator<(const model &rhs);
	/// Compare the value/ordering represented by this type; references and resources remain owned
	/// by their original objects.
	int operator>(const model &rhs);
	/// Compare the value/ordering represented by this type; references and resources remain owned
	/// by their original objects.
	int operator<=(const model &rhs);
	/// Compare the value/ordering represented by this type; references and resources remain owned
	/// by their original objects.
	int operator>=(const model &rhs);
	/// Compare the value/ordering represented by this type; references and resources remain owned
	/// by their original objects.
	int operator==(const model &rhs);
	/// Compare the value/ordering represented by this type; references and resources remain owned
	/// by their original objects.
	int operator!=(const model &rhs);

private:
	vert_buffer *vb_;
	index_buf   *ib_;
	texture     *dif_tex_;
	texture     *norm_tex_;
	texture     *spec_tex_;

	graphics_device *device_;

	e_model *header_;

	friend class ice::assets::content_loader;
};

} // namespace ice::render
