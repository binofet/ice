

#include "ice/render/gl/graphics_device.h"
#include "ice/platform/window.h"

#include "ice/assets/image.h"
namespace ice::render::gl {

#ifdef ICGL
} // namespace ice::render::gl

#include "ice/render/gl/font_gl.h"
#include "ice/render/gl/index_buffer.h"
#include "ice/render/gl/texture.h"
#include "ice/render/gl/vertex_buffer.h"
namespace ice::render::gl {

#ifdef WIN32
} // namespace ice::render::gl
#include "GL/glu.h"
#include "ice/platform/win32/window.h"
namespace ice::render::gl {
#elif defined(__APPLE__) || defined(__APPLE_CC__)
} // namespace ice::render::gl
#include "ice/platform/macos/window.h"
#include <OpenGL/glu.h>
namespace ice::render::gl {
using platform_gl_window = ice::platform::macos::window;
#elif (defined(X11))
} // namespace ice::render::gl
#include "ice/platform/x11/window.h"
#include <GL/glu.h>
namespace ice::render::gl {
using platform_gl_window = ice::platform::x11::window;
#endif

/*! Default Constructor
 **/
graphics_device::graphics_device(void) {
#ifdef WIN32
	device_context_ = NULL;
	render_context_ = NULL;
#endif

	(view_matrix_) = ice::math::mat4(1);
#if defined(__APPLE__) || defined(X11)
	native_context_ = NULL;
#endif
	hardware_buffers_ = false;
	col_clear_        = ice::math::color::bblue;
}

/*! Default Destructor
 **/
graphics_device::~graphics_device(void) { cleanup(); }

/*! Cleans up the device, frees any allocated memory
 *
 *    @returns    ICRESULT    Result of cleanup
 *
 **/
ice::result graphics_device::cleanup(void) {
	ice::result result_code = IC_OK;
#ifdef WIN32
	if (device_context_) {
		if (!wglMakeCurrent(device_context_, NULL))
			result_code = IC_FAIL_GEN;
		// if (ReleaseDC((HWND)main_window_, m_hDC)==0)
		//     result_code = IC_FAIL_GEN;
	}

	if (render_context_) {
		if (!wglDeleteContext(render_context_))
			result_code = IC_FAIL_GEN;
	}
	if (device_context_ && main_window_)
		ReleaseDC(static_cast<ice::platform::win32::window *>(main_window_)->get_hwnd(),
		          device_context_);
	device_context_ = NULL;
	render_context_ = NULL;

#elif defined(__APPLE__) || defined(X11)

	if (native_context_ && main_window_)
		static_cast<platform_gl_window *>(main_window_)->destroy_gl_context();
	native_context_ = nullptr;
#elif (defined(X11))
#endif

	return result_code;
}

/*! Initializes the openGL device
 *
 *    @param[in]    window        Pointer to the window
 *    @returns    ICRESULT    Status of initialization
 *
 **/
ice::result graphics_device::init(ice::platform::window *const window) {
	// we can only create a device with a handle to the window
	if (window) {
		main_window_       = window;
		ice::result result = create_device();
		if (ICEFAIL(result))
			return result;
		// GL starts with depth testing off; match the device's declared default.
		// Previously the first UI widget happened to enable it as a side effect.
		return enable_depth_test(depth_test_);
	}

	return IC_FAIL_GEN;
}

/*! Creates the openGL device
 *
 *    @returns    ICRESULT    Result of device creation
 *
 **/
ice::result graphics_device::create_device() {
#if (defined(_MSC_VER) || defined(__CYGWIN__) || defined(__MINGW32__))
	HWND hwnd       = ((ice::platform::win32::window *)main_window_)->get_hwnd();
	device_context_ = GetDC(hwnd);

	int nPixelFormat;

	static PIXELFORMATDESCRIPTOR pfd = {sizeof(PIXELFORMATDESCRIPTOR), // Size of this structure
	                                    1,                             // Version of this structure
	                                    PFD_DRAW_TO_WINDOW |     // Draw to window (can be bitmap)
	                                        PFD_SUPPORT_OPENGL | // Support openGL calls in window
	                                        PFD_DOUBLEBUFFER |   // Double-buffered mode
	                                        PFD_TYPE_RGBA,       // RGBA color mode
	                                    32,                      // color depth
	                                    0,
	                                    0,
	                                    0,
	                                    0,
	                                    0,
	                                    0, // not used to select mode
	                                    0,
	                                    0, // not used to select mode
	                                    0,
	                                    0,
	                                    0,
	                                    0,
	                                    0,  // not used to select mode
	                                    16, // Size of depth buffer
	                                    0,  // not used here
	                                    0,  // not used here
	                                    0,  // not used here
	                                    0,  // not used here
	                                    0,
	                                    0,
	                                    0}; // not used here

	// choose pixel format
	nPixelFormat = ChoosePixelFormat(device_context_, &pfd);

	// set the pixel format
	if (!device_context_ || !nPixelFormat || !SetPixelFormat(device_context_, nPixelFormat, &pfd))
		return IC_FAIL_CREATE_DEVICE;

	render_context_ = wglCreateContext(device_context_);
	if (!render_context_ || !wglMakeCurrent(device_context_, render_context_))
		return IC_FAIL_CREATE_DEVICE;

	ice::platform::window_def windef;
	main_window_->get_params(&windef);

	width_  = windef.width;
	height_ = windef.height;

	// Set the viewport to be the entire window
	glViewport(0, 0, width_, height_);

	screen_ortho_ = glm::orthoLH_ZO(float(0.0f), float((ice::math::real)width_), float(0.0f),
	                                float((ice::math::real)height_), float(-1.0f), float(1.0f));

	// obtain information about the clients opengl version/capabilities
	ps_gl_vendor_     = (const char *)glGetString(GL_VENDOR);
	ps_gl_render_     = (const char *)glGetString(GL_RENDERER);
	ps_gllib_         = (const char *)glGetString(GL_VERSION);
	ps_gl_ext_        = (const char *)glGetString(GL_EXTENSIONS);
	ps_gl_shader_ver_ = (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);

	// LOAD EXTENSIONS
	wglSwapIntervalEXT = (SWAPINTEXT)wglGetProcAddress("wglSwapIntervalEXT");

	hardware_buffers_ = IsExtensionSupported("GL_ARB_vertex_buffer_object", ps_gl_ext_);
	if (!hardware_buffers_) {
		ice::warning(
		    "Airhockey requires OpenGL vertex buffer objects; install the GPU OpenGL driver.");
		return IC_FAIL_CREATE_DEVICE;
	}
	if (hardware_buffers_) {
		vert_buff_gl::hardware_enabled = true;
		index_buffer::hardware_enabled = true;
		// TODO: verify these function pointers come back!
		glGenBuffers = (PFNGLGENBUFFERS)wglGetProcAddress("glGenBuffers");
		if (!glGenBuffers)
			glGenBuffers = (PFNGLGENBUFFERS)wglGetProcAddress("glGenBuffersARB");
		glBindBuffer = (PFNGLBINDBUFFER)wglGetProcAddress("glBindBuffer");
		if (!glBindBuffer)
			glBindBuffer = (PFNGLBINDBUFFER)wglGetProcAddress("glBindBufferARB");
		glBufferData = (PFNGLBUFFERDATA)wglGetProcAddress("glBufferData");
		if (!glBufferData)
			glBufferData = (PFNGLBUFFERDATA)wglGetProcAddress("glBufferDataARB");
		glDeleteBuffers = (PFNGLDELETEBUFFERS)wglGetProcAddress("glDeleteBuffers");
		if (!glDeleteBuffers)
			glDeleteBuffers = (PFNGLDELETEBUFFERS)wglGetProcAddress("glDeleteBuffersARB");
		glMapBuffer = (PFNGLMAPBUFFER)wglGetProcAddress("glMapBuffer");
		if (!glMapBuffer)
			glMapBuffer = (PFNGLMAPBUFFER)wglGetProcAddress("glMapBufferARB");
		glUnmapBuffer = (PFNGLUNMAPBUFFER)wglGetProcAddress("glUnmapBuffer");
		if (!glUnmapBuffer)
			glUnmapBuffer = (PFNGLUNMAPBUFFER)wglGetProcAddress("glUnmapBufferARB");
	}

	// TODO:
	// this needs to be optional, disables vertical sync
	if (wglSwapIntervalEXT)
		wglSwapIntervalEXT(0);

	// GLfloat ambient[] = {1.0f,1.0f,1.0f,1.0f};
	GLfloat fNoLight[]     = {0.0f, 0.0f, 0.0f, 0.0f};
	GLfloat fLowLight[]    = {0.25f, 0.25f, 0.25f, 1.0f};
	GLfloat fBrightLight[] = {1.0f, 1.0f, 1.0f, 1.0f};

	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, fNoLight);
	glLightfv(GL_LIGHT0, GL_AMBIENT, fLowLight);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, fBrightLight);
	glLightfv(GL_LIGHT0, GL_SPECULAR, fBrightLight);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);

	glEnable(GL_COLOR_MATERIAL);
	glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
	glMaterialfv(GL_FRONT, GL_SPECULAR, fBrightLight);
	glMateriali(GL_FRONT, GL_SHININESS, 128);

	// todo, what if this fails?
	return IC_OK;
