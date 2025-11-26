/*
 * Copyright © 2011 Benjamin Franzke
 * Copyright (c) 2019 STMicroelectronics. All rights reserved.
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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
//#include <string.h>
#include <stdbool.h>
//#include <math.h>
#include <assert.h>
#include <signal.h>
#include <getopt.h>

#include <linux/input.h>

#include <wayland-client.h>
#include <wayland-egl.h>
#include <wayland-cursor.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


//#include <GLES2/gl2.h>
#include <EGL/egl.h>
//#include <EGL/eglext.h>
#include <gbm.h>

#include "xdg-shell-client-protocol.h"
#include "linux-dmabuf-unstable-v1-client-protocol.h"
#include <unistd.h>

#ifdef HAVE_GST
#include <gst/gst.h>
GST_DEBUG_CATEGORY(cube_video_debug);
#endif

#include "simple-st-egl.h"
#include "cube-common.h"
#include "shared/platform.h"

#define DRM_RENDER_NODE "/dev/dri/card0"

#define CUBE_VERSION "20200908"

static int running = 1;

static void
handle_surface_configure(void *data, struct xdg_surface *surface,
			 uint32_t serial)
{
	struct window *window = data;

	xdg_surface_ack_configure(surface, serial);

	window->wait_for_configure = false;
}

static const struct xdg_surface_listener xdg_surface_listener = {
	handle_surface_configure
};

static void
handle_toplevel_configure(void *data, struct xdg_toplevel *toplevel,
			  int32_t width, int32_t height,
			  struct wl_array *states)
{
	struct window *window = data;
	uint32_t *p;

	window->fullscreen = 0;
	wl_array_for_each(p, states) {
		uint32_t state = *p;
		switch (state) {
		case XDG_TOPLEVEL_STATE_FULLSCREEN:
		case XDG_TOPLEVEL_STATE_MAXIMIZED:
			window->fullscreen = 1;
			break;
		}
	}

	if (width > 0 && height > 0) {
		if (!window->fullscreen) {
			window->window_size.width = width;
			window->window_size.height = height;
		}
		window->geometry.width = width;
		window->geometry.height = height;
	} else if (!window->fullscreen) {
		window->geometry = window->window_size;
	}

	if (window->native)
		wl_egl_window_resize(window->native,
				     window->geometry.width,
				     window->geometry.height, 0, 0);

	printf("handle_toplevel_configure(%p): WxH={%ix%i} - Fullscreen: %s\n",
		   window->native,
		   window->geometry.width, window->geometry.height,
		   (window->fullscreen?"Yes":"No"));

	compute_pitch(window);
}

static void
handle_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel)
{
	running = 0;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
	handle_toplevel_configure,
	handle_toplevel_close,
};

static void
create_xdg_surface(struct window *window, struct display *display)
{
	window->xdg_surface = xdg_wm_base_get_xdg_surface(display->shell,
							    window->surface);
	xdg_surface_add_listener(window->xdg_surface,
				     &xdg_surface_listener, window);

	window->xdg_toplevel =
		xdg_surface_get_toplevel(window->xdg_surface);
	xdg_toplevel_add_listener(window->xdg_toplevel,
				      &xdg_toplevel_listener, window);

	xdg_toplevel_set_title(window->xdg_toplevel, "simple-st-egl");


	window->wait_for_configure = true;
	wl_surface_commit(window->surface);
}

static void
create_surface(struct window *window)
{
	struct display *display = window->display;
	EGLBoolean ret;

	window->surface = wl_compositor_create_surface(display->compositor);

	if (display->shell) {
		create_xdg_surface(window, display);
	} else {
		assert(0);
	}

	window->native =
		wl_egl_window_create(window->surface,
				     window->geometry.width,
				     window->geometry.height);
	window->egl_surface =
		weston_platform_create_egl_surface(display->egl.dpy,
						   display->egl.conf,
						   window->native, NULL);


	ret = eglMakeCurrent(window->display->egl.dpy, window->egl_surface,
			     window->egl_surface, window->display->egl.ctx);
	assert(ret == EGL_TRUE);

	if (!window->frame_sync)
		eglSwapInterval(display->egl.dpy, 0);

	if (!display->shell)
		return;

	if (window->fullscreen)
		xdg_toplevel_set_maximized(window->xdg_toplevel);
		//xdg_toplevel_set_fullscreen(window->xdg_toplevel, NULL);
}

static void
destroy_surface(struct window *window)
{
	/* Required, otherwise segfault in egl_dri2.c: dri2_make_current()
	 * on eglReleaseThread(). */
	eglMakeCurrent(window->display->egl.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE,
		       EGL_NO_CONTEXT);

	weston_platform_destroy_egl_surface(window->display->egl.dpy,
					    window->egl_surface);
	wl_egl_window_destroy(window->native);

	if (window->xdg_toplevel)
		xdg_toplevel_destroy(window->xdg_toplevel);
	if (window->xdg_surface)
		xdg_surface_destroy(window->xdg_surface);
	wl_surface_destroy(window->surface);

	if (window->callback)
		wl_callback_destroy(window->callback);
}

