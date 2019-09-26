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

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>
#include <sys/time.h>
#include <string.h>

#include <wayland-client.h>

#include "cube-common.h"
#include "shared/helpers.h"
#include "esUtil.h"


struct gl {
	GLfloat aspect;
	GLuint pos, col, normal;
	GLuint program;
	GLint modelviewmatrix, modelviewprojectionmatrix, normalmatrix;
	GLuint vbo;
	GLuint positionsoffset, colorsoffset, normalsoffset;
} gl_smooth;

static const GLfloat vVertices[] = {
		// front
		-1.0f, -1.0f, +1.0f,
		+1.0f, -1.0f, +1.0f,
		-1.0f, +1.0f, +1.0f,
		+1.0f, +1.0f, +1.0f,
		// back
		+1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f,
		+1.0f, +1.0f, -1.0f,
		-1.0f, +1.0f, -1.0f,
		// right
		+1.0f, -1.0f, +1.0f,
		+1.0f, -1.0f, -1.0f,
		+1.0f, +1.0f, +1.0f,
		+1.0f, +1.0f, -1.0f,
		// left
		-1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f, +1.0f,
		-1.0f, +1.0f, -1.0f,
		-1.0f, +1.0f, +1.0f,
		// top
		-1.0f, +1.0f, +1.0f,
		+1.0f, +1.0f, +1.0f,
		-1.0f, +1.0f, -1.0f,
		+1.0f, +1.0f, -1.0f,
		// bottom
		-1.0f, -1.0f, -1.0f,
		+1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f, +1.0f,
		+1.0f, -1.0f, +1.0f,
};

static const GLfloat vColors[] = {
		// front
		0.0f,  0.0f,  1.0f, // blue
		1.0f,  0.0f,  1.0f, // magenta
		0.0f,  1.0f,  1.0f, // cyan
		1.0f,  1.0f,  1.0f, // white
		// back
		1.0f,  0.0f,  0.0f, // red
		0.0f,  0.0f,  0.0f, // black
		1.0f,  1.0f,  0.0f, // yellow
		0.0f,  1.0f,  0.0f, // green
		// right
		1.0f,  0.0f,  1.0f, // magenta
		1.0f,  0.0f,  0.0f, // red
		1.0f,  1.0f,  1.0f, // white
		1.0f,  1.0f,  0.0f, // yellow
		// left
		0.0f,  0.0f,  0.0f, // black
		0.0f,  0.0f,  1.0f, // blue
		0.0f,  1.0f,  0.0f, // green
		0.0f,  1.0f,  1.0f, // cyan
		// top
		0.0f,  1.0f,  1.0f, // cyan
		1.0f,  1.0f,  1.0f, // white
		0.0f,  1.0f,  0.0f, // green
		1.0f,  1.0f,  0.0f, // yellow
		// bottom
		0.0f,  0.0f,  0.0f, // black
		1.0f,  0.0f,  0.0f, // red
		0.0f,  0.0f,  1.0f, // blue
		1.0f,  0.0f,  1.0f  // magenta
};

static const GLfloat vNormals[] = {
		// front
		+0.0f, +0.0f, +1.0f, // forward
		+0.0f, +0.0f, +1.0f, // forward
		+0.0f, +0.0f, +1.0f, // forward
		+0.0f, +0.0f, +1.0f, // forward
		// back
		+0.0f, +0.0f, -1.0f, // backward
		+0.0f, +0.0f, -1.0f, // backward
		+0.0f, +0.0f, -1.0f, // backward
		+0.0f, +0.0f, -1.0f, // backward
		// right
		+1.0f, +0.0f, +0.0f, // right
		+1.0f, +0.0f, +0.0f, // right
		+1.0f, +0.0f, +0.0f, // right
		+1.0f, +0.0f, +0.0f, // right
		// left
		-1.0f, +0.0f, +0.0f, // left
		-1.0f, +0.0f, +0.0f, // left
		-1.0f, +0.0f, +0.0f, // left
		-1.0f, +0.0f, +0.0f, // left
		// top
		+0.0f, +1.0f, +0.0f, // up
		+0.0f, +1.0f, +0.0f, // up
		+0.0f, +1.0f, +0.0f, // up
		+0.0f, +1.0f, +0.0f, // up
		// bottom
		+0.0f, -1.0f, +0.0f, // down
		+0.0f, -1.0f, +0.0f, // down
		+0.0f, -1.0f, +0.0f, // down
		+0.0f, -1.0f, +0.0f  // down
};

