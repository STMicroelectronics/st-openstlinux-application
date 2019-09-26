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

#ifndef CUBE_COMMON_H
#define CUBE_COMMON_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "simple-st-egl.h"
#include "esUtil.h"

#define Z_TRANSLATION 9.0f

#define FRUSTRUM_LEFT     -3.0f
#define FRUSTRUM_RIGTH    -FRUSTRUM_LEFT
#define FRUSTRUM_BOTTOM   -3.0f
#define FRUSTRUM_TOP      -FRUSTRUM_BOTTOM
#define FRUSTRUM_NEAR_Z    5.0f
#define FRUSTRUM_FAR_Z     10.0f

/* pre-computed value : worse case cube */
#define DAMAGE_X_PERCENT   (32.0f / 100.0f)
#define DAMAGE_Y_PERCENT   (19.0f / 100.0f)
#define DAMAGE_W_PERCENT   (36.0f / 100.0f)
#define DAMAGE_H_PERCENT   (62.0f / 100.0f)

#define CUBE_STR_HELPER(x) #x
#define CUBE_STR(x) CUBE_STR_HELPER(x)

#define CUBE_RED		0.31
#define CUBE_GREEN		0.32
#define CUBE_BLUE		0.31
#define CUBE_ALPHA		0.8

#define CUBE_VID_TEX_WIDTH	320
#define CUBE_VID_TEX_HEIGTH	240

#define CUBE_X_SPEED		4.0f
#define CUBE_Y_SPEED		4.0f
#define CUBE_Z_SPEED		4.0f

#define CUBE_X_INIT_ANGLE	45.0f
#define CUBE_X_PACE		0.25f * CUBE_X_SPEED

#define CUBE_Y_INIT_ANGLE	45.0f
#define CUBE_Y_PACE		-0.50f * CUBE_Y_SPEED

#define CUBE_Z_INIT_ANGLE	10.0f
#define CUBE_Z_PACE		0.15f * CUBE_Z_SPEED

/* This reset define is here to limit the frame cumulation value that
 * overflows mathematical functions like fmod or sin/cos and leads in
 * erratic behavior after several days of running.
 * Value has been derived based on init angle from each axes and the
 * actual speed values above.
 * Changing any initialize angles or cube speed will mandatory modify this
 * reset value.
 */
#define FRAME_CUMUL_RESET_VALUE	1800


static inline int __egl_check(void *ptr, const char *name)
{
	if (!ptr) {
		printf("no %s\n", name);
		return -1;
	}
	return 0;
}

#define egl_check(egl, name) __egl_check((egl)->name, #name)

int  init_egl(struct display *d, struct window *w);
void fini_egl(struct display *d);

int  init_supported_modifiers_for_egl(struct display *d);
int  int_gbm(struct display *d, char const* drm_render_node);

int  link_program(unsigned program);
int  create_program(const char *vs_src, const char *fs_src);

void init_cube_smooth(struct window *window);
void init_cube_tex(struct window *window);

void compute_pitch(struct window *window);

#ifdef HAVE_GST
struct decoder;
struct decoder * video_init(const struct _egl *egl,
							const struct _gbm *gbm,
							const char *filename,
							char *fps);
EGLImage video_frame(struct decoder *dec);
void video_deinit(struct decoder *dec);
void init_cube_video(struct window *w,
					 const char *filenames);

void cube_next_shader(struct window *w);
#else
static inline void init_cube_video(struct window *w,
								   const char *filenames)
{
	printf("no GStreamer support!\n");
}
static inline void cube_next_shader(struct window *w) {};
#endif

#endif