#elif defined(__APPLE__) || defined(X11)
	auto window = static_cast<platform_gl_window *>(main_window_);
	if (!window)
		return IC_FAIL_CREATE_DEVICE;
	native_context_ = window->create_gl_context();
	if (!native_context_)
		return IC_FAIL_CREATE_DEVICE;
	window->make_current();
	hardware_buffers_              = true;
	vert_buff_gl::hardware_enabled = true;
	index_buffer::hardware_enabled = true;
	int width, height;
	window->get_framebuffer_size(&width, &height);
	change_size(width, height);
	ps_gl_vendor_     = (const char *)glGetString(GL_VENDOR);
	ps_gl_render_     = (const char *)glGetString(GL_RENDERER);
	ps_gllib_         = (const char *)glGetString(GL_VERSION);
	ps_gl_ext_        = (const char *)glGetString(GL_EXTENSIONS);
	ps_gl_shader_ver_ = (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
	ice::printf("OpenGL %s / %s", ps_gllib_, ps_gl_render_);
	GLfloat no_light[]     = {0.0f, 0.0f, 0.0f, 0.0f};
	GLfloat low_light[]    = {0.25f, 0.25f, 0.25f, 1.0f};
	GLfloat bright_light[] = {1.0f, 1.0f, 1.0f, 1.0f};

	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, no_light);
	glLightfv(GL_LIGHT0, GL_AMBIENT, low_light);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, bright_light);
	glLightfv(GL_LIGHT0, GL_SPECULAR, bright_light);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);

	glEnable(GL_COLOR_MATERIAL);
	glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
	glMaterialfv(GL_FRONT, GL_SPECULAR, bright_light);
	glMateriali(GL_FRONT, GL_SHININESS, 128);

	return IC_OK;