static const char *vertex_shader_source =
		"uniform mat4 modelviewMatrix;      \n"
		"uniform mat4 modelviewprojectionMatrix;\n"
		"uniform mat3 normalMatrix;         \n"
		"                                   \n"
		"attribute vec4 in_position;        \n"
		"attribute vec3 in_normal;          \n"
		"attribute vec4 in_color;           \n"
		"\n"
		"vec4 lightSource = vec4(2.0, 2.0, 20.0, 0.0);\n"
		"                                   \n"
		"varying vec4 vVaryingColor;        \n"
		"                                   \n"
		"void main()                        \n"
		"{                                  \n"
		"    gl_Position = modelviewprojectionMatrix * in_position;\n"
		"    vec3 vEyeNormal = normalMatrix * in_normal;\n"
		"    vec4 vPosition4 = modelviewMatrix * in_position;\n"
		"    vec3 vPosition3 = vPosition4.xyz / vPosition4.w;\n"
		"    vec3 vLightDir = normalize(lightSource.xyz - vPosition3);\n"
		"    float diff = max(0.0, dot(vEyeNormal, vLightDir));\n"
		"    vVaryingColor = vec4(diff * in_color.rgb, 1.0);\n"
		"}                                  \n";

static const char *fragment_shader_source =
		"precision mediump float;           \n"
		"                                   \n"
		"varying vec4 vVaryingColor;        \n"
		"                                   \n"
		"void main()                        \n"
		"{                                  \n"
		"    gl_FragColor = vVaryingColor;  \n"
		"}                                  \n";

