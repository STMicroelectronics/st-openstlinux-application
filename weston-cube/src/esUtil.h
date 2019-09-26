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

//
/// \file ESUtil.h
/// \brief A utility library for OpenGL ES.  This library provides a
///        basic common framework for the example applications in the
///        OpenGL ES 2.0 Programming Guide.
//
#ifndef ESUTIL_H
#define ESUTIL_H

///
//  Includes
//
#include <GLES2/gl2.h>
#include <EGL/egl.h>

#ifdef __cplusplus

extern "C" {
#endif

///
//  Macros
//
#define ESUTIL_API
#define ESCALLBACK


/// esCreateWindow flag - RGB color buffer
#define ES_WINDOW_RGB           0
/// esCreateWindow flag - ALPHA color buffer
#define ES_WINDOW_ALPHA         1
/// esCreateWindow flag - depth buffer
#define ES_WINDOW_DEPTH         2
/// esCreateWindow flag - stencil buffer
#define ES_WINDOW_STENCIL       4
/// esCreateWindow flat - multi-sample buffer
#define ES_WINDOW_MULTISAMPLE   8


// Types
//

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

typedef struct
{
    float   m4x4[4][4];
} ESMatrix4x4;

typedef struct
{
    float   vec4[4];
} ESVec4;

typedef struct
{
    float   vec3[3];
} ESVec3;

typedef struct
{
    float   vec2[2];
} ESVec2;


typedef struct _escontext
{
   /// Put your user data here...
   void*       userData;

   /// Window width
   int       width;

   /// Window height
   int       height;

} ESContext;


///
//  Public Functions
//

void ESUTIL_API esDumpMat(float *f, unsigned char dim);

void ESUTIL_API esDumpVec(float *f, unsigned char dim);

/// \param formatStr Format string for error log.
//
void ESUTIL_API esLogMessage ( const char *formatStr, ... );

//
/// \brief Generates geometry for a sphere.  Allocates memory for the vertex data and stores
///        the results in the arrays.  Generate index list for a TRIANGLE_STRIP
/// \param numSlices The number of slices in the sphere
/// \param vertices If not NULL, will contain array of float3 positions
/// \param normals If not NULL, will contain array of float3 normals
/// \param texCoords If not NULL, will contain array of float2 texCoords
/// \param indices If not NULL, will contain the array of indices for the triangle strip
/// \return The number of indices required for rendering the buffers (the number of indices stored in the indices array
///         if it is not NULL ) as a GL_TRIANGLE_STRIP
//
int ESUTIL_API esGenSphere ( int numSlices, float radius, float **vertices, float **normals,
                             float **texCoords, unsigned int **indices );

//
/// \brief Generates geometry for a cube.  Allocates memory for the vertex data and stores
///        the results in the arrays.  Generate index list for a TRIANGLES
/// \param scale The size of the cube, use 1.0 for a unit cube.
/// \param vertices If not NULL, will contain array of float3 positions
/// \param normals If not NULL, will contain array of float3 normals
/// \param texCoords If not NULL, will contain array of float2 texCoords
/// \param indices If not NULL, will contain the array of indices for the triangle strip
/// \return The number of indices required for rendering the buffers (the number of indices stored in the indices array
///         if it is not NULL ) as a GL_TRIANGLES
//
int ESUTIL_API esGenCube ( float scale, float **vertices, float **normals,
                           float **texCoords, unsigned int **indices );

//
/// \brief multiply matrix specified by result with a scaling matrix and return new matrix in result
/// \param result Specifies the input matrix.  Scaled matrix is returned in result.
/// \param sx, sy, sz Scale factors along the x, y and z axes respectively
//
void ESUTIL_API esScale(ESMatrix4x4 *result, float sx, float sy, float sz);

//
/// \brief multiply matrix specified by result with a translation matrix and return new matrix in result
/// \param result Specifies the input matrix.  Translated matrix is returned in result.
/// \param tx, ty, tz Scale factors along the x, y and z axes respectively
//
void ESUTIL_API esTranslate(ESMatrix4x4 *result, float tx, float ty, float tz);

//
/// \brief multiply matrix specified by result with a rotation matrix and return new matrix in result
/// \param result Specifies the input matrix.  Rotated matrix is returned in result.
/// \param angle Specifies the angle of rotation, in degrees.
/// \param x, y, z Specify the x, y and z coordinates of a vector, respectively
//
void ESUTIL_API esRotate(ESMatrix4x4 *result, float angle, float x, float y, float z);

//
// \brief multiply matrix specified by result with a perspective matrix and return new matrix in result
/// \param result Specifies the input matrix.  new matrix is returned in result.
/// \param left, right Coordinates for the left and right vertical clipping planes
/// \param bottom, top Coordinates for the bottom and top horizontal clipping planes
/// \param nearZ, farZ Distances to the near and far depth clipping planes.  Both distances must be positive.
//
void ESUTIL_API esFrustum(ESMatrix4x4 *result, float left, float right, float bottom, float top, float nearZ, float farZ);

//
/// \brief multiply matrix specified by result with a perspective matrix and return new matrix in result
/// \param result Specifies the input matrix.  new matrix is returned in result.
/// \param fovy Field of view y angle in degrees
/// \param aspect Aspect ratio of screen
/// \param nearZ Near plane distance
/// \param farZ Far plane distance
//
void ESUTIL_API esPerspective(ESMatrix4x4 *result, float fovy, float aspect, float nearZ, float farZ);

//
/// \brief multiply matrix specified by result with a perspective matrix and return new matrix in result
/// \param result Specifies the input matrix.  new matrix is returned in result.
/// \param left, right Coordinates for the left and right vertical clipping planes
/// \param bottom, top Coordinates for the bottom and top horizontal clipping planes
/// \param nearZ, farZ Distances to the near and far depth clipping planes.  These values are negative if plane is behind the viewer
//
void ESUTIL_API esOrtho(ESMatrix4x4 *result, float left, float right, float bottom, float top, float nearZ, float farZ);

//
/// \brief perform the following operation - result matrix = srcA matrix * srcB matrix
/// \param result Returns multiplied matrix
/// \param srcA, srcB Input matrices to be multiplied
//
void ESUTIL_API esMatrixMultiply(ESMatrix4x4 *result, ESMatrix4x4 *srcA, ESMatrix4x4 *srcB);


void ESUTIL_API esMatrixClipped(ESVec4 *result, ESMatrix4x4 *m, ESVec3 *v);

void ESUTIL_API esMatrixNDC(ESVec3 *result, ESVec4 *c);

void ESUTIL_API esMatrixWindow(ESVec2 *result, ESVec3 *NDC, int width, int height);

//
//// \brief return an indentity matrix
//// \param result returns identity matrix
//
void ESUTIL_API esMatrixLoadIdentity(ESMatrix4x4 *result);

#ifdef __cplusplus
}
#endif

#endif // ESUTIL_H