#elif (defined(LINUX))
	return IC_FAIL_CREATE_DEVICE;
#else
	return IC_FAIL_CREATE_DEVICE;
#endif
}

/*! Updates various stats in the engine
 *
 *    @returns    ICRESULT    Result of update
 *
 **/
ice::result graphics_device::update(void) { return ice::render::graphics_device::update(); }

void graphics_device::change_size(ice::ushort size_x, ice::ushort size_y) {
	width_  = (int)size_x;
	height_ = (int)size_y;

	// Set the viewport to be the entire window
	glViewport(0, 0, width_, height_);

	screen_ortho_ = glm::orthoLH_ZO(float(0.0f), float((ice::math::real)width_), float(0.0f),
	                                float((ice::math::real)height_), float(-1.0f), float(1.0f));
}

/*! Clears the backbuffer and starts a new render frame
 **/
void graphics_device::frame_begin(void) {
#if (defined(_MSC_VER) || defined(__CYGWIN__) || defined(__MINGW32__))
	HWND hwnd = ((ice::platform::win32::window *)main_window_)->get_hwnd();
	wglMakeCurrent(device_context_, render_context_);
	int width, height;
	main_window_->get_framebuffer_size(&width, &height);
	if (width > 0 && height > 0 && (width != width_ || height != height_))
		change_size(width, height);
	InvalidateRect(hwnd, NULL, FALSE);
#elif defined(__APPLE__) || defined(X11)
	auto window = static_cast<platform_gl_window *>(main_window_);
	window->make_current();
	int width, height;
	window->get_framebuffer_size(&width, &height);
	if (width > 0 && height > 0 && (width != width_ || height != height_))
		change_size(width, height);
#endif
	glClearColor(col_clear_.red_float(), col_clear_.green_float(), col_clear_.blue_float(),
	             col_clear_.alpha_float());
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	GLfloat ambient [[maybe_unused]][] = {col_ambient_.red_float(), col_ambient_.green_float(),
	                                      col_ambient_.blue_float(), col_ambient_.alpha_float()};
	GLfloat no_light[]                 = {0.0f, 0.0f, 0.0f, 0.0f};
	GLfloat low_light[]                = {0.25f, 0.25f, 0.25f, 1.0f};
	GLfloat bright_light[]             = {1.0f, 1.0f, 1.0f, 1.0f};

	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, no_light);
	glLightfv(GL_LIGHT0, GL_AMBIENT, low_light);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, bright_light);
	glLightfv(GL_LIGHT0, GL_SPECULAR, bright_light);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);

	static GLfloat light_pos[4] = {0.0f, 20.0f, 0.0f, 1.0f}; // Point source
	// static icReal ang = 0.0f;

	// ang += 0.01f;
	// if (ang >= 2 * IC_PI)
	//     ang -= IC_PI;

	// fLightPos[0] = 20.0f * icCos(ang);
	// fLightPos[2] = 20.0f * icSin(ang);

	glLightfv(GL_LIGHT0, GL_POSITION, light_pos);

	glEnable(GL_COLOR_MATERIAL);
	glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
	glMaterialfv(GL_FRONT, GL_SPECULAR, bright_light);
	glMateriali(GL_FRONT, GL_SHININESS, 128);
}

/*! Presents the current frame to the display
 *
 *    @param    swap    Set true to swap buffers
 **/