static void draw_cube_smooth(void *data, struct wl_callback *callback )
{
	struct window *w = data;
	struct display *d = w->display;
	struct gl *pgl = w->gl;
	struct wl_region *region;
	struct timeval tv;
	ESMatrix4x4 modelview;
	static const uint32_t benchmark_interval = 5;
	uint32_t time;
	EGLint rect[8];
	EGLint buffer_age = 0;
	struct point move;

	assert(w->callback == callback);
	w->callback = NULL;
	if (callback)
		wl_callback_destroy(callback);

	gettimeofday(&tv, NULL);
	time = tv.tv_sec * 1000 + tv.tv_usec / 1000;
	if (w->frames == 0)
		w->benchmark_time = time;
	if (time - w->benchmark_time > (benchmark_interval * 1000)) {
		printf("%d frames in %d seconds: %f fps\n",
		       w->frames,
		       benchmark_interval,
		       (float) w->frames / benchmark_interval);
		w->benchmark_time = time;
		w->frames = 0;
	}

	if (d->egl.swap_buffers_with_damage)
		eglQuerySurface(d->egl.dpy, w->egl_surface,
				EGL_BUFFER_AGE_EXT, &buffer_age);

	glViewport(0, 0, w->geometry.width, w->geometry.height);
	pgl->aspect = (GLfloat)(w->geometry.height) /
		(GLfloat)(w->geometry.width);

	/* clear the color buffer */
	glClearColor(CUBE_RED, CUBE_GREEN, CUBE_BLUE, CUBE_ALPHA);
#ifdef DAMAGE_DEBUG
	glClearColor((GLfloat)(w->frames_cumul % 255) / 255.0f,
		     0.0, 0.0, 0.5);
#endif
	glClear(GL_COLOR_BUFFER_BIT);

	esMatrixLoadIdentity(&modelview);
	move = w->move;
	esTranslate(&modelview,
		    move.x / w->pitch.x, -move.y /w->pitch.y,
		    -Z_TRANSLATION);

	esRotate(&modelview, CUBE_X_INIT_ANGLE + CUBE_X_PACE * w->frames_cumul,
		 1.0f, 0.0f, 0.0f);
	esRotate(&modelview, CUBE_Y_INIT_ANGLE + CUBE_Y_PACE * w->frames_cumul,
		 0.0f, 1.0f, 0.0f);
	esRotate(&modelview, CUBE_Z_INIT_ANGLE + CUBE_Z_PACE * w->frames_cumul,
		 0.0f, 0.0f, 1.0f);

	ESMatrix4x4 projection;
	esMatrixLoadIdentity(&projection);
	esFrustum(&projection,
		  FRUSTRUM_LEFT,                 FRUSTRUM_RIGTH,
		  FRUSTRUM_BOTTOM * pgl->aspect, FRUSTRUM_TOP * pgl->aspect,
		  FRUSTRUM_NEAR_Z,               FRUSTRUM_FAR_Z);

	ESMatrix4x4 modelviewprojection;
	esMatrixLoadIdentity(&modelviewprojection);
	esMatrixMultiply(&modelviewprojection, &modelview, &projection);

	float normal[9];
	normal[0] = modelview.m4x4[0][0];
	normal[1] = modelview.m4x4[0][1];
	normal[2] = modelview.m4x4[0][2];
	normal[3] = modelview.m4x4[1][0];
	normal[4] = modelview.m4x4[1][1];
	normal[5] = modelview.m4x4[1][2];
	normal[6] = modelview.m4x4[2][0];
	normal[7] = modelview.m4x4[2][1];
	normal[8] = modelview.m4x4[2][2];

	glUniformMatrix4fv(pgl->modelviewmatrix, 1, GL_FALSE,
			   &modelview.m4x4[0][0]);
	glUniformMatrix4fv(pgl->modelviewprojectionmatrix, 1, GL_FALSE,
			   &modelviewprojection.m4x4[0][0]);
	glUniformMatrix3fv(pgl->normalmatrix, 1, GL_FALSE, normal);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glDrawArrays(GL_TRIANGLE_STRIP, 4, 4);
	glDrawArrays(GL_TRIANGLE_STRIP, 8, 4);
	glDrawArrays(GL_TRIANGLE_STRIP, 12, 4);
	glDrawArrays(GL_TRIANGLE_STRIP, 16, 4);
	glDrawArrays(GL_TRIANGLE_STRIP, 20, 4);

	if (w->opaque) {
		region =
		       wl_compositor_create_region(w->display->compositor);
		wl_region_add(region, 0, 0,
			      w->geometry.width,
			      w->geometry.height);
		wl_surface_set_opaque_region(w->surface, region);
		wl_region_destroy(region);
	} else {
		wl_surface_set_opaque_region(w->surface, NULL);
	}

	if (d->egl.swap_buffers_with_damage) {
		if (buffer_age > 0) {
#ifdef DAMAGE_DEBUG
			rect[0] = (EGLint)w->corner.x;
			rect[1] = (EGLint)w->corner.y;
			rect[2] = (EGLint)w->rect.x;
			rect[3] = (EGLint)w->rect.y;
#else
			rect[0] = w->geometry.width  * DAMAGE_X_PERCENT +
				move.x;
			rect[1] = w->geometry.height * DAMAGE_Y_PERCENT -
				move.y;
			rect[2] = w->geometry.width  * DAMAGE_W_PERCENT;
			rect[3] = w->geometry.height * DAMAGE_H_PERCENT ;
#endif
			memcpy(&rect[4], w->damage, 4 * sizeof(EGLint));
			d->egl.swap_buffers_with_damage(d->egl.dpy,
							w->egl_surface,
							rect, 2);
			memcpy(w->damage, rect, 4 * sizeof(EGLint));

#ifdef DAMAGE_DEBUG
			printf("[1] {x,y}[w x h]: {%i,%i}[%i x %i]\n"
			       "[2] {x,y}[w x h]: {%i,%i}[%i x %i]\n",
			       rect[0], rect[1] ,rect[2] ,rect[3],
			       rect[4], rect[5] ,rect[6] ,rect[7]);
#endif

			wl_surface_damage(w->surface,
					  rect[0], rect[1],
					  rect[2], rect[3]);
			wl_surface_commit(w->surface);
		 } else {
			w->damage[0] = 0;
			w->damage[1] = 0;
			w->damage[2] = w->geometry.width;
			w->damage[3] = w->geometry.height;
			eglSwapBuffers(d->egl.dpy, w->egl_surface);
		 }
	} else {
		eglSwapBuffers(d->egl.dpy, w->egl_surface);
	}

	w->frames++;
	if (w->frames_cumul++ >= FRAME_CUMUL_RESET_VALUE) {
		printf("Reseting !!\n");
		w->frames_cumul  = 0;
	}
}

