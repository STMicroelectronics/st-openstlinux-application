/*
 * Copyright © 2011 Benjamin Franzke
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef SIMPLE_ST_EGL_H
#define SIMPLE_ST_EGL_H

struct window;
struct seat;

#include <stdbool.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <drm_fourcc.h>

#define BUFFER_FORMAT DRM_FORMAT_XRGB8888

enum mode {
	SMOOTH,
	ONE_TEX,
	THREE_TEX,
	ONE_MAP_TEX,
	VIDEO
};

struct _egl {
	EGLDisplay dpy;
	EGLContext ctx;
	EGLConfig conf;

	bool has_dma_buf_import_modifiers;

	PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC swap_buffers_with_damage;
	PFNEGLQUERYDMABUFMODIFIERSEXTPROC query_dma_buf_modifiers;
	PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR;
	PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR;
	PFNEGLCREATESYNCKHRPROC eglCreateSyncKHR;
	PFNEGLWAITSYNCKHRPROC eglWaitSyncKHR;
	PFNEGLDESTROYSYNCKHRPROC eglDestroySyncKHR;
	PFNEGLCLIENTWAITSYNCKHRPROC eglClientWaitSyncKHR;
	PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;

};
struct _gbm {
	int drm_fd;
	struct gbm_device *dev;
};

struct display {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct xdg_wm_base *shell;
	struct zwp_linux_dmabuf_v1 *dmabuf;
	struct wl_seat *seat;
	struct wl_pointer *pointer;
	struct wl_touch *touch;
	struct wl_keyboard *keyboard;
	struct wl_shm *shm;
	struct wl_cursor_theme *cursor_theme;
	struct wl_cursor *default_cursor;
	struct wl_surface *cursor_surface;

	uint64_t *modifiers;
	int modifiers_count;

	struct _egl egl;
	struct _gbm gbm;
	struct window *window;
};

struct point {
	GLfloat x;
	GLfloat y;
};

struct geometry {
	int width, height;
};

struct gl;

struct window {
	struct display *display;
	struct geometry geometry, window_size;
	struct gl *gl;
	uint32_t benchmark_time, frames, frames_cumul;
	struct wl_egl_window *native;
	struct wl_surface *surface;
	struct xdg_surface *xdg_surface;
	struct xdg_toplevel *xdg_toplevel;
	EGLSurface egl_surface;
	struct wl_callback *callback;
	int fullscreen, opaque, buffer_size, frame_sync, background;
	char * cam_fps;
	char * tex_filename[3];
	bool wait_for_configure;
	bool animated;
	struct point move, enter;
	struct point pitch;
	EGLint damage[4];
	GLuint time_enter;
	bool button_pressed;
	enum mode mod;
	void (*redraw)(void *data, struct wl_callback *callback);
	void (*next_shader)(void *data);
#ifdef DAMAGE_DEBUG
	GLfloat *update;
	struct point corner, rect;
#endif
};

#endif