void graphics_device::frame_end(bool swap) {
#if (defined(_MSC_VER) || defined(__CYGWIN__) || defined(__MINGW32__))
	if (swap) {
		SwapBuffers(device_context_);

		ice::platform::win32::window *win = (ice::platform::win32::window *)main_window_;
		ValidateRect(win->get_hwnd(), NULL);
	} else
		glFinish();

#elif defined(__APPLE__) || defined(X11)
	if (swap)
		static_cast<platform_gl_window *>(main_window_)->swap_buffers();
	else
		glFlush();

#endif
	ice::render::graphics_device::frame_end(swap);
}

/*! Renders a vertex buffer to the current frame
 *
 *  @param[in]    vb             Pointer to the vertex buffer
 *  @returns      ICRESULT        Result of rendering the vert buffer
 **/
ice::result graphics_device::draw_vert_buf(class ice::render::vert_buffer *vb, ice::uint offset,
                                           ice::uint count) {
#ifdef WIN32
	if (device_context_)
#else
	if (native_context_)
#endif
	{
		if (!vb)
			return IC_FAIL_GEN;
		count = (count) ? count : vb->get_vert_def()->num_verts - offset;

		if (vb->is_locked())
			return IC_WARN_ALREADY_LOCKED_VB;

		vert_buff_gl          *gl_vb  = (vert_buff_gl *)vb;
		ice::render::vert_def *vd     = vb->get_vert_def();
		ice::uint              pos_sz = (vd->vert_type > ice::render::ic_vert_2_dcol_dif) ? 3 : 2;

		// TODO: save theses states so we don't needlessly change them
		glEnableClientState(GL_VERTEX_ARRAY);

		if (vd->vert_type == ice::render::ic_vert_nrmcol)
			glEnableClientState(GL_NORMAL_ARRAY);
		if (vd->vert_type == ice::render::ic_vert_2_dcol ||
		    vd->vert_type == ice::render::ic_vert_2_dcol_dif ||
		    vd->vert_type == ice::render::ic_vert_nrmcol ||
		    vd->vert_type == ice::render::ic_vert_col)
			glEnableClientState(GL_COLOR_ARRAY);
		if (vd->vert_type != ice::render::ic_vert_2_dcol &&
		    vd->vert_type == ice::render::ic_vert_2_dcol_dif &&
		    vd->vert_type != ice::render::ic_vert_col)
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);

		if (hardware_buffers_) {
			glBindBuffer(GL_ARRAY_BUFFER, gl_vb->get_name());
			if (vd->vert_type == ice::render::ic_vert_dif) {
				glVertexPointer(pos_sz, GL_FLOAT, sizeof(ice::render::vrt_dif), (GLvoid *)NULL);
				glTexCoordPointer(2, GL_FLOAT, sizeof(ice::render::vrt_dif),
				                  (GLvoid *)(sizeof(ice::math::real) * pos_sz));
			} else if (vd->vert_type == ice::render::ic_vert_2_dcol) {
				glVertexPointer(pos_sz, GL_FLOAT, sizeof(ice::render::vrt2_dcol), (GLvoid *)NULL);
				glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(ice::render::vrt2_dcol),
				               (GLvoid *)(sizeof(ice::math::real) * pos_sz));
			} else if (vd->vert_type == ice::render::ic_vert_2_dcol_dif) {
				glVertexPointer(pos_sz, GL_FLOAT, sizeof(ice::render::vrt2_dcol_dif),
				                (GLvoid *)NULL);
				glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(ice::render::vrt2_dcol_dif),
				               (GLvoid *)(sizeof(ice::math::real) * pos_sz));
				glTexCoordPointer(
				    2, GL_FLOAT, sizeof(ice::render::vrt2_dcol_dif),
				    (GLvoid *)(sizeof(ice::math::real) * pos_sz + sizeof(ice::math::color)));
			} else if (vd->vert_type == ice::render::ic_vert_nrmcol) {
				glVertexPointer(pos_sz, GL_FLOAT, sizeof(ice::render::vrtnrmcol), (GLvoid *)NULL);
				glNormalPointer(GL_FLOAT, sizeof(ice::render::vrtnrmcol),
				                (GLvoid *)(sizeof(ice::math::vec3)));
				glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(ice::render::vrtnrmcol),
				               (GLvoid *)(sizeof(ice::math::vec3) * 2));
			} else if (vd->vert_type == ice::render::ic_vert_col) {
				glVertexPointer(pos_sz, GL_FLOAT, sizeof(ice::render::vrtcol), (GLvoid *)NULL);
				glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(ice::render::vrtcol),
				               (GLvoid *)(sizeof(ice::math::real) * pos_sz));
			}
		} else {
			return IC_NOT_IMPL; // Client-memory drawing is not implemented by this backend.
		}

		// render the buffer
		glDrawArrays(prim_to_gl(vd->prim_type), offset, count);

		// TODO: save theses states so we don't needlessly change them
		glDisableClientState(GL_VERTEX_ARRAY);
		if (vd->vert_type == ice::render::ic_vert_nrmcol)
			glDisableClientState(GL_NORMAL_ARRAY);
		if (vd->vert_type == ice::render::ic_vert_2_dcol ||
		    vd->vert_type == ice::render::ic_vert_2_dcol_dif ||
		    vd->vert_type == ice::render::ic_vert_nrmcol ||
		    vd->vert_type == ice::render::ic_vert_col)
			glDisableClientState(GL_COLOR_ARRAY);
		if (vd->vert_type != ice::render::ic_vert_2_dcol &&
		    vd->vert_type == ice::render::ic_vert_2_dcol_dif &&
		    vd->vert_type != ice::render::ic_vert_col)
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		return IC_OK;
	}
	return IC_FAIL_NO_DEVICE;
}