static void
pointer_handle_enter(void *data, struct wl_pointer *pointer,
		     uint32_t serial, struct wl_surface *surface,
		     wl_fixed_t sx, wl_fixed_t sy)
{
	struct display *d = data;
	struct wl_buffer *buffer;
	struct wl_cursor *cursor = d->default_cursor;
	struct wl_cursor_image *image;

	//if (d->window->fullscreen)
	//	wl_pointer_set_cursor(pointer, serial, NULL, 0, 0);
	//else if (cursor) {
	if (cursor) {
		image = d->default_cursor->images[0];
		buffer = wl_cursor_image_get_buffer(image);
		if (!buffer)
			return;
		wl_pointer_set_cursor(pointer, serial,
				      d->cursor_surface,
				      image->hotspot_x,
				      image->hotspot_y);
		wl_surface_attach(d->cursor_surface, buffer, 0, 0);
		wl_surface_damage(d->cursor_surface, 0, 0,
				  image->width, image->height);
		wl_surface_commit(d->cursor_surface);
	}
}

static void
pointer_handle_leave(void *data, struct wl_pointer *pointer,
		     uint32_t serial, struct wl_surface *surface)
{
}

static void
pointer_handle_motion(void *data, struct wl_pointer *pointer,
		      uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
	struct display *d = (struct display *)data;
	struct window *w = d->window;

	if (w->fullscreen) {
                if (w->button_pressed) {
			w->move.x = wl_fixed_to_double(sx) - w->enter.x;
			w->move.y = wl_fixed_to_double(sy) - w->enter.y;
		} else {
			w->enter.x = wl_fixed_to_double(sx) - w->move.x;
			w->enter.y = wl_fixed_to_double(sy) - w->move.y;
		}
	}

/*        printf("MOTION(%s): time: %i - x_w: %g - y_w: %g - move_x:%g - move_y: %g\n",*/
/*               (w->fullscreen ? "Yes" : "No"), time,*/
/*               wl_fixed_to_double(sx), wl_fixed_to_double(sy),*/
/*               w->move.x, w->move.y);*/
}

static void
pointer_handle_button(void *data, struct wl_pointer *wl_pointer,
		      uint32_t serial, uint32_t time, uint32_t button,
		      uint32_t state)
{
	struct display *d = data;
	struct window *w = d->window;

	if (!w->xdg_toplevel)
		return;

	if (button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_PRESSED){
		if ((time - w->time_enter) < 300)
			running = 0;
		w->time_enter = time;

		if (w->fullscreen)
			w->button_pressed = true;
		else
			xdg_toplevel_move(w->xdg_toplevel,
					      d->seat, serial);
	} else {
		w->button_pressed = false;
	}
	if (button == BTN_RIGHT && state == WL_POINTER_BUTTON_STATE_PRESSED)
		cube_next_shader(w);
}

static void
pointer_handle_axis(void *data, struct wl_pointer *wl_pointer,
		    uint32_t time, uint32_t axis, wl_fixed_t value)
{
}

static const struct wl_pointer_listener pointer_listener = {
	pointer_handle_enter,
	pointer_handle_leave,
	pointer_handle_motion,
	pointer_handle_button,
	pointer_handle_axis,
};

