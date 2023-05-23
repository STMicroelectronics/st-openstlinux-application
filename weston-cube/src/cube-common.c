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
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <gbm.h>

#include <wayland-client.h>
#include <wayland-egl.h>

#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "cube-common.h"
#include "shared/platform.h"
#include "shared/weston-egl-ext.h"

#define BUFFER_FORMAT DRM_FORMAT_XRGB8888

#ifndef ARRAY_LENGTH
#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])
#endif

int
init_egl(struct display *d, struct window *w)
{
	static const struct {
		char *extension, *entrypoint;
	} swap_damage_ext_to_entrypoint[] = {
		{
			.extension = "EGL_EXT_swap_buffers_with_damage",
			.entrypoint = "eglSwapBuffersWithDamageEXT",
		},
		{
			.extension = "EGL_KHR_swap_buffers_with_damage",
			.entrypoint = "eglSwapBuffersWithDamageKHR",
		},
	};

	static const EGLint context_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};
	const char *egl_extensions;
	const char *gl_extensions = NULL;

	EGLint config_attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 1,
		EGL_GREEN_SIZE, 1,
		EGL_BLUE_SIZE, 1,
		EGL_ALPHA_SIZE, 1,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};

	EGLint major, minor, n, count, i, size;
	EGLConfig *configs;
	EGLBoolean ret;

	if (w->opaque || w->buffer_size == 16)
		config_attribs[9] = 0;

	d->egl.dpy =
		weston_platform_get_egl_display(EGL_PLATFORM_WAYLAND_KHR,
						d->display, NULL);
	assert(d->egl.dpy);

	if (!eglInitialize(d->egl.dpy, &major, &minor)) {
		printf("failed to initialize\n");
		return -1;
	}

	if (!eglBindAPI(EGL_OPENGL_ES_API)) {
		printf("failed to bind api EGL_OPENGL_ES_API\n");
		return -1;
	}

	if (!eglGetConfigs(d->egl.dpy, NULL, 0, &count) || count < 1)
		assert(0);

	configs = calloc(count, sizeof *configs);
	assert(configs);

	ret = eglChooseConfig(d->egl.dpy, config_attribs,
			      configs, count, &n);
	assert(ret && n >= 1);

	for (i = 0; i < n; i++) {
		eglGetConfigAttrib(d->egl.dpy,
				   configs[i], EGL_BUFFER_SIZE, &size);
		if (w->buffer_size == size) {
			d->egl.conf = configs[i];
			break;
		}
	}
	free(configs);
	if (d->egl.conf == NULL) {
		fprintf(stderr, "did not find config with buffer size %d\n",
			w->buffer_size);
		exit(EXIT_FAILURE);
	}

	d->egl.ctx = eglCreateContext(d->egl.dpy,
				      d->egl.conf,
				      EGL_NO_CONTEXT,
				      context_attribs);
	assert(d->egl.ctx);

	egl_extensions = eglQueryString(d->egl.dpy, EGL_EXTENSIONS);
	assert(egl_extensions != NULL);

	if (!weston_check_egl_extension(egl_extensions,
                                        "EGL_EXT_image_dma_buf_import")) {
		fprintf(stderr, "EGL_EXT_image_dma_buf_import not supported\n");
		return -1;
	}

	d->egl.swap_buffers_with_damage = NULL;
	if (weston_check_egl_extension(egl_extensions, "EGL_EXT_buffer_age")) {
		for (i = 0;
		     i < (int) ARRAY_LENGTH(swap_damage_ext_to_entrypoint);
		     i++) {
			if (weston_check_egl_extension(egl_extensions,
				  swap_damage_ext_to_entrypoint[i].extension)) {
				/* The EXTPROC is identical to the KHR one */
				d->egl.swap_buffers_with_damage =
					(PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC)
					eglGetProcAddress(swap_damage_ext_to_entrypoint[i].entrypoint);
				break;
			}
		}
	}

	if (d->egl.swap_buffers_with_damage)
		printf("has EGL_EXT_buffer_age and %s\n",
		       swap_damage_ext_to_entrypoint[i].extension);

#define get_proc_gl(ext, name) do { \
		d->egl.name = (void *)eglGetProcAddress(#name); \
	} while (0)


	eglMakeCurrent(d->egl.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE,
		       d->egl.ctx);

	gl_extensions = (const char *) glGetString(GL_EXTENSIONS);
	if (!gl_extensions) {
		fprintf(stderr, "Retrieving GL extension string failed.\n");
		return -1;
	}

	if (!weston_check_egl_extension(gl_extensions,
					"GL_OES_EGL_image")) {
		fprintf(stderr, "GL_OES_EGL_image not suported\n");
		return -1;
	}
	get_proc_gl(GL_OES_EGL_image, glEGLImageTargetTexture2DOES);

	if (weston_check_egl_extension(egl_extensions,
				       "EGL_EXT_image_dma_buf_import_modifiers")
	    ) {
		d->egl.has_dma_buf_import_modifiers = true;
		d->egl.query_dma_buf_modifiers =
			(void *)eglGetProcAddress("eglQueryDmaBufModifiersEXT");
		assert(d->egl.query_dma_buf_modifiers);
	}