/*! Renders an indexed vertex buffer to the current frame
 *
 *  @param[in]    vb             Pointer to the vertex buffer
 *  @param[in]    ib             Pointer to the index buffer
 *  @returns      ICRESULT        Result of rendering the vert buffer
 **/
ice::result graphics_device::draw_indexed_vert_buf(class ice::render::vert_buffer *vb,
                                                   class ice::render::index_buf   *ib) {
#ifdef WIN32
	if (device_context_)
#else
	if (native_context_)
#endif
	{
		vert_buff_gl *gl_vb = (vert_buff_gl *)vb;
		index_buffer *gl_ib = (index_buffer *)ib;

		ice::render::vert_def *pvd = gl_vb->get_vert_def();

		glEnableClientState(GL_VERTEX_ARRAY);
		if (pvd->vert_type == ice::render::ic_vert_nrm_dif ||
		    pvd->vert_type == ice::render::ic_vert_nrmcol ||
		    pvd->vert_type == ice::render::ic_vert_nrmcolspc_dif) {
			glEnable(GL_LIGHTING);
			glEnable(GL_LIGHT0);
			glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SEPARATE_SPECULAR_COLOR);
			glEnableClientState(GL_NORMAL_ARRAY);
		}

		if (pvd->vert_type == ice::render::ic_vert_dif ||
		    pvd->vert_type == ice::render::ic_vert_nrm_dif) {
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		} else if (pvd->vert_type == ice::render::ic_vert_nrmcol) {
			glEnableClientState(GL_COLOR_ARRAY);
		}

		ice::render::index_def *id = ib->get_index_def();
		if (hardware_buffers_) {
			ice::uint pos_sz = (pvd->vert_type > ice::render::ic_vert_2_dcol_dif) ? 3 : 2;

			// bind the vertex buffer
			glBindBuffer(GL_ARRAY_BUFFER, gl_vb->get_name());

			glVertexPointer(pos_sz, GL_FLOAT, pvd->vert_size, NULL);

			if (pvd->vert_type == ice::render::ic_vert_nrm_dif ||
			    pvd->vert_type == ice::render::ic_vert_nrmcol ||
			    pvd->vert_type == ice::render::ic_vert_nrmcolspc_dif) {
				glNormalPointer(GL_FLOAT, pvd->vert_size,
				                (GLvoid *)(sizeof(ice::math::real) * pos_sz));
			}

			if (pvd->vert_type == ice::render::ic_vert_dif) {
				glTexCoordPointer(2, GL_FLOAT, pvd->vert_size,
				                  (GLvoid *)(sizeof(ice::math::real) * pos_sz));
			} else if (pvd->vert_type == ice::render::ic_vert_nrm_dif) {
				glTexCoordPointer(
				    2, GL_FLOAT, pvd->vert_size,
				    (GLvoid *)(sizeof(ice::math::vec3) + sizeof(ice::math::real) * pos_sz));
			} else if (pvd->vert_type == ice::render::ic_vert_nrmcol) {
				glColorPointer(
				    3, GL_UNSIGNED_BYTE, pvd->vert_size,
				    (GLvoid *)(sizeof(ice::math::vec3) + sizeof(ice::math::real) * pos_sz));
			}

			// bind the index buffer
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_ib->get_name());

			if (id->index_size == ice::render::ic_index_size_32)
				glDrawElements(GL_TRIANGLES, id->num_index, GL_UNSIGNED_INT, NULL);
			else
				glDrawElements(GL_TRIANGLES, id->num_index, GL_UNSIGNED_SHORT, NULL);

			glBindBuffer(GL_ARRAY_BUFFER, 0);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		} else {
			return IC_NOT_IMPL; // Client-memory indexed drawing is not implemented.
		}

		glDisableClientState(GL_VERTEX_ARRAY);
		if (pvd->vert_type == ice::render::ic_vert_nrm_dif ||
		    pvd->vert_type == ice::render::ic_vert_nrmcol ||
		    pvd->vert_type == ice::render::ic_vert_nrmcolspc_dif) {
			glDisable(GL_LIGHTING);
			glDisable(GL_LIGHT0);
			glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SINGLE_COLOR);
			glDisableClientState(GL_NORMAL_ARRAY);
		}

		if (pvd->vert_type == ice::render::ic_vert_dif ||
		    pvd->vert_type == ice::render::ic_vert_nrm_dif) {
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		} else if (pvd->vert_type == ice::render::ic_vert_nrmcol) {
			glDisableClientState(GL_COLOR_ARRAY);
		}
		return IC_OK;
	}
	return IC_FAIL_NO_DEVICE;
}