static void
touch_handle_down(void *data, struct wl_touch *wl_touch,
		  uint32_t serial, uint32_t time, struct wl_surface *surface,
		  int32_t id, wl_fixed_t x_w, wl_fixed_t y_w)
{
	struct display *d = (struct display *)data;
	struct window *w = d->window;

	if (!d->shell)
		return;

	if ((time - w->time_enter) < 300)
		running = 0;
	w->time_enter = time;

	if (w->fullscreen) {
		w->enter.x = wl_fixed_to_double(x_w) - w->move.x;
		w->enter.y = wl_fixed_to_double(y_w) - w->move.y;

		//printf("DOWN: time: %i - enter_x: %g - enter_y: %g\n", time,
		//       wl_fixed_to_double(x_w), wl_fixed_to_double(y_w));
	} else {
		xdg_toplevel_move(d->window->xdg_toplevel, d->seat, serial);
	}
}

static void
touch_handle_up(void *data, struct wl_touch *wl_touch,
		uint32_t serial, uint32_t time, int32_t id)
{
	struct display *d = (struct display *)data;

	if (!d->shell)
		return;

}

static void
touch_handle_motion(void *data, struct wl_touch *wl_touch,
		    uint32_t time, int32_t id, wl_fixed_t x_w, wl_fixed_t y_w)
{
	struct display *d = (struct display *)data;
	struct window *w = d->window;

	if (w->fullscreen) {
		w->move.x = wl_fixed_to_double(x_w) - w->enter.x;
		w->move.y = wl_fixed_to_double(y_w) - w->enter.y;

/*                printf("MOTION: t: %i - x_w: %g - y_w: %g - m_x:%g - m_y: %g\n",*/
/*                       time,*/
/*                       wl_fixed_to_double(x_w), wl_fixed_to_double(y_w),*/
/*                       w->move.x, w->move.y);*/
	}
}

static void
touch_handle_frame(void *data, struct wl_touch *wl_touch)
{
}

static void
touch_handle_cancel(void *data, struct wl_touch *wl_touch)
{
}

static const struct wl_touch_listener touch_listener = {
	touch_handle_down,
	touch_handle_up,
	touch_handle_motion,
	touch_handle_frame,
	touch_handle_cancel,
};

static void
keyboard_handle_keymap(void *data, struct wl_keyboard *keyboard,
		       uint32_t format, int fd, uint32_t size)
{
}

static void
keyboard_handle_enter(void *data, struct wl_keyboard *keyboard,
		      uint32_t serial, struct wl_surface *surface,
		      struct wl_array *keys)
{
}

static void
keyboard_handle_leave(void *data, struct wl_keyboard *keyboard,
		      uint32_t serial, struct wl_surface *surface)
{
}

static void
keyboard_handle_key(void *data, struct wl_keyboard *keyboard,
		    uint32_t serial, uint32_t time, uint32_t key,
		    uint32_t state)
{
	struct display *d = data;

	if (!d->shell)
		return;

	if (key == KEY_F11 && state) {
		if (d->window->fullscreen)
			xdg_toplevel_unset_maximized(d->window->xdg_toplevel);
			//xdg_toplevel_unset_fullscreen(d->window->xdg_toplevel);
		else
			xdg_toplevel_set_maximized(d->window->xdg_toplevel);
			//xdg_toplevel_set_fullscreen(d->window->xdg_toplevel,
			//				NULL);
	} else if (key == KEY_ESC && state)
		running = 0;

#ifdef DAMAGE_DEBUG
	struct window *w = d->window;

	if (key == KEY_X && state) {
		w->update = &w->corner.x;
	} else if (key == KEY_Y && state) {
		w->update = &w->corner.y;
	} else if (key == KEY_W && state) {
		w->update = &w->rect.x;
	} else if (key == KEY_H && state) {
		w->update = &w->rect.y;
	} else if (key == KEY_UP && state) {
		*(w->update) += 1;
	} else if (key == KEY_DOWN && state) {
		*(w->update) -= 1;
	} else if (key == KEY_PAGEUP && state) {
		*(w->update) += 10;
	} else if (key == KEY_PAGEDOWN && state) {
		*(w->update) -= 10;
	}

	printf("{%ix%i}-[%ix%i]\n",
	       (EGLint)w->corner.x, (EGLint)w->corner.y,
	       (EGLint)w->rect.x,     (EGLint)w->rect.y);
	printf("{%6.2fx%6.2f}-[%6.2fx%6.2f]\n",
		   w->corner.x / w->geometry.width,
		   w->corner.y / w->geometry.height,
		   w->rect.x   / w->geometry.width,
		   w->rect.y   / w->geometry.height);
#endif
}