void init_cube_smooth(struct window *w)
{
	struct gl *pgl;
	int ret;

	pgl = &gl_smooth;

	pgl->aspect = (GLfloat)(w->geometry.width) /
		(GLfloat)(w->geometry.height);

	ret = create_program(vertex_shader_source, fragment_shader_source);
	if (ret < 0)
		return;

	pgl->pos = 0;
	pgl->normal = 1;
	pgl->col = 2;
	pgl->program = ret;

	glBindAttribLocation(pgl->program, pgl->pos, "in_position");
	glBindAttribLocation(pgl->program, pgl->normal, "in_normal");
	glBindAttribLocation(pgl->program, pgl->col, "in_color");

	ret = link_program(pgl->program);
	if (ret)
		return;

	glUseProgram(pgl->program);

	pgl->modelviewmatrix = glGetUniformLocation(pgl->program,
						    "modelviewMatrix");
	pgl->modelviewprojectionmatrix =
		glGetUniformLocation(pgl->program,
				     "modelviewprojectionMatrix");
	pgl->normalmatrix = glGetUniformLocation(pgl->program, "normalMatrix");

	glViewport(0, 0, w->geometry.width, w->geometry.width);
	glEnable(GL_CULL_FACE);

	pgl->positionsoffset = 0;
	pgl->colorsoffset = sizeof(vVertices);
	pgl->normalsoffset = sizeof(vVertices) + sizeof(vColors);
	glGenBuffers(1, &pgl->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, pgl->vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vVertices) + sizeof(vColors) +
		     sizeof(vNormals), 0, GL_STATIC_DRAW);

	glBufferSubData(GL_ARRAY_BUFFER, pgl->positionsoffset,
			sizeof(vVertices), &vVertices[0]);
	glBufferSubData(GL_ARRAY_BUFFER, pgl->colorsoffset, sizeof(vColors),
			&vColors[0]);
	glBufferSubData(GL_ARRAY_BUFFER, pgl->normalsoffset, sizeof(vNormals),
			&vNormals[0]);

	glVertexAttribPointer(pgl->pos, 3, GL_FLOAT, GL_FALSE, 0,
			      (const GLvoid *)(intptr_t)pgl->positionsoffset);
	glEnableVertexAttribArray(pgl->pos);

	glVertexAttribPointer(pgl->normal, 3, GL_FLOAT, GL_FALSE, 0,
			      (const GLvoid *)(intptr_t)pgl->normalsoffset);
	glEnableVertexAttribArray(pgl->normal);

	glVertexAttribPointer(pgl->col, 3, GL_FLOAT, GL_FALSE, 0,
			      (const GLvoid *)(intptr_t)pgl->colorsoffset);
	glEnableVertexAttribArray(pgl->col);

	w->redraw = draw_cube_smooth;
	w->next_shader = NULL;

	w->gl = pgl;
}