/*! Creates an openGL vertex buffer
 *
 *    @param[out]    out_vertex_buffer        Pointer to ouput VB pointer
 *    @param[in]    description    Information about the VB
 *    @returns    ICRESULT    Result of creating VB
 *
 **/
ice::result graphics_device::create_vb(class ice::render::vert_buffer **out_vertex_buffer,
                                       struct ice::render::vert_def    *description) {
#ifdef WIN32
	if (device_context_)
#else
	if (native_context_)
#endif
	{
		vert_buff_gl *vb = new vert_buff_gl();
		if (vb) {
			GLuint &buf_name = vb->get_name();
			GLenum  usage    = GL_STATIC_DRAW; //(pVertDef->usage == IC_VERT_USAGE_DYNAMIC)?
			                                   // GL_STREAM_DRAW : GL_STATIC_DRAW;

			if (hardware_buffers_) {
				glGenBuffers(1, &buf_name);
				glBindBuffer(GL_ARRAY_BUFFER, buf_name);
				glBufferData(GL_ARRAY_BUFFER, description->vert_size * description->num_verts, NULL,
				             usage);
			}

			*out_vertex_buffer = vb;
			memcpy(vb->get_vert_def(), description, sizeof(ice::render::vert_def));

			return IC_OK;
		}
		return IC_FAIL_OUT_OF_MEM;
	}
	return IC_FAIL_NO_DEVICE;
}

/*! Releases vertex buffer memory back to gpu
 *
 *  @param[in]    vb            Pointer to VB to release
 *  @returns    ICRESULT    Result of releasing VB
 **/
ice::result graphics_device::release_vb(class ice::render::vert_buffer *vb) {
	if (vb)
		delete vb;

	return IC_OK;
}

/*! Creates an openGl hardware index buffer
 *
 *    @param[out]   out_index_buffer        Pointer to ouput IB pointer
 *    @param[in]    description   Information about the IB
 *    @returns      ICRESULT    Result of creating IB
 *
 **/
ice::result graphics_device::create_ib(class ice::render::index_buf **out_index_buffer,
                                       struct ice::render::index_def *description) {
#ifdef WIN32
	if (device_context_)
#else
	if (native_context_)
#endif
	{
		index_buffer *ib = new index_buffer();
		if (ib) {
			GLuint &buf_name = ib->get_name();
			GLenum  usage    = GL_STATIC_DRAW; //(pIndexDef->usage == IC_INDEX_USAGE_DYNAMIC) ?
			                                   // GL_STREAM_DRAW : GL_STATIC_DRAW;

			GLsizeiptr size = (description->index_size == ice::render::ic_index_size_16)
			                      ? 2 * description->num_index
			                      : 4 * description->num_index;

			// TODO: FIX THIS
			if (hardware_buffers_) {
				glGenBuffers(1, &buf_name);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf_name);
				glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, NULL, usage);
			}

			*out_index_buffer = ib;
			memcpy(ib->get_index_def(), description, sizeof(ice::render::index_def));

			return IC_OK;
		}
		return IC_FAIL_OUT_OF_MEM;
	}
	return IC_FAIL_NO_DEVICE;
}

/*! Releases index buffer from memory
 *
 *  @param[in]    ib         Pointer to index buffer
 *  @returns      ICRESULT    Result of index buffer release
 **/
ice::result graphics_device::release_ib(class ice::render::index_buf *ib) {
	if (ib)
		delete ib;

	return IC_OK;
}

/*! Creates an openGL texture
 *
 *    @param[out]   out_texture       Pointer to ouput texture pointer
 *    @param[in]    description     Information about the texture
 *    @returns      ICRESULT    Result of creating texture
 *
 **/
ice::result graphics_device::create_tex(class ice::render::texture **out_texture,
                                        struct ice::render::tex_def *description) {
#ifdef WIN32
	if (device_context_)
#else
	if (native_context_)
#endif
	{
		texture *tex = new texture();
		if (tex) {
			GLuint &buf_name = tex->get_name();

			glGenTextures(1, &buf_name);
			glBindTexture(GL_TEXTURE_2D, buf_name); // needs exposed more

			*out_texture = tex;
			memcpy(tex->get_tex_def(), description, sizeof(ice::render::tex_def));

			return IC_OK;
		}
		return IC_FAIL_OUT_OF_MEM;
	}
	return IC_FAIL_NO_DEVICE;
}