static void
keyboard_handle_modifiers(void *data, struct wl_keyboard *keyboard,
			  uint32_t serial, uint32_t mods_depressed,
			  uint32_t mods_latched, uint32_t mods_locked,
			  uint32_t group)
{
}

static const struct wl_keyboard_listener keyboard_listener = {
	keyboard_handle_keymap,
	keyboard_handle_enter,
	keyboard_handle_leave,
	keyboard_handle_key,
	keyboard_handle_modifiers,
};

static void
seat_handle_capabilities(void *data, struct wl_seat *seat,
			 enum wl_seat_capability caps)
{
	struct display *d = data;

	if ((caps & WL_SEAT_CAPABILITY_POINTER) && !d->pointer) {
		d->pointer = wl_seat_get_pointer(seat);
		wl_pointer_add_listener(d->pointer, &pointer_listener, d);
	} else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && d->pointer) {
		wl_pointer_destroy(d->pointer);
		d->pointer = NULL;
	}

	if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !d->keyboard) {
		d->keyboard = wl_seat_get_keyboard(seat);
		wl_keyboard_add_listener(d->keyboard, &keyboard_listener, d);
	} else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && d->keyboard) {
		wl_keyboard_destroy(d->keyboard);
		d->keyboard = NULL;
	}

	if ((caps & WL_SEAT_CAPABILITY_TOUCH) && !d->touch) {
		d->touch = wl_seat_get_touch(seat);
		wl_touch_set_user_data(d->touch, d);
		wl_touch_add_listener(d->touch, &touch_listener, d);
	} else if (!(caps & WL_SEAT_CAPABILITY_TOUCH) && d->touch) {
		wl_touch_destroy(d->touch);
		d->touch = NULL;
	}
}

static const struct wl_seat_listener seat_listener = {
	seat_handle_capabilities,
};

static void
xdg_wm_base_ping(void *data, struct xdg_wm_base *shell, uint32_t serial)
{
	xdg_wm_base_pong(shell, serial);
}

static const struct xdg_wm_base_listener wm_base_listener = {
	xdg_wm_base_ping,
};

static void
dmabuf_modifiers(void *data, struct zwp_linux_dmabuf_v1 *zwp_linux_dmabuf,
		 uint32_t format, uint32_t modifier_hi, uint32_t modifier_lo)
{
	struct display *d = data;

	switch (format) {
	case BUFFER_FORMAT:
		++d->modifiers_count;
		d->modifiers = realloc(d->modifiers,
				       d->modifiers_count *
				       sizeof(*d->modifiers));
		d->modifiers[d->modifiers_count - 1] =
			((uint64_t)modifier_hi << 32) | modifier_lo;
		break;
	default:
		break;
	}
}

static void
dmabuf_format(void *data, struct zwp_linux_dmabuf_v1 *zwp_linux_dmabuf,
	      uint32_t format)
{
	/* XXX: deprecated */
}
static const struct zwp_linux_dmabuf_v1_listener dmabuf_listener = {
	dmabuf_format,
	dmabuf_modifiers
};

static void
registry_handle_global(void *data, struct wl_registry *registry,
		       uint32_t name, const char *interface, uint32_t version)
{
	struct display *d = data;

	if (strcmp(interface, "wl_compositor") == 0) {
		d->compositor =
			wl_registry_bind(registry, name,
					 &wl_compositor_interface,
					 MIN(version, 4));
	} else if (strcmp(interface, "xdg_wm_base") == 0) {
		d->shell = wl_registry_bind(registry, name,
					      &xdg_wm_base_interface, 1);
		xdg_wm_base_add_listener(d->shell, &wm_base_listener, d);
	} else if (strcmp(interface, "wl_seat") == 0) {
		d->seat = wl_registry_bind(registry, name,
					   &wl_seat_interface, 1);
		wl_seat_add_listener(d->seat, &seat_listener, d);
	} else if (strcmp(interface, "wl_shm") == 0) {
		d->shm = wl_registry_bind(registry, name,
					  &wl_shm_interface, 1);
		d->cursor_theme = wl_cursor_theme_load(NULL, 32, d->shm);
		if (!d->cursor_theme) {
			fprintf(stderr, "unable to load default theme\n");
			return;
		}
		d->default_cursor =
			wl_cursor_theme_get_cursor(d->cursor_theme, "left_ptr");
		if (!d->default_cursor) {
			fprintf(stderr, "unable to load default left pointer\n");
			// TODO: abort ?
		}
	} else if (strcmp(interface, "zwp_linux_dmabuf_v1") == 0) {
		if (version < 3)
			return;
		d->dmabuf = wl_registry_bind(registry, name,
					     &zwp_linux_dmabuf_v1_interface, 3);
		zwp_linux_dmabuf_v1_add_listener(d->dmabuf, &dmabuf_listener,
						 d);
	}
}