#define get_proc_dpy(ext, name) do { \
		if (weston_check_egl_extension(egl_extensions, #ext)) \
			d->egl.name = (void *)eglGetProcAddress(#name); \
	} while (0)
	get_proc_dpy(EGL_KHR_image_base, eglCreateImageKHR);
	get_proc_dpy(EGL_KHR_image_base, eglDestroyImageKHR);
	get_proc_dpy(EGL_KHR_fence_sync, eglCreateSyncKHR);
	get_proc_dpy(EGL_KHR_fence_sync, eglDestroySyncKHR);
	get_proc_dpy(EGL_KHR_fence_sync, eglWaitSyncKHR);
	get_proc_dpy(EGL_KHR_fence_sync, eglClientWaitSyncKHR);

	printf("Using display %p with EGL version %d.%d\n",
	       d->egl.dpy, major, minor);

	printf("===================================\n");
	printf("EGL information:\n");
	printf("  version: \"%s\"\n", eglQueryString(d->egl.dpy, EGL_VERSION));
	printf("  vendor: \"%s\"\n", eglQueryString(d->egl.dpy, EGL_VENDOR));
	printf("  display extensions: \"%s\"\n", egl_extensions);
	printf("===================================\n");
	printf("OpenGL ES 2.x information:\n");
	printf("  version: \"%s\"\n", glGetString(GL_VERSION));
	printf("  shading language version: \"%s\"\n",
	       glGetString(GL_SHADING_LANGUAGE_VERSION));
	printf("  vendor: \"%s\"\n", glGetString(GL_VENDOR));
	printf("  renderer: \"%s\"\n", glGetString(GL_RENDERER));
	printf("  extensions: \"%s\"\n", gl_extensions);
	printf("===================================\n");

	return 0;
}

void
fini_egl(struct display *d)
{
	eglTerminate(d->egl.dpy);
	eglReleaseThread();
}

int
init_supported_modifiers_for_egl(struct display *d)
{
	uint64_t *egl_modifiers = NULL;
	int num_egl_modifiers = 0;
	EGLBoolean ret;
	int i;

	/* If EGL doesn't support modifiers, don't use them at all. */
	if (!d->egl.has_dma_buf_import_modifiers) {
		d->modifiers_count = 0;
		free(d->modifiers);
		d->modifiers = NULL;
		return true;
	}

	ret = d->egl.query_dma_buf_modifiers(d->egl.dpy,
				             BUFFER_FORMAT,
				             0,    /* max_modifiers */
				             NULL, /* modifiers */
				             NULL, /* external_only */
				             &num_egl_modifiers);
	if (ret == EGL_FALSE || num_egl_modifiers == 0) {
		fprintf(stderr, "Failed to query num EGL modifiers for format: %i/%i\n", ret, num_egl_modifiers);
		goto error;
	}

	egl_modifiers = calloc(1, num_egl_modifiers * sizeof(*egl_modifiers));

	ret = d->egl.query_dma_buf_modifiers(d->egl.dpy,
					     BUFFER_FORMAT,
					     num_egl_modifiers,
					     egl_modifiers,
					     NULL, /* external_only */
					     &num_egl_modifiers);
	if (ret == EGL_FALSE) {
		fprintf(stderr, "Failed to query EGL modifiers for format\n");
		goto error;
	}

	/* Poor person's set intersection: d->modifiers INTERSECT
	 * egl_modifiers.  If a modifier is not supported, replace it with
	 * DRM_FORMAT_MOD_INVALID in the d->modifiers array.
	 */
	for (i = 0; i < d->modifiers_count; ++i) {
		uint64_t mod = d->modifiers[i];
		bool egl_supported = false;
		int j;

		for (j = 0; j < num_egl_modifiers; ++j) {
			if (egl_modifiers[j] == mod) {
				egl_supported = true;
				break;
			}
		}

		if (!egl_supported)
			d->modifiers[i] = DRM_FORMAT_MOD_INVALID;
	}

	free(egl_modifiers);

	return 0;

error:
	free(egl_modifiers);

	return -1;
}

int
int_gbm(struct display *d, char const* drm_render_node)
{
	d->gbm.drm_fd = open(drm_render_node, O_RDWR);
	if (d->gbm.drm_fd < 0) {
		fprintf(stderr, "Failed to open drm render node %s\n",
			drm_render_node);
		return -1;
	}

	d->gbm.dev = gbm_create_device(d->gbm.drm_fd);
	if (d->gbm.dev == NULL) {
		fprintf(stderr, "Failed to create gbm device\n");
		return -1;
	}

	return 0;
}

int create_program(const char *vs_src, const char *fs_src)
{
	GLuint vertex_shader, fragment_shader, program;
	GLint ret;

	vertex_shader = glCreateShader(GL_VERTEX_SHADER);

	glShaderSource(vertex_shader, 1, &vs_src, NULL);
	glCompileShader(vertex_shader);

	glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &ret);
	if (!ret) {
		char *log;

		printf("vertex shader compilation failed!:\n");
		glGetShaderiv(vertex_shader, GL_INFO_LOG_LENGTH, &ret);
		if (ret > 1) {
			log = malloc(ret);
			glGetShaderInfoLog(vertex_shader, ret, NULL, log);
			printf("%s", log);
		}

		return -1;
	}

	fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

	glShaderSource(fragment_shader, 1, &fs_src, NULL);
	glCompileShader(fragment_shader);

	glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &ret);
	if (!ret) {
		char *log;

		printf("fragment shader compilation failed!:\n");
		glGetShaderiv(fragment_shader, GL_INFO_LOG_LENGTH, &ret);

		if (ret > 1) {
			log = malloc(ret);
			glGetShaderInfoLog(fragment_shader, ret, NULL, log);
			printf("%s", log);
		}

		return -1;
	}

	program = glCreateProgram();

	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);

	return program;
}

