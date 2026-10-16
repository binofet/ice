#include "ice/core/sha256.h"
#include "ice/data/json.h"
// Compile supported public APIs together as an external consumer.
#include "ice/animation/scene_animation.h"
#ifdef ICE_HAS_AUDIO
#include "ice/audio/capture.h"
#endif
#include "ice/assets/primitive_mesh.h"
#include "ice/assets/scene_asset.h"
#include "ice/commands/service.h"
#include "ice/core/expected.h"
#include "ice/core/utf8.h"
#include "ice/data/document.h"
#include "ice/input/companion.h"
#include "ice/input/companion_room.h"
#include "ice/io/resource_uri.h"
#include "ice/math/types.h"
#include "ice/meta/schema.h"
#ifdef ICE_HAS_PHYSICS_2D
#include "ice/physics/world2d.h"
#endif
#ifdef ICE_HAS_PHYSICS_3D
#include "ice/physics/world3d.h"
#endif
#include "ice/net/companion_link.h"
#include "ice/net/companion_web.h"
#include "ice/platform/host.h"
#include "ice/platform/input_types.h"
#include "ice/platform/platform_events.h"
#include "ice/render/camera.h"
#include "ice/render/device.h"
#include "ice/render/live_pipeline.h"
#include "ice/render/scene_camera.h"
#include "ice/render/shader_snapshot.h"
#ifdef ICE_HAS_SCRIPTING
#include "ice/script/lua_program.h"
#endif
#ifdef ICE_HAS_METAL
#include "ice/render/metal/device.h"
#endif
#ifdef ICE_HAS_OPENGL
#include "ice/render/gl/device.h"
#endif
#include "ice/util/join_qr.h"
#ifdef ICE_HAS_DESKTOP
#include "ice/platform/desktop_window.h"
#endif