static void
registry_handle_global_remove(void *data, struct wl_registry *registry,
			      uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
	registry_handle_global,
	registry_handle_global_remove
};

static void
destroy_display(struct display *d)
{
	if (d->gbm.dev)
		gbm_device_destroy(d->gbm.dev);

	if (d->gbm.drm_fd >= 0)
		close(d->gbm.drm_fd);

	if (d->egl.ctx != EGL_NO_CONTEXT)
		eglDestroyContext(d->egl.dpy, d->egl.ctx);

	if (d->egl.dpy != EGL_NO_DISPLAY)
		fini_egl(d);

	free(d->modifiers);

	if (d->dmabuf)
		zwp_linux_dmabuf_v1_destroy(d->dmabuf);

	if (d->shell)
		xdg_wm_base_destroy(d->shell);

	if (d->cursor_theme)
		wl_cursor_theme_destroy(d->cursor_theme);

	if (d->compositor)
		wl_compositor_destroy(d->compositor);

	if (d->registry)
		wl_registry_destroy(d->registry);

	if (d->display) {
		wl_display_flush(d->display);
		wl_display_disconnect(d->display);
	}

	free(d);
}

static struct display *
create_display(char const *drm_render_node, struct window *w)
{
	struct display *d = NULL;

	d = calloc(1, sizeof *d);
	if (d == NULL) {
		fprintf(stderr, "out of memory\n");
		goto error;
	}

	d->gbm.drm_fd = -1;

	d->display = wl_display_connect(NULL);
	assert(d->display);

	d->registry = wl_display_get_registry(d->display);
	wl_registry_add_listener(d->registry,
				 &registry_listener, d);
	wl_display_roundtrip(d->display);
	if (d->dmabuf == NULL) {
		fprintf(stderr, "No zwp_linux_dmabuf global\n");
		goto error;
	}

	wl_display_roundtrip(d->display);

	if (!d->modifiers_count) {
		fprintf(stderr, "format XRGB8888 is not available\n");
		goto error;
	}

	if (init_egl(d, w))
		goto error;

	if (int_gbm(d, drm_render_node))
		goto error;

	return d;

error:
	if (d != NULL)
		destroy_display(d);
	return NULL;
}




static void
signal_int(int signum)
{
	running = 0;
}

static const char *shortopts = "fod:si:1:3:6:v:c:bh";

static const struct option longopts[] = {
	{"fullscreen",    no_argument,       0, 'f'},
	{"opaque",        no_argument,       0, 'o'},
	{"device",        required_argument, 0, 'd'},
	{"egl-buf-size",  no_argument,       0, 's'},
	{"swap-interval", required_argument, 0, 'i'},
	{"tex1",          required_argument, 0, '1'},
	{"tex3",          required_argument, 0, '3'},
	{"tex6",          required_argument, 0, '6'},
	{"video",         required_argument, 0, 'v'},
	{"animated",      no_argument,       0, 'a'},
	{"cam-fps",       required_argument, 0, 'c'},
	{"background",    no_argument,       0, 'b'},
	{"help",          no_argument,       0, 'h'},
	{0, 0, 0, 0}
};