int link_program(unsigned program)
{
	GLint ret;

	glLinkProgram(program);

	glGetProgramiv(program, GL_LINK_STATUS, &ret);
	if (!ret) {
		char *log;

		printf("program linking failed!:\n");
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &ret);

		if (ret > 1) {
			log = malloc(ret);
			glGetProgramInfoLog(program, ret, NULL, log);
			printf("%s", log);
		}

		return -1;
	}

	return 0;
}

void compute_pitch(struct window *w) {
	ESVec3  origin = {  { 0.0f, 0.0f,  0.0f } };
	ESMatrix4x4 modelview, projection, modelviewprojection;
	ESVec4 Clipped;
	ESVec3 NDC;
	ESVec2 win0x0, win1x1;
	float aspect;

	aspect = (GLfloat)(w->geometry.height) / (GLfloat)(w->geometry.width);

	esMatrixLoadIdentity(&projection);
	esFrustum(&projection,
		  FRUSTRUM_LEFT,             FRUSTRUM_RIGTH,
		  FRUSTRUM_BOTTOM * aspect,  FRUSTRUM_TOP * aspect,
		  FRUSTRUM_NEAR_Z,           FRUSTRUM_FAR_Z);

	esMatrixLoadIdentity(&modelview);
	esTranslate(&modelview,0.0f, 0.0f, -Z_TRANSLATION);

	esMatrixLoadIdentity(&modelviewprojection);
	esMatrixMultiply(&modelviewprojection, &modelview, &projection);

	esMatrixClipped(&Clipped, &modelviewprojection, &origin);
	esMatrixNDC(&NDC, &Clipped);
	esMatrixWindow(&win0x0, &NDC, w->geometry.width, w->geometry.height);

	esTranslate(&modelview, 1.0f, 1.0f, 0.0f);
	esMatrixMultiply(&modelviewprojection, &modelview, &projection);
	esMatrixClipped(&Clipped, &modelviewprojection, &origin);
	esMatrixNDC(&NDC, &Clipped);
	esMatrixWindow(&win1x1, &NDC, w->geometry.width, w->geometry.height);

	w->pitch.x = win1x1.vec2[0] - win0x0.vec2[0];
	w->pitch.y = win1x1.vec2[1] - win0x0.vec2[1];

	printf("x pitch : %f\nx pitch : %f\n", w->pitch.x, w->pitch.y);

#ifdef DAMAGE_DEBUG
	w->corner.x = w->geometry.width  * DAMAGE_X_PERCENT;
	w->corner.y = w->geometry.height * DAMAGE_Y_PERCENT;
	w->rect.x   = w->geometry.width  * DAMAGE_W_PERCENT;
	w->rect.y   = w->geometry.height * DAMAGE_H_PERCENT;
#endif
}

#ifdef HAVE_GST
void cube_next_shader(struct window *w) {
	if (w->next_shader)
		w->next_shader((void*)w);
}
#endif
