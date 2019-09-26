//
// Book:      OpenGL(R) ES 2.0 Programming Guide
// Authors:   Aaftab Munshi, Dan Ginsburg, Dave Shreiner
// ISBN-10:   0321502795
// ISBN-13:   9780321502797
// Publisher: Addison-Wesley Professional
// URLs:      http://safari.informit.com/9780321563835
//            http://www.opengles-book.com
//

/*
 * (c) 2009 Aaftab Munshi, Dan Ginsburg, Dave Shreiner
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

// ESUtil.c
//
//    A utility library for OpenGL ES.  This library provides a
//    basic common framework for the example applications in the
//    OpenGL ES 2.0 Programming Guide.
//

///
//  Includes
//
#include <stdio.h>
#include "esUtil.h"
#include <math.h>
#include <string.h>

#define PI 3.1415926535897932384626433832795f

void ESUTIL_API
esDumpVec(float *f, unsigned char dim)
{
	int         i;

	printf("           ");
	for (i = 0; i < dim; i++)
		printf("%i       ", i);
	printf("\n");

	printf("         ");
	for (i = 0; i < dim; i++)
		printf("%+7.2f ", *(f + i));
	printf("]\n");
}


void ESUTIL_API
esDumpMat(float *f, unsigned char dim)
{
	int         i, j;

	printf("           ");
	for (i = 0; i < dim; i++)
		printf("%i       ", i);
	printf("\n");

	for (i = 0; i < dim; i++) {
		printf("[ %d ] [ ", i);
		for (j = 0; j < dim; j++)
			printf("%7.2f ", *(f + j + i * dim));
		printf("]\n");
	}
}

void ESUTIL_API
esScale(ESMatrix4x4 *result, GLfloat sx, GLfloat sy, GLfloat sz)
{
	result->m4x4[0][0] *= sx;
	result->m4x4[0][1] *= sx;
	result->m4x4[0][2] *= sx;
	result->m4x4[0][3] *= sx;

	result->m4x4[1][0] *= sy;
	result->m4x4[1][1] *= sy;
	result->m4x4[1][2] *= sy;
	result->m4x4[1][3] *= sy;

	result->m4x4[2][0] *= sz;
	result->m4x4[2][1] *= sz;
	result->m4x4[2][2] *= sz;
	result->m4x4[2][3] *= sz;
}

void ESUTIL_API
esTranslate(ESMatrix4x4 *result, GLfloat tx, GLfloat ty, GLfloat tz)
{
	result->m4x4[3][0] += (result->m4x4[0][0] * tx +
			       result->m4x4[1][0] * ty +
			       result->m4x4[2][0] * tz);
	result->m4x4[3][1] += (result->m4x4[0][1] * tx +
			       result->m4x4[1][1] * ty +
			       result->m4x4[2][1] * tz);
	result->m4x4[3][2] += (result->m4x4[0][2] * tx +
			       result->m4x4[1][2] * ty +
			       result->m4x4[2][2] * tz);
	result->m4x4[3][3] += (result->m4x4[0][3] * tx +
			       result->m4x4[1][3] * ty +
			       result->m4x4[2][3] * tz);
}

void ESUTIL_API
esRotate(ESMatrix4x4 *result, GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
	GLfloat sinAngle, cosAngle;
	GLfloat mag = sqrtf(x * x + y * y + z * z);

	sinAngle = sinf ( angle * PI / 180.0f );
	cosAngle = cosf ( angle * PI / 180.0f );
	if ( mag > 0.0f )
	{
		GLfloat xx, yy, zz, xy, yz, zx, xs, ys, zs;
		GLfloat oneMinusCos;
		ESMatrix4x4 rotMat;

		x /= mag;
		y /= mag;
		z /= mag;

		xx = x * x;
		yy = y * y;
		zz = z * z;
		xy = x * y;
		yz = y * z;
		zx = z * x;
		xs = x * sinAngle;
		ys = y * sinAngle;
		zs = z * sinAngle;
		oneMinusCos = 1.0f - cosAngle;

		rotMat.m4x4[0][0] = (oneMinusCos * xx) + cosAngle;
		rotMat.m4x4[0][1] = (oneMinusCos * xy) - zs;
		rotMat.m4x4[0][2] = (oneMinusCos * zx) + ys;
		rotMat.m4x4[0][3] = 0.0F;

		rotMat.m4x4[1][0] = (oneMinusCos * xy) + zs;
		rotMat.m4x4[1][1] = (oneMinusCos * yy) + cosAngle;
		rotMat.m4x4[1][2] = (oneMinusCos * yz) - xs;
		rotMat.m4x4[1][3] = 0.0F;

		rotMat.m4x4[2][0] = (oneMinusCos * zx) - ys;
		rotMat.m4x4[2][1] = (oneMinusCos * yz) + xs;
		rotMat.m4x4[2][2] = (oneMinusCos * zz) + cosAngle;
		rotMat.m4x4[2][3] = 0.0F;

		rotMat.m4x4[3][0] = 0.0F;
		rotMat.m4x4[3][1] = 0.0F;
		rotMat.m4x4[3][2] = 0.0F;
		rotMat.m4x4[3][3] = 1.0F;

		esMatrixMultiply( result, &rotMat, result );
	}
}

void ESUTIL_API
esFrustum(ESMatrix4x4 *result, float left, float right, float bottom, float top,
	  float nearZ, float farZ)
{
	float       deltaX = right - left;
	float       deltaY = top - bottom;
	float       deltaZ = farZ - nearZ;
	ESMatrix4x4    frust;

	if ( (nearZ <= 0.0f) || (farZ <= 0.0f) ||
	     (deltaX <= 0.0f) || (deltaY <= 0.0f) || (deltaZ <= 0.0f) )
		return;

	frust.m4x4[0][0] = 2.0f * nearZ / deltaX;
	frust.m4x4[0][1] = frust.m4x4[0][2] = frust.m4x4[0][3] = 0.0f;

	frust.m4x4[1][1] = 2.0f * nearZ / deltaY;
	frust.m4x4[1][0] = frust.m4x4[1][2] = frust.m4x4[1][3] = 0.0f;

	frust.m4x4[2][0] = (right + left) / deltaX;
	frust.m4x4[2][1] = (top + bottom) / deltaY;
	frust.m4x4[2][2] = -(nearZ + farZ) / deltaZ;
	frust.m4x4[2][3] = -1.0f;

	frust.m4x4[3][2] = -2.0f * nearZ * farZ / deltaZ;
	frust.m4x4[3][0] = frust.m4x4[3][1] = frust.m4x4[3][3] = 0.0f;

	esMatrixMultiply(result, &frust, result);
}


void ESUTIL_API
esPerspective(ESMatrix4x4 *result, float fovy, float aspect, float nearZ, float farZ)
{
	GLfloat frustumW, frustumH;

	frustumH = tanf( fovy / 360.0f * PI ) * nearZ;
	frustumW = frustumH * aspect;

	esFrustum( result, -frustumW, frustumW, -frustumH, frustumH, nearZ, farZ );
}

void ESUTIL_API
esOrtho(ESMatrix4x4 *result, float left, float right, float bottom, float top, float nearZ, float farZ)
{
	float       deltaX = right - left;
	float       deltaY = top - bottom;
	float       deltaZ = farZ - nearZ;
	ESMatrix4x4    ortho;

	if ( (deltaX == 0.0f) || (deltaY == 0.0f) || (deltaZ == 0.0f) )
		return;

	esMatrixLoadIdentity(&ortho);
	ortho.m4x4[0][0] = 2.0f / deltaX;
	ortho.m4x4[3][0] = -(right + left) / deltaX;
	ortho.m4x4[1][1] = 2.0f / deltaY;
	ortho.m4x4[3][1] = -(top + bottom) / deltaY;
	ortho.m4x4[2][2] = -2.0f / deltaZ;
	ortho.m4x4[3][2] = -(nearZ + farZ) / deltaZ;

	esMatrixMultiply(result, &ortho, result);
}


void ESUTIL_API
esMatrixMultiply(ESMatrix4x4 *result, ESMatrix4x4 *srcA, ESMatrix4x4 *srcB)
{
	ESMatrix4x4    tmp;
	int         i;

	for (i=0; i<4; i++)
	{
		tmp.m4x4[i][0] = (srcA->m4x4[i][0] * srcB->m4x4[0][0]) +
			(srcA->m4x4[i][1] * srcB->m4x4[1][0]) +
			(srcA->m4x4[i][2] * srcB->m4x4[2][0]) +
			(srcA->m4x4[i][3] * srcB->m4x4[3][0]) ;

		tmp.m4x4[i][1] = (srcA->m4x4[i][0] * srcB->m4x4[0][1]) +
			(srcA->m4x4[i][1] * srcB->m4x4[1][1]) +
			(srcA->m4x4[i][2] * srcB->m4x4[2][1]) +
			(srcA->m4x4[i][3] * srcB->m4x4[3][1]) ;

		tmp.m4x4[i][2] = (srcA->m4x4[i][0] * srcB->m4x4[0][2]) +
			(srcA->m4x4[i][1] * srcB->m4x4[1][2]) +
			(srcA->m4x4[i][2] * srcB->m4x4[2][2]) +
			(srcA->m4x4[i][3] * srcB->m4x4[3][2]) ;

		tmp.m4x4[i][3] = (srcA->m4x4[i][0] * srcB->m4x4[0][3]) +
			(srcA->m4x4[i][1] * srcB->m4x4[1][3]) +
			(srcA->m4x4[i][2] * srcB->m4x4[2][3]) +
			(srcA->m4x4[i][3] * srcB->m4x4[3][3]) ;
	}
	memcpy(result, &tmp, sizeof(ESMatrix4x4));
}

void ESUTIL_API
esMatrixClipped(ESVec4 *result, ESMatrix4x4 *m, ESVec3 *v)
{
	ESVec4    tmp;

	tmp.vec4[0] =
		m->m4x4[0][0] * v->vec3[0] +  m->m4x4[1][0] * v->vec3[1]+
		m->m4x4[2][0] * v->vec3[2] +  m->m4x4[3][0] * 1.0f;
	tmp.vec4[1] =
		m->m4x4[0][1] * v->vec3[0] +  m->m4x4[1][1] * v->vec3[1]+
		m->m4x4[2][1] * v->vec3[2] +  m->m4x4[3][1] * 1.0f;
	tmp.vec4[2] =
		m->m4x4[0][2] * v->vec3[0] +  m->m4x4[1][2] * v->vec3[1]+
		m->m4x4[2][2] * v->vec3[2] +  m->m4x4[3][2] * 1.0f;
	tmp.vec4[3] =
		m->m4x4[0][3] * v->vec3[0] +  m->m4x4[1][3] * v->vec3[1]+
		m->m4x4[2][3] * v->vec3[2] +  m->m4x4[3][3] * 1.0f;

	memcpy(result, &tmp, sizeof(ESVec4));
}

void ESUTIL_API
esMatrixNDC(ESVec3 *result, ESVec4 *c)
{
	ESVec3    tmp;

	tmp.vec3[0] =	c->vec4[0] / c->vec4[3];
	tmp.vec3[1] =	c->vec4[1] / c->vec4[3];
	tmp.vec3[2] =	c->vec4[2] / c->vec4[3];
	memcpy(result, &tmp, sizeof(ESVec3));
}

void ESUTIL_API
esMatrixWindow(ESVec2 *result, ESVec3 *NDC, int width, int height)
{
	ESVec2    tmp;

	tmp.vec2[0] = width  * NDC->vec3[0] / 2 + width  / 2;
	tmp.vec2[1] = height * NDC->vec3[1] / 2 + height / 2;

	memcpy(result, &tmp, sizeof(ESVec2));
}

void ESUTIL_API
esMatrixLoadIdentity(ESMatrix4x4 *result)
{
	memset(result, 0x0, sizeof(ESMatrix4x4));
	result->m4x4[0][0] = 1.0f;
	result->m4x4[1][1] = 1.0f;
	result->m4x4[2][2] = 1.0f;
	result->m4x4[3][3] = 1.0f;
}