static void
usage(int error_code)
{
	fprintf(stderr, "Usage: simple-st-egl-cube-tex [fosba136vch]\n"
			"\n"
			"options:\n"
			"  -f, --fullscreen          Run in fullscreen mode\n"
			"  -o, --opaque              Create an opaque surface\n"
			"  -d, --device=DEVICE       Use the given device\n"
			"                            (\"DRM_RENDER_NODE\")\n"
			"  -s, --egl-buf-size        Use a 16 bpp EGL config\n"
			"                            (Buffer Size 32)\n"
			"  -i, --swap-interval       Don't sync to compositor redraw\n"
			"                            (eglSwapInterval 0)\n"
			"  -1, --tex1=TEXFILE        Add a texture file on all cube faces\n"
			"  -3, --tex3=T1 T2 T3       Add a 3 texture file on all cube faces 2 by 2\n"
			"  -6, --tex6=TEXFIL         Add a texture per cube face.\n"
			"                            This should be a 3x2 tex\n"
			"  -v, --video=FILE          Video textured cube\n"
			"  -a, --animated            GPU shader applied on textured cube faced.\n"
			"                            (WARNING: High GPU consumption)\n"
			"  -c, --cam-fps=(fraction)  Only for camera. Specify Camera fps output\n"
			"                            (i.e 15/1 default)\n"
			"  -b, --background          Video frams as background\n"
			"  -h, --help                This help text\n\n");

	exit(error_code);
}

int
main(int argc, char **argv)
{
	struct sigaction sigint;
	struct display *display;
	struct window  window  = { 0 };
	char *drm_render_node = DRM_RENDER_NODE;
	int i, ret = 0, opt;
	const char *video = NULL;

#ifdef HAVE_GST
	gst_init(&argc, &argv);
	GST_DEBUG_CATEGORY_INIT(cube_video_debug, "cube", 0,
				"cube video pipeline");
#endif
	window.geometry.width  = 480;
	window.geometry.height = 480;
	window.window_size = window.geometry;
	window.buffer_size = 32;
	window.frame_sync = 1;
	window.mod = SMOOTH;
	window.background = 0;
	window.cam_fps = strdup("15/1");
	window.animated = false;

	fprintf(stderr, "Version: simple-st-egl-cube-tex \"%s\"\n",
		CUBE_VERSION);

	while ((opt = getopt_long_only(argc, argv, shortopts, longopts, NULL))
	       != -1) {
		switch (opt) {

		case 'f':
			window.fullscreen = 1;
			break;
		case 'o':
			window.opaque = 1;
			break;
		case 'd':
			drm_render_node = strdup(optarg);
			break;
		case 's':
			window.buffer_size = 16;
			break;
		case 'b':
			window.background = 1;
			break;
		case 'i':
			window.frame_sync = 0;
			break;
		case 'h':
			usage(EXIT_SUCCESS);
			break;
		case '1':
			window.tex_filename[0] = strdup(optarg);
			window.mod = ONE_TEX;
			break;
		case '3':
				i = optind - 1;
				window.tex_filename[0] = strdup(argv[i++]);
				window.tex_filename[1] = strdup(argv[i++]);
				window.tex_filename[2] = strdup(argv[i++]);
				window.mod = THREE_TEX;
				optind = i - 1;
				break;
		case '6':
				window.tex_filename[0] = strdup(optarg);
				window.mod = ONE_MAP_TEX;
				break;
		case 'v':
				video = optarg;
				window.mod = VIDEO;
				break;
		case 'c':
				free(window.cam_fps);
				window.cam_fps = strdup(optarg);
				break;
		case 'a':
				window.animated = true;
				break;
		default:
				usage(EXIT_FAILURE);
				break;
		}
	}

	display = create_display(drm_render_node, &window);
	if (!display)
		return 1;
	window.display = display;
	display->window = &window;

	create_surface(&window);
	if (window.mod == SMOOTH)
		init_cube_smooth(&window);
	else if (window.mod == VIDEO)
		init_cube_video(&window, video);
	else
		init_cube_tex(&window);

	display->cursor_surface =
		wl_compositor_create_surface(display->compositor);

	sigint.sa_handler = signal_int;
	sigemptyset(&sigint.sa_mask);
	sigint.sa_flags = SA_RESETHAND;
	sigaction(SIGINT, &sigint, NULL);

	/* The mainloop here is a little subtle.  Redrawing will cause
	 * EGL to read events so we can just call
	 * wl_display_dispatch_pending() to handle any events that got
	 * queued up as a side effect. */
	while (running && ret != -1) {
		if (window.wait_for_configure) {
			wl_display_dispatch(display->display);
		} else {
			wl_display_dispatch_pending(display->display);
			window.redraw(&window, NULL);
		}
	}

	fprintf(stderr, "simple-egl exiting\n");

	wl_surface_destroy(display->cursor_surface);
	destroy_surface(&window);
	destroy_display(display);

	return 0;
}
