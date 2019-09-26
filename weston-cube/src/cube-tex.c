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
#include "image-loader.h"
#include "esUtil.h"

#define MAX_PROG 4

struct face_ctx {
	GLuint prg;
	struct {
		GLint modelviewmatrix;
		GLint modelviewprojectionmatrix;
		GLint normalmatrix;
		GLint texture;
		GLint frame;
		GLint reso;
	} attr;
};

struct gl {
	GLfloat aspect;
	GLuint pos, tex, normal;

	struct face_ctx face[MAX_PROG];

	GLuint positionsoffset, texcoordsoffset, normalsoffset;
	GLuint vbo, texhandle[3];
	pixman_image_t *texture[3];
	GLfloat *TexCoords;
	enum mode mod;
} gl_tex;

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

GLfloat vTexCoords1[] = {
		//front
		0.0f, 1.0f,
		1.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		//back
		0.0f, 1.0f,
		1.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		//right
		0.0f, 1.0f,
		1.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		//left
		0.0f, 1.0f,
		1.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		//top
		0.0f, 1.0f,
		1.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		//bottom
		0.0f, 1.0f,
		1.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
};

GLfloat vTexCoords6[] = {
		//front
		0.0f       , 1.0f / 2.0f,
		1.0f / 4.0f, 1.0f / 2.0f,
		0.0f       , 0.0f,
		1.0f / 4.0f, 0.0f,
		//back
		1.0f / 4.0f, 1.0f / 2.0f,
		2.0f / 4.0f, 1.0f / 2.0f,
		1.0f / 4.0f, 0.0f,
		2.0f / 4.0f, 0.0f,
		//right
		2.0f / 4.0f, 1.0f / 2.0f,
		3.0f / 4.0f, 1.0f / 2.0f,
		2.0f / 4.0f, 0.0f,
		3.0f / 4.0f, 0.0f,
		//left
		3.0f / 4.0f, 1.0f / 2.0f,
		1.0f       , 1.0f / 2.0f,
		3.0f / 4.0f, 0.0f,
		1.0        , 0.0f,
		//top
		0.0f       , 1.0f,
		1.0f / 4.0f, 1.0f,
		0.0f       , 1.0f / 2.0f,
		1.0f / 4.0f, 1.0f / 2.0f,
		//bottom
		1.0f / 4.0f, 1.0f,
		2.0f / 4.0f, 1.0f,
		1.0f / 4.0f, 1.0f / 2.0f,
		2.0f / 4.0f, 1.0f / 2.0f,
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
		"uniform mat4 modelviewMatrix;                     \n"
		"uniform mat4 modelviewprojectionMatrix;           \n"
		"uniform mat3 normalMatrix;                        \n"
		"                                                  \n"
		"attribute vec4 in_position;                       \n"
		"attribute vec2 in_TexCoord;                       \n"
		"attribute vec3 in_normal;                         \n"
		"                                                  \n"
		"vec4 lightSource = vec4(2.0, 2.0, 20.0, 0.0);     \n"
		"                                                  \n"
		"varying vec4 vVaryingColor;                       \n"
		"varying vec2 vTexCoord;                           \n"
		"                                                  \n"
		"void main()                                       \n"
		"{                                                 \n"
		"    gl_Position = modelviewprojectionMatrix *     \n"
		"                  in_position;                    \n"
		"    vec3 vEyeNormal = normalMatrix * in_normal;   \n"
		"    vec4 vPosition4 = modelviewMatrix *           \n"
		"                      in_position;                \n"
		"    vec3 vPosition3 = vPosition4.xyz /            \n"
		"                      vPosition4.w;               \n"
		"    vec3 vLightDir = normalize(lightSource.xyz -  \n"
		"                               vPosition3);       \n"
		"    float diff = max(0.0, dot(vEyeNormal,         \n"
		"                              vLightDir));        \n"
		"    vVaryingColor = vec4(diff *                   \n"
		"                    vec3(1.0, 1.0, 1.0), 1.0);    \n"
		"    vTexCoord = in_TexCoord;                      \n"
		"}                                                 \n";

static const char *fragment_shader_source0 =
		"precision mediump float;                          \n"
		"                                                  \n"
		"uniform sampler2D uTex;                           \n"
		"                                                  \n"
		"varying vec4 vVaryingColor;                       \n"
		"varying vec2 vTexCoord;                           \n"
		"                                                  \n"
		"void main()                                       \n"
		"{                                                 \n"
		"    gl_FragColor = vVaryingColor *                \n"
		"                   texture2D(uTex,                \n"
		"                             vTexCoord).bgra;     \n"
		"}                                                 \n";

/* https://www.shadertoy.com/view/XsVSW1 */
static const char *fragment_shader_source1 =
		"precision mediump float;                          \n"
		"                                                  \n"
		"uniform sampler2D uTex;                           \n"
		"                                                  \n"
		"varying vec4 vVaryingColor;                       \n"
		"varying vec2 vTexCoord;                           \n"
		"                                                  \n"
		"void main()                                       \n"
		"{                                                 \n"
		"    vec2 p = vTexCoord - 0.5;                     \n"
		"                                                  \n"
		"    float r = length(p);                          \n"
		"    float a = atan(p.y, p.x);                     \n"
		"                                                  \n"
		"    r = r * r * 3.0;                              \n"
		"    p = r * vec2(cos(a) * 0.5, sin(a) * 0.5);     \n"
		"                                                  \n"
		"    vec4 color = texture2D(uTex, p + 0.5).bgra;   \n"
		"    gl_FragColor = vVaryingColor * color;         \n"
		"}                                                 \n";

/* https://www.shadertoy.com/view/XlccRX */
static const char *fragment_shader_source2 =
		"precision mediump float;                          \n"
		"                                                  \n"
		"uniform sampler2D uTex;                           \n"
		"uniform vec3 uReso;                               \n"
		"uniform float uFrame;                             \n"
		"                                                  \n"
		"varying vec4 vVaryingColor;                       \n"
		"varying vec2 vTexCoord;                           \n"
		"                                                  \n"
		"const bool LUMA = true;                           \n"
		"                                                  \n"
		"float luma(vec4 color){                           \n"
		"    //OpenGL 4.0 Shading Language Cookbook,       \n"
		"    //page 154                                    \n"
		"    return 0.2126 * color.x +                     \n"
		"           0.7152 * color.y +                     \n"
		"           0.0722 * color.z;                      \n"
		"}                                                 \n"
		"                                                  \n"
		"void main()                                       \n"
		"{                                                 \n"
		"    vec2 uv = vTexCoord;                          \n"
		"                                                  \n"
		"    float dx = 3.0 / uReso.x;                     \n"
		"    float dy = 3.0 / uReso.y;                     \n"
		"                                                  \n"
		"    vec4 _00 = texture2D(uTex,                    \n"
		"                         uv + vec2(-dx,-dy));     \n"
		"    vec4 _01 = texture2D(uTex,                    \n"
		"                         uv + vec2(-dx,0.0));     \n"
		"    vec4 _02 = texture2D(uTex,                    \n"
		"                         uv + vec2(-dx, dy));     \n"
		"    vec4 _10 = texture2D(uTex,                    \n"
		"                         uv + vec2(0.0,-dy));     \n"
		"    vec4 _11 = texture2D(uTex,                    \n"
		"                         uv + vec2(0.0,0.0));     \n"
		"    vec4 _12 = texture2D(uTex,                    \n"
		"                         uv + vec2(0.0, dy));     \n"
		"    vec4 _20 = texture2D(uTex,                    \n"
		"                         uv + vec2( dx,-dy));     \n"
		"    vec4 _21 = texture2D(uTex,                    \n"
		"                         uv + vec2( dx,0.0));     \n"
		"    vec4 _22 = texture2D(uTex,                    \n"
		"                         uv + vec2( dx, dy));     \n"
		"                                                  \n"
		"   if (LUMA) {                                    \n"
		"        _00 = vec4(luma(_00));                    \n"
		"        _01 = vec4(luma(_01));                    \n"
		"        _02 = vec4(luma(_02));                    \n"
		"        _10 = vec4(luma(_10));                    \n"
		"        _11 = vec4(luma(_11));                    \n"
		"        _12 = vec4(luma(_12));                    \n"
		"        _20 = vec4(luma(_20));                    \n"
		"        _21 = vec4(luma(_21));                    \n"
		"        _22 = vec4(luma(_22));                    \n"
		"    }                                             \n"
		"                                                  \n"
		"    vec4 horiz = _00 + 2.0 * _01 + _02 - _20 -    \n"
		"                 2.0 * _21 - _22;                 \n"
		"    vec4 vert  = _00 + 2.0 * _10 + _20 - _02 -    \n"
		"                 2.0 * _12 - _22;                 \n"
		"                                                  \n"
		"    vec4 sobel = sqrt(horiz * horiz +             \n"
		"                      vert * vert);               \n"
		"                                                  \n"
		"    vec3 col = 0.5 + 0.5 * cos(uFrame +           \n"
		"               uv.xyx + vec3(0,2,4));             \n"
		"    gl_FragColor = vVaryingColor *                \n"
		"                   vec4(sobel.xyz, 1.0).bgra *    \n"
		"                   vec4(col.xyz, 1.0).bgra;       \n"
		"}                                                 \n";

/* https://www.shadertoy.com/view/lstfzl */
/*
 * based on the barrel deformation shader taken from:
   http://www.geeks3d.com/20140213/glsl-shader-library-fish-eye-and-dome-and-barrel-distortion-post-processing-filters/2/
*/
static const char *fragment_shader_source3 =
		"precision mediump float;                          \n"
		"                                                  \n"
		"uniform sampler2D uTex;                  \n"
		"uniform float uFrame;                             \n"
		"                                                  \n"
		"varying vec4 vVaryingColor;                       \n"
		"varying vec2 vTexCoord;                           \n"
		"                                                  \n"
		"//CONTROL VARIABLES                               \n"
		"// barrel power - (values between 0-1 work well)  \n"
		"float uPower = 0.5;                               \n"
		"float uSpeed = 1.0;                               \n"
		"float uFrequency = 8.0;                           \n"
		"                                                  \n"
		"vec2 Distort(vec2 p, float power, float speed,    \n"
		"             float freq)                          \n"
		"{                                                 \n"
		"    float theta  = atan(p.y, p.x);                \n"
		"    float radius = length(p);                     \n"
		"    radius = pow(radius, power * sin( radius *    \n"
		"             freq - uFrame * speed ) + 1.0);      \n"
		"    p.x = radius * cos(theta);                    \n"
		"    p.y = radius * sin(theta);                    \n"
		"    return 0.5 * (p + 1.0);                       \n"
		"}                                                 \n"
		"                                                  \n"
		"void main()                                       \n"
		"{                                                 \n"
		"    vec2 xy = 2.0 * vTexCoord - 1.0;              \n"
		"    vec2 uvt;                                     \n"
		"    float d = length(xy);                         \n"
		"                                                  \n"
		"    //distance of distortion                      \n"
		"    if (d < 1.0 && uPower != 0.0)                 \n"
		"    {                                             \n"
		"        // if power is 0, then don't call the     \n"
		"        // distortion function since there's no   \n"
		"        // reason to do it :)                     \n"
		"        uvt = Distort(xy, uPower, uSpeed,         \n"
		"                      uFrequency);                \n"
		"    }                                             \n"
		"    else                                          \n"
		"    {                                             \n"
		"        uvt = vTexCoord;                          \n"
		"    }                                             \n"
		"    vec4 c = texture2D(uTex, uvt).bgra;           \n"
		"    gl_FragColor = vVaryingColor * c;             \n"
		"}                                                 \n";

struct _sharder_list {
	const char *name;
	const char **source;
};

static struct _sharder_list fragment_shader_sources[MAX_PROG] = {
	{ "legacy",                        &fragment_shader_source0 },
	{ "Fisheye/Pinch",                 &fragment_shader_source1 },
	{ "Derivative Outline effect",     &fragment_shader_source2 },
	{ "Sobel edge detection operator", &fragment_shader_source3 }
};

static void init_tex(struct gl *gl)
{
	int width, height, stride;
        void *tex_data;
	int i, max;

	if (gl->mod == ONE_TEX || gl->mod == ONE_MAP_TEX)
		max = 1;
	else
		max = 3;

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glGenTextures(max, gl->texhandle);
	glActiveTexture(GL_TEXTURE0);

	for (i = 0; i < max; i++){
		tex_data = pixman_image_get_data(gl->texture[i]);
		width = pixman_image_get_width(gl->texture[i]);
		height = pixman_image_get_height(gl->texture[i]);
		stride = pixman_image_get_stride(gl->texture[i]);

		printf("[%d]texture: %p - width: %i - height: %i - stride: %i\n"
		       ,i, tex_data, width, height, stride);

		glBindTexture(GL_TEXTURE_2D, gl->texhandle[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
			     GL_RGBA, GL_UNSIGNED_BYTE, tex_data);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
				GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
				GL_LINEAR);
	}
}

static void draw_cube_tex(void *data, struct wl_callback *callback)
{
	struct window *w = data;
	struct display *d = w->display;
	struct gl *pgl;
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

	pgl = w->gl;

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
		    move.x / w->pitch.x, -move.y / w->pitch.y,
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

	{
		struct face_ctx *face = &pgl->face[2];

		glUseProgram(face->prg);
		glUniformMatrix4fv(face->attr.modelviewmatrix, 1, GL_FALSE,
				   &modelview.m4x4[0][0]);
		glUniformMatrix4fv(face->attr.modelviewprojectionmatrix, 1,
				   GL_FALSE, &modelviewprojection.m4x4[0][0]);
		glUniformMatrix3fv(face->attr.normalmatrix, 1, GL_FALSE,
				   normal);
		glUniform1i(face->attr.texture, 0); /* '0' refers to texture unit 0. */
		glUniform1f(face->attr.frame, (GLfloat)w->frames_cumul);
		glUniform3f(face->attr.reso,
			    (GLfloat)pixman_image_get_width(pgl->texture[0]),
			    (GLfloat)pixman_image_get_height(pgl->texture[0]),
			    0.0f);

	}
	glBindTexture(GL_TEXTURE_2D, pgl->texhandle[0]);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glDrawArrays(GL_TRIANGLE_STRIP, 4, 4);

	{
		struct face_ctx *face = &pgl->face[3];

		glUseProgram(face->prg);
		glUniformMatrix4fv(face->attr.modelviewmatrix, 1, GL_FALSE,
				   &modelview.m4x4[0][0]);
		glUniformMatrix4fv(face->attr.modelviewprojectionmatrix, 1,
				   GL_FALSE, &modelviewprojection.m4x4[0][0]);
		glUniformMatrix3fv(face->attr.normalmatrix, 1, GL_FALSE,
				   normal);
		glUniform1i(face->attr.texture, 0); /* '0' refers to texture unit 0. */
		glUniform1f(face->attr.frame, (GLfloat)w->frames_cumul);
		glUniform3f(face->attr.reso,
			      (GLfloat)pixman_image_get_width(pgl->texture[0]),
			      (GLfloat)pixman_image_get_height(pgl->texture[0]),
			      0.0f);
		if (pgl->mod == THREE_TEX) {
			glBindTexture(GL_TEXTURE_2D, pgl->texhandle[1]);
			glUniform3f(face->attr.reso,
			      (GLfloat)pixman_image_get_width(pgl->texture[1]),
			      (GLfloat)pixman_image_get_height(pgl->texture[1]),
			      0.0f);
		}
	}
	glDrawArrays(GL_TRIANGLE_STRIP, 8, 4);
	glDrawArrays(GL_TRIANGLE_STRIP, 12, 4);

	{
		struct face_ctx *face = &pgl->face[0];

		glUseProgram(face->prg);
		glUniformMatrix4fv(face->attr.modelviewmatrix, 1, GL_FALSE,
				   &modelview.m4x4[0][0]);
		glUniformMatrix4fv(face->attr.modelviewprojectionmatrix, 1,
				   GL_FALSE, &modelviewprojection.m4x4[0][0]);
		glUniformMatrix3fv(face->attr.normalmatrix, 1, GL_FALSE,
				   normal);
		glUniform1i(face->attr.texture, 0); /* '0' refers to texture unit 0. */
		glUniform1f(face->attr.frame, (GLfloat)w->frames_cumul);
		glUniform3f(face->attr.reso,
			    (GLfloat)pixman_image_get_width(pgl->texture[0]),
			    (GLfloat)pixman_image_get_height(pgl->texture[0]),
			    0.0f);
		if (pgl->mod == THREE_TEX) {
			glBindTexture(GL_TEXTURE_2D, pgl->texhandle[2]);
			glUniform3f(face->attr.reso,
			       (GLfloat)pixman_image_get_width(pgl->texture[2]),
			       (GLfloat)pixman_image_get_height(pgl->texture[2]),
			       0.0f);
		}
	}

	glDrawArrays(GL_TRIANGLE_STRIP, 16, 4);
	glDrawArrays(GL_TRIANGLE_STRIP, 20, 4);

	if (w->opaque) {
		region = wl_compositor_create_region(w->display->compositor);
		wl_region_add(region, 0, 0,
			      w->geometry.width,
			      w->geometry.height);
		wl_surface_set_opaque_region(w->surface, region);
		wl_region_destroy(region);
	} else {
		wl_surface_set_opaque_region(w->surface, NULL);
	}

	if (d->egl.swap_buffers_with_damage ) {
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

void
init_cube_tex(struct window *w)
{
	struct gl *pgl;
	int ret, i, max;

	pgl = &gl_tex;
	pgl->mod = w->mod;

	switch (pgl->mod) {
	case ONE_TEX:
		pgl->TexCoords = &vTexCoords1[0];
		max = 1;
		break;
	case THREE_TEX:
		pgl->TexCoords = &vTexCoords1[0];
		max = 3;
		break;
	case ONE_MAP_TEX:
		pgl->TexCoords = &vTexCoords6[0];
		max = 1;
		break;
	default:
		return;
		break;
	}

	for (i = 0; i < max; i++){
		pgl->texture[i] = load_image(w->tex_filename[i]);
		if (pgl->texture[i] == NULL) {
			printf("failed to load texture file: %s\n",
			       w->tex_filename[i]);
			return;
		}
	}

	pgl->aspect = (GLfloat)(w->geometry.width) /
		(GLfloat)(w->geometry.height);

	pgl->pos = 0;
	pgl->normal = 1;
	pgl->tex = 2;

	for (i = 0; i < MAX_PROG; i++) {
		struct face_ctx *face = &pgl->face[i];
		int shader = 0;

		if (w->animated)
			shader = i;

		printf("Creating shader: \"%s\"\n",
		       fragment_shader_sources[shader].name);
		ret = create_program(vertex_shader_source,
				     *fragment_shader_sources[shader].source);
		if (ret < 0)
			goto end;
		face->prg = ret;

		glBindAttribLocation(face->prg, pgl->pos,
				     "in_position");
		glBindAttribLocation(face->prg, pgl->tex,
				     "in_TexCoord");
		glBindAttribLocation(face->prg, pgl->normal,
				     "in_normal");

		ret = link_program(face->prg);
		if (ret)
			goto end;

		face->attr.modelviewmatrix =
			glGetUniformLocation(face->prg,
					     "modelviewMatrix");
		face->attr.modelviewprojectionmatrix =
			glGetUniformLocation(face->prg,
					     "modelviewprojectionMatrix");
		face->attr.normalmatrix = glGetUniformLocation(face->prg,
							       "normalMatrix");
		face->attr.texture = glGetUniformLocation(face->prg,
							  "uTex");
		face->attr.reso = glGetUniformLocation(face->prg, "uReso");
		face->attr.frame = glGetUniformLocation(face->prg, "uFrame");
	}

	glViewport(0, 0, w->geometry.width, w->geometry.width);
	glEnable(GL_CULL_FACE);

	pgl->positionsoffset = 0;
	pgl->texcoordsoffset = sizeof(vVertices);
	pgl->normalsoffset = sizeof(vVertices) + sizeof(vTexCoords1);

	glGenBuffers(1, &pgl->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, pgl->vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vVertices) + sizeof(vTexCoords1) +
		     sizeof(vNormals), 0, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, pgl->positionsoffset,
			sizeof(vVertices), &vVertices[0]);
	glBufferSubData(GL_ARRAY_BUFFER, pgl->texcoordsoffset,
			sizeof(vTexCoords1),
			pgl->TexCoords);
	glBufferSubData(GL_ARRAY_BUFFER, pgl->normalsoffset, sizeof(vNormals),
			&vNormals[0]);
	glVertexAttribPointer(pgl->pos, 3, GL_FLOAT, GL_FALSE, 0,
			      (const GLvoid *)(intptr_t)pgl->positionsoffset);
	glEnableVertexAttribArray(pgl->pos);
	glVertexAttribPointer(pgl->normal, 3, GL_FLOAT, GL_FALSE, 0,
			      (const GLvoid *)(intptr_t)pgl->normalsoffset);
	glEnableVertexAttribArray(pgl->normal);
	glVertexAttribPointer(pgl->tex, 2, GL_FLOAT, GL_FALSE, 0,
			      (const GLvoid *)(intptr_t)pgl->texcoordsoffset);
	glEnableVertexAttribArray(pgl->tex);

	init_tex(pgl);

	w->redraw = draw_cube_tex;
	w->next_shader = NULL;

	w->gl = pgl;
end:
	return;
}