/*! Creates an icTexture from a file
 *
 *    @param[in/out]    out_texture       Pointer to ouput texture pointer
 *    @param[in]        file      Name of file to load as texture
 *    @returns          ICRESULT    Result of texture creation
 *
 **/
ice::result graphics_device::create_tex_from_file(class ice::render::texture **out_texture,
                                                  const char                  *file) {
#ifdef WIN32
	if (device_context_)
#else
	if (native_context_)
#endif
	{
		texture *tex = new texture();
		if (tex) {
			GLuint &buf_name = tex->get_name();

			glGenTextures(1, &buf_name);
			glBindTexture(GL_TEXTURE_2D, buf_name); // needs exposed more

			// get the actual image bits
			ice::assets::image image_loader;
			if (ICEFAIL(image_loader.read_header(file))) {
				glDeleteTextures(1, &buf_name);
				delete tex;
				warningf("GXDevice could not read (%s) file header", file);
				return IC_FAIL_GEN;
			}

			// TODO: use PBOs so that we don't have to do this
			ice::uint size = 0;
			image_loader.get_image_data_size(&size);
			GLbyte *bits = (GLbyte *)malloc(size);
			if (!bits) {
				glDeleteTextures(1, &buf_name);
				delete tex;
				return IC_FAIL_OUT_OF_MEM;
			}
			image_loader.copy_image_bits(bits);

			ice::assets::gl_tex_info info;

			image_loader.get_gl_tex_info(&info);

			// Load texture, set filter and wrap modes
			glTexImage2D(GL_TEXTURE_2D, 0, info.comp, info.width, info.height, 0, info.format,
			             GL_UNSIGNED_BYTE, bits);
			gluBuild2DMipmaps(GL_TEXTURE_2D, info.comp, info.width, info.height, info.format,
			                  GL_UNSIGNED_BYTE, bits);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

			free(bits);

			*out_texture              = tex;
			ice::render::tex_def *def = tex->get_tex_def();

			switch (info.depth) {
			case 1:
				def->format = ICTEX_FMT_LUM8;
				break;
			// case 2:def->format = ICTEX_FMT_RBGA; break;
			case 3:
				def->format = ICTEX_FMT_RBG24;
				break;
			case 4:
				def->format = ICTEX_FMT_RBGA32;
				break;
			default:
				ice::error("nope");
			}

			def->width  = info.width;
			def->height = info.height;
			def->usage  = 0;

#ifdef _MSC_VER
			strcpy_s(tex->source_name, 256, file);
#else
			strcpy(tex->source_name, file);
#endif

			return IC_OK;
		}
		return IC_FAIL_OUT_OF_MEM;
	}
	return IC_FAIL_NO_DEVICE;
}

/*! Releases a texture from graphics memory
 *
 *    @param[in]    tex        Pointer to texture to release
 *    returns       ICRESULT    Result of texture release
 **/
ice::result graphics_device::release_tex(class ice::render::texture *tex) {
	if (!tex)
		return IC_OK;
	GLuint &glname = ((texture *)tex)->get_name();
	if (glname != 0xFFFFFFFF)
		glDeleteTextures(1, &glname);
	delete tex;
	return IC_OK;
}

ice::result graphics_device::create_font(class ice::render::font        **out_font,
                                         struct ice::render::font_params *parameters) {
	font_gl *font = new font_gl();
	if (ICEOK(font->initialize(parameters->font_name.c_str(), parameters->size, this))) {
		*out_font = font;
		return IC_OK;
	}
	delete font;
	return IC_FAIL_GEN;
}

ice::result graphics_device::release_font(class ice::render::font *font) {
	delete font;
	return IC_OK;
}

/*! Toggles depth test
 *
 *  @param      depth      Boolean, true - enable depth testing
 *  @returns    ICRESULT    Success/failure of depth test state change
 **/
ice::result graphics_device::enable_depth_test(bool depth) {
#ifdef WIN32
	if (device_context_)
#else
	if (native_context_)
#endif
	{
		depth_test_ = depth;
		if (depth_test_)
			glEnable(GL_DEPTH_TEST);
		else
			glDisable(GL_DEPTH_TEST);

		// TODO: expose this function
		glDepthFunc(GL_LESS);

		return IC_OK;
	}
	return IC_FAIL_NO_DEVICE;
}

/*! Toggles fog, on or off
 *
 *  @param      enable         Set - true to enable
 *  @param      params          Fog parameters
 *  @returns    ICRESULT        Success/failure of operation
 **/
ice::result graphics_device::enable_fog(bool enable, const ice::render::fog_params &params) {
	if (fog_enable_ == enable && params == fog_params_)
		return IC_OK;

	fog_enable_ = enable;

	const ice::math::color &c = params.color;
	GLfloat fog_color[]       = {c.red_float(), c.green_float(), c.blue_float(), c.alpha_float()};

	if (enable) {
		glEnable(GL_FOG);                 // Turn Fog on
		glFogfv(GL_FOG_COLOR, fog_color); // Set fog color to match background
		glFogf(GL_FOG_DENSITY, params.density);
		glFogf(GL_FOG_START, params.start); // How far away does the fog start
		glFogf(GL_FOG_END, params.end);     // How far away does the fog stop
		switch (params.type) {
		case 1:
			glFogi(GL_FOG_MODE, GL_LINEAR);
			break;
		case 2:
			glFogi(GL_FOG_MODE, GL_EXP);
			break;
		case 3:
			glFogi(GL_FOG_MODE, GL_EXP2);
			break;
		default:
			glFogi(GL_FOG_MODE, GL_LINEAR);
		}
	} else
		glDisable(GL_FOG);

	return IC_OK;
}

/*! Toggles alpha blending
 *
 *  @param      alpha_blend      Boolean, true - enable alpha blending
 *  @returns    ICRESULT    Success/failure of alpha blending state change
 **/
ice::result graphics_device::enable_alpha_blend(bool alpha_blend) {
	if (alpha_blend) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	} else {
		glDisable(GL_BLEND);
	}
	return IC_OK;
}

/*! Sets the ambient light color
 *
 *    @param        color         Desired ambient color
 *    @returns      ICRESULT    Result of ambient color change request
 **/
ice::result graphics_device::set_ambient_color(const ice::math::color &color) {
	col_ambient_ = color;
	return IC_FAIL_NO_DEVICE;
}

/*! Sets the ambient light color
 *
 *  @param        color         Desired clear color
 *  @returns      ICRESULT    Result of clear color change request
 **/
ice::result graphics_device::set_clear_color(const ice::math::color &color) {
	col_clear_ = color;
	return IC_OK;
}

/*! Sets the device triangle cull mode
 *
 *    @param[in]    mode     Tells the winding order for culling
 *    @returns      ICRESULT    Result of cull mode change request
 **/
ice::result graphics_device::set_cull_mode(ice::render::winding_cull_mode mode) {
	if (mode == ice::render::ic_cull_anticlock) {
		glEnable(GL_CULL_FACE);
		glFrontFace(GL_CCW);
	} else if (mode == ice::render::ic_cull_clockwise) {
		glEnable(GL_CULL_FACE);
		glFrontFace(GL_CW);
	} else if (mode == ice::render::ic_cull_off)
		glDisable(GL_CULL_FACE);
	else
		return IC_WARN_GEN; // invalid cull mode submitted
	return IC_OK;
}

/*! Sets the projection matrix
 *
 *    @param        projection        Projection matrix
 *    @result       ICRESULT    Result of projection change
 *
 **/
ice::result graphics_device::set_projection(const ice::math::mat4 &projection) {
	glMatrixMode(GL_PROJECTION);

	glLoadMatrixf(glm::value_ptr(projection));

	return IC_OK;
}

/*! Sets the view matrix
 *
 *    @param        view        Current view matrix
 *    @returns      ICRESULT    Result of the transform change
 *
 **/
ice::result graphics_device::set_view_matrix(const ice::math::mat4 &view) {
	glMatrixMode(GL_MODELVIEW);

	view_matrix_ = glm::inverse(view);

	// Load explicitly: callers need not manage the OpenGL matrix stack.
	glLoadMatrixf(glm::value_ptr(view_matrix_));

	return IC_OK;
}

/*! Sets the world transformation
 *
 *    @param        world       World rotation/position
 *    @returns      ICRESULT    Result of the transform change
 *
 **/
ice::result graphics_device::set_world_transform(const ice::math::mat4 &world) {
	glMatrixMode(GL_MODELVIEW);

	glLoadMatrixf(glm::value_ptr(view_matrix_));
	glMultMatrixf(glm::value_ptr(world));

	return IC_OK;
}

/*! Sets render texture by index
 *
 *  @param      index       Index of render texture to set
 *  @param      tex        Texture to use
 *  @returns    ICRESULT    Result of setting render texture
 **/
ice::result graphics_device::set_texture(ice::uint             index [[maybe_unused]],
                                         ice::render::texture *tex) {
#ifdef WIN32
	if (device_context_)
#else
	if (native_context_)
#endif
	{
		if (tex) {
			glEnable(GL_TEXTURE_2D);
			if (tex->has_alpha()) {
				glEnable(GL_BLEND);
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			} else {
				glDisable(GL_BLEND);
			}

			glBindTexture(GL_TEXTURE_2D, ((texture *)tex)->get_name());
		} else {
			glDisable(GL_BLEND);
			glBindTexture(GL_TEXTURE_2D, 0);
		}
		return IC_OK;
	}
	return IC_FAIL_NO_DEVICE;
}
#endif // #ifdef ICGL

} // namespace ice::render::gl
