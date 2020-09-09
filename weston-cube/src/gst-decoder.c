/*
 * Copyright (c) 2017 Rob Clark <rclark@redhat.com>
 * Copyright (c) 2017 Carlos Rafael Giani <dv@pseudoterminal.org>
 * Copyright (c) 2019 STMicroelectronics. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <assert.h>
#include <gbm.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cube-common.h"

#include <gbm.h>
#include <drm_fourcc.h>

#include <gst/gst.h>
#include <gst/gstmemory.h>
#include <gst/gstpad.h>
#include <gst/allocators/gstdmabuf.h>
#include <gst/app/gstappsink.h>
#include <gst/video/gstvideometa.h>

GST_DEBUG_CATEGORY_EXTERN(cube_video_debug);
#define GST_CAT_DEFAULT cube_video_debug

#define MAX_NUM_PLANES 3

inline static const char *
yesno(int yes)
{
	return yes ? "yes" : "no";
}

#if HAVE_GBM_BO_MAP
struct _gdm_bo_map {
	int             fd[MAX_NUM_PLANES];
	struct gbm_bo * bo[MAX_NUM_PLANES];
	void          * map[MAX_NUM_PLANES];
	void          * map_data[MAX_NUM_PLANES];
	unsigned int    fd_index;
};
#endif

struct decoder {
	GMainLoop          *loop;
	GstElement         *pipeline;
	GstElement         *sink;
	pthread_t           gst_thread;

	uint32_t            format;
	GstVideoInfo        info;

	const struct _gbm   *gbm;
#if HAVE_GBM_BO_MAP
	struct _gdm_bo_map  gdm_bo_map;
#endif
	const struct _egl   *egl;
	unsigned            frame;

	EGLImage            last_frame;
	GstSample          *last_samp;
};

static GstPadProbeReturn
pad_probe(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
	struct decoder *dec = user_data;
	GstEvent *event = GST_PAD_PROBE_INFO_EVENT(info);
	GstCaps *caps;

	(void)pad;

	if (GST_EVENT_TYPE(event) != GST_EVENT_CAPS)
		return GST_PAD_PROBE_OK;

	gst_event_parse_caps(event, &caps);

	if (!caps) {
		GST_ERROR("caps event without caps");
		return GST_PAD_PROBE_OK;
	}

	if (!gst_video_info_from_caps(&dec->info, caps)) {
		GST_ERROR("caps event with invalid video caps");
		return GST_PAD_PROBE_OK;
	}

	switch (GST_VIDEO_INFO_FORMAT(&(dec->info))) {
	case GST_VIDEO_FORMAT_I420:
		dec->format = DRM_FORMAT_YUV420;
		break;
	case GST_VIDEO_FORMAT_NV12:
		dec->format = DRM_FORMAT_NV12;
		break;
	case GST_VIDEO_FORMAT_YUY2:
		dec->format = DRM_FORMAT_YUYV;
		break;
	default:
		GST_ERROR("unknown format\n");
		return GST_PAD_PROBE_OK;
	}

	return GST_PAD_PROBE_OK;
}

static void *
gst_thread_func(void *args)
{
	struct decoder *dec = args;
	g_main_loop_run(dec->loop);
	return NULL;
}

static gboolean
bus_watch_cb(GstBus *bus, GstMessage *msg, gpointer user_data)
{
	struct decoder *dec = (struct decoder *)user_data;

	(void)bus;

	switch (GST_MESSAGE_TYPE(msg)) {
	case GST_MESSAGE_STATE_CHANGED: {
		gchar *dotfilename;
		GstState old_gst_state, cur_gst_state, pending_gst_state;

		/* Only consider state change messages coming from
		 * the toplevel element. */
		if (GST_MESSAGE_SRC(msg) != GST_OBJECT(dec->pipeline))
			break;

		gst_message_parse_state_changed(msg, &old_gst_state, &cur_gst_state, &pending_gst_state);

		printf(
			"GStreamer state change:  old: %s  current: %s  pending: %s\n",
			gst_element_state_get_name(old_gst_state),
			gst_element_state_get_name(cur_gst_state),
			gst_element_state_get_name(pending_gst_state)
		);

		dotfilename = g_strdup_printf(
			"statechange__old-%s__cur-%s__pending-%s",
			gst_element_state_get_name(old_gst_state),
			gst_element_state_get_name(cur_gst_state),
			gst_element_state_get_name(pending_gst_state)
		);
		GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(dec->pipeline), GST_DEBUG_GRAPH_SHOW_ALL, dotfilename);
		g_free(dotfilename);

		break;
	}
	case GST_MESSAGE_REQUEST_STATE: {
		GstState requested_state;
		gst_message_parse_request_state(msg, &requested_state);
		printf(
			"state change to %s was requested by %s\n",
			gst_element_state_get_name(requested_state),
			GST_MESSAGE_SRC_NAME(msg)
		);
		gst_element_set_state(GST_ELEMENT(dec->pipeline), requested_state);
		break;
	}
	case GST_MESSAGE_LATENCY: {
		printf("redistributing latency\n");
		gst_bin_recalculate_latency(GST_BIN(dec->pipeline));
		break;
	}
	case GST_MESSAGE_INFO:
	case GST_MESSAGE_WARNING:
	case GST_MESSAGE_ERROR: {
		GError *error = NULL;
		gchar *debug_info = NULL;
		gchar const *prefix;

		switch (GST_MESSAGE_TYPE(msg)) {
			case GST_MESSAGE_INFO:
				gst_message_parse_info(msg, &error, &debug_info);
				prefix = "INFO";
				break;
			case GST_MESSAGE_WARNING:
				gst_message_parse_warning(msg, &error, &debug_info);
				prefix = "WARNING";
				break;
			case GST_MESSAGE_ERROR:
				gst_message_parse_error(msg, &error, &debug_info);
				prefix = "ERROR";
				break;
			default:
				g_assert_not_reached();
		}
		printf("GStreamer %s: %s; debug info: %s", prefix, error->message, debug_info);

		g_clear_error(&error);
		g_free(debug_info);

		if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
			GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(dec->pipeline), GST_DEBUG_GRAPH_SHOW_ALL, "error");
		}

		// TODO: stop mainloop in case of an error

		break;
	}
	default:
		break;
	}

	return TRUE;
}

static GstPadProbeReturn
appsink_query_cb(GstPad *pad G_GNUC_UNUSED, GstPadProbeInfo *info,
	gpointer user_data G_GNUC_UNUSED)
{
	GstQuery *query = info->data;

	if (GST_QUERY_TYPE (query) != GST_QUERY_ALLOCATION)
	  return GST_PAD_PROBE_OK;

	gst_query_add_allocation_meta(query, GST_VIDEO_META_API_TYPE, NULL);

	return GST_PAD_PROBE_HANDLED;
}

struct decoder *
video_init(const struct _egl *egl, const struct _gbm *gbm, const char *filename,
	   char *fps)
{
	struct decoder *dec;
	GstElement *src;
	GstPad *pad;
	GstBus *bus;
	static const char *pipeline;

	if (egl_check(egl, eglCreateImageKHR) ||
	    egl_check(egl, eglDestroyImageKHR))
		return NULL;

	dec = calloc(1, sizeof(*dec));
	dec->loop = g_main_loop_new(NULL, FALSE);
	dec->gbm = gbm;
	dec->egl = egl;

#if HAVE_GBM_BO_MAP
	unsigned int i = 0;
	for (i = 0; i< MAX_NUM_PLANES; i++) {
		dec->gdm_bo_map.fd[i] = -1;
		dec->gdm_bo_map.bo[i] = NULL;
		dec->gdm_bo_map.map[i] = NULL;
		dec->gdm_bo_map.map_data[i] = NULL;
	}
	dec->gdm_bo_map.fd_index = 0;
#endif

	/* Setup pipeline: */
	if (strstr(filename, "/dev/video")) {
		char newpipeline[512];

		snprintf(newpipeline, sizeof(newpipeline),
			"v4l2src name=\"v4l2src\" device=%s ! "
			"image/jpeg,width=" CUBE_STR(CUBE_VID_TEX_WIDTH) ","
			"height=" CUBE_STR(CUBE_VID_TEX_HEIGTH) ","
			"framerate=(fraction)%s ! "
			"jpegdec name=\"decode\" ! video/x-raw ! "
			"appsink sync=false name=\"sink\"",
			filename, fps
			);
		pipeline = newpipeline;
	} else {
		pipeline =
			"filesrc name=\"src\" ! decodebin name=\"decode\" ! "
			"video/x-raw ! appsink sync=false name=\"sink\"";

	}
	printf("GST Pipeline: %s", pipeline);

	dec->pipeline = gst_parse_launch(pipeline, NULL);

	dec->sink = gst_bin_get_by_name(GST_BIN(dec->pipeline), "sink");

	/* Implement the allocation query using a pad probe. This probe will
	 * adverstize support for GstVideoMeta, which avoid hardware accelerated
	 * decoder that produce special strides and offsets from having to
	 * copy the buffers.
	 */
	pad = gst_element_get_static_pad(dec->sink, "sink");
	gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_QUERY_DOWNSTREAM,
		appsink_query_cb, NULL, NULL);
	gst_object_unref(pad);

	src = gst_bin_get_by_name(GST_BIN(dec->pipeline), "src");
	if (src) {
		g_object_set(G_OBJECT(src), "location", filename, NULL);
		gst_object_unref(src);
	}

	/* Configure the sink like a video sink (mimic GstVideoSink) */
	gst_base_sink_set_max_lateness(GST_BASE_SINK(dec->sink), 20 * GST_MSECOND);
	gst_base_sink_set_qos_enabled(GST_BASE_SINK(dec->sink), TRUE);

	/* if we don't limit max-buffers then we can let the decoder outrun
	 * vsync and quickly chew up 100's of MB of buffers:
	 */
	g_object_set(G_OBJECT(dec->sink), "max-buffers", 2, NULL);

	gst_pad_add_probe(gst_element_get_static_pad(dec->sink, "sink"),
			GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
			pad_probe, dec, NULL);

	/* add bus to be able to receive error message, handle latency
	 * requests, produce pipeline dumps, etc. */
	bus = gst_pipeline_get_bus(GST_PIPELINE(dec->pipeline));
	gst_bus_add_watch(bus, bus_watch_cb, dec);
	gst_object_unref(GST_OBJECT(bus));

	/* let 'er rip! */
	gst_element_set_state(dec->pipeline, GST_STATE_PLAYING);

	pthread_create(&dec->gst_thread, NULL, gst_thread_func, dec);

	return dec;
}

static void
set_last_frame(struct decoder *dec, EGLImage frame, GstSample *samp)
{
	if (dec->last_frame)
		dec->egl->eglDestroyImageKHR(dec->egl->dpy, dec->last_frame);
	dec->last_frame = frame;
	if (dec->last_samp)
		gst_sample_unref(dec->last_samp);
	dec->last_samp = samp;
}

#if HAVE_GBM_BO_MAP
static int
buf_to_fd(struct decoder *dec, int size, void *ptr)
{
	const struct _gbm *gbm = dec->gbm;
	struct _gdm_bo_map *bo_map = &dec->gdm_bo_map;
	unsigned int i = 0;

	if (bo_map->fd_index >= MAX_NUM_PLANES)
		bo_map->fd_index  = 0;

	i = bo_map->fd_index;
	if (bo_map->map[i] == NULL) {
		struct gbm_bo *bo = NULL;
		void *map, *map_data;
		uint32_t stride;
		static int fd;

		/* NOTE: do not actually use GBM_BO_USE_WRITE since that gets us a dumb buffer: */

		bo = gbm_bo_create(gbm->dev, size, 1, GBM_FORMAT_R8,
				   GBM_BO_USE_LINEAR);

		map = gbm_bo_map(bo, 0, 0, size, 1, GBM_BO_TRANSFER_WRITE,
				 &stride, &map_data);

		fd = gbm_bo_get_fd(bo);

		bo_map->map[i] = map;
		bo_map->map_data[i] = map_data;
		bo_map->fd[i] = fd;
		bo_map->bo[i] = bo;

		printf("Create [%d] +-> bo     = %p\n", i, bo);
		printf("            |-> fd     = %i\n", fd);
		printf("            |-> size   = %i\n", size);
		printf("            |-> stride = %i\n", stride);
	}

	memcpy(bo_map->map[i], ptr, size);

	return bo_map->fd[bo_map->fd_index++];
}
#endif

static EGLImage
buffer_to_image(struct decoder *dec, GstBuffer *buf)
{
	struct { int fd, offset, stride; } planes[MAX_NUM_PLANES];
	GstVideoMeta *meta = gst_buffer_get_video_meta(buf);
	EGLImage image;
	guint nmems = gst_buffer_n_memory(buf);
	guint nplanes = GST_VIDEO_INFO_N_PLANES(&(dec->info));
	guint i;
	guint width, height;
	gboolean is_dmabuf_mem;
	GstMemory *mem;
	int dmabuf_fd = -1;

	static const EGLint egl_dmabuf_plane_fd_attr[MAX_NUM_PLANES] = {
		EGL_DMA_BUF_PLANE0_FD_EXT,
		EGL_DMA_BUF_PLANE1_FD_EXT,
		EGL_DMA_BUF_PLANE2_FD_EXT,
	};
	static const EGLint egl_dmabuf_plane_offset_attr[MAX_NUM_PLANES] = {
		EGL_DMA_BUF_PLANE0_OFFSET_EXT,
		EGL_DMA_BUF_PLANE1_OFFSET_EXT,
		EGL_DMA_BUF_PLANE2_OFFSET_EXT,
	};
	static const EGLint egl_dmabuf_plane_pitch_attr[MAX_NUM_PLANES] = {
		EGL_DMA_BUF_PLANE0_PITCH_EXT,
		EGL_DMA_BUF_PLANE1_PITCH_EXT,
		EGL_DMA_BUF_PLANE2_PITCH_EXT,
	};

	/* Query gst_is_dmabuf_memory() here, since the gstmemory
	 * block might get merged below by gst_buffer_map(), meaning
	 * that the mem pointer would become invalid */
	mem = gst_buffer_peek_memory(buf, 0);
	is_dmabuf_mem = gst_is_dmabuf_memory(mem);

	if (nmems > 1) {
		if (is_dmabuf_mem) {
			/* this case currently is not defined */

			GST_FIXME("gstbuffers with multiple memory blocks and DMABUF "
			          "memory currently are not supported");
			return EGL_NO_IMAGE_KHR;
		}

		/* if this is not DMABUF memory, then the gst_buffer_map()
		 * call below will automatically merge the memory blocks
		 */
	}

	if (is_dmabuf_mem) {
		dmabuf_fd = dup(gst_dmabuf_memory_get_fd(mem));
	}
#if HAVE_GBM_BO_MAP
	else {
		GstMapInfo map_info;
		gst_buffer_map(buf, &map_info, GST_MAP_READ);
		dmabuf_fd = buf_to_fd(dec, map_info.size, map_info.data);
		gst_buffer_unmap(buf, &map_info);
	}
#endif

	if (dmabuf_fd < 0) {
		GST_ERROR("could not obtain DMABUF FD");
		return EGL_NO_IMAGE_KHR;
	}

	/* Usually, a videometa should be present, since by using the internal kmscube
	 * video_appsink element instead of the regular appsink, it is guaranteed that
	 * video meta support is declared in the video_appsink's allocation query.
	 * However, this assumes that upstream elements actually look at the allocation
	 * query's contents properly, or that they even send a query at all. If this
	 * is not the case, then upstream might decide to push frames without adding
	 * a meta. It can happen, and in this case, look at the video info data as
	 * a fallback (it is computed out of the input caps).
	 */
	if (meta) {
		for (i = 0; i < nplanes; i++) {
			planes[i].fd = dmabuf_fd;
			planes[i].offset = meta->offset[i];
			planes[i].stride = meta->stride[i];
		}
	} else {
		for (i = 0; i < nplanes; i++) {
			planes[i].fd = dmabuf_fd;
			planes[i].offset = GST_VIDEO_INFO_PLANE_OFFSET(&(dec->info), i);
			planes[i].stride = GST_VIDEO_INFO_PLANE_STRIDE(&(dec->info), i);
		}
	}

	width = GST_VIDEO_INFO_WIDTH(&(dec->info));
	height = GST_VIDEO_INFO_HEIGHT(&(dec->info));

	/* output some information at the beginning (= when the first frame is handled) */
	if (dec->frame == 0) {
		GstVideoFormat pixfmt;
		const char *pixfmt_str;

		pixfmt = GST_VIDEO_INFO_FORMAT(&(dec->info));
		pixfmt_str = gst_video_format_to_string(pixfmt);

		printf("===================================\n");
		printf("GStreamer video stream information:\n");
		printf("  size: %u x %u pixel\n", width, height);
		printf("  pixel format: %s  number of planes: %u\n", pixfmt_str, nplanes);
		printf("  can use zero-copy: %s\n", yesno(is_dmabuf_mem));
		printf("  video meta found: %s\n", yesno(meta != NULL));
		printf("===================================\n");
	}

	{
		/* Initialize the first 6 attributes with values that are
		 * plane invariant (width, height, format) */
		EGLint attr[6 + 6*(MAX_NUM_PLANES) + 1] = {
			EGL_WIDTH, width,
			EGL_HEIGHT, height,
			EGL_LINUX_DRM_FOURCC_EXT, dec->format
		};

		for (i = 0; i < nplanes; i++) {
			attr[6 + 6*i + 0] = egl_dmabuf_plane_fd_attr[i];
			attr[6 + 6*i + 1] = planes[i].fd;
			attr[6 + 6*i + 2] = egl_dmabuf_plane_offset_attr[i];
			attr[6 + 6*i + 3] = planes[i].offset;
			attr[6 + 6*i + 4] = egl_dmabuf_plane_pitch_attr[i];
			attr[6 + 6*i + 5] = planes[i].stride;
		}

		attr[6 + 6*nplanes] = EGL_NONE;

		image = dec->egl->eglCreateImageKHR(dec->egl->dpy, EGL_NO_CONTEXT,
				EGL_LINUX_DMA_BUF_EXT, NULL, attr);
	}

#if !defined(HAVE_GBM_BO_MAP) || (HAVE_GBM_BO_MAP == 0)
	/* Cleanup */
	for (unsigned i = 0; i < nmems; i++)
		close(planes[i].fd);
#endif

	return image;
}

EGLImage
video_frame(struct decoder *dec)
{
	GstSample *samp;
	GstBuffer *buf;
	EGLImage   frame = NULL;

	samp = gst_app_sink_pull_sample(GST_APP_SINK(dec->sink));
	if (!samp) {
		GST_DEBUG("got no appsink sample");
		return NULL;
	}

	buf = gst_sample_get_buffer(samp);

	// TODO inline buffer_to_image??
	frame = buffer_to_image(dec, buf);

	// TODO in the zero-copy dmabuf case it would be nice to associate
	// the eglimg w/ the buffer to avoid recreating it every frame..

	set_last_frame(dec, frame, samp);

	dec->frame++;

	return frame;
}

void video_deinit(struct decoder *dec)
{
#if HAVE_GBM_BO_MAP
	unsigned int i;
	for (i = 0; i< MAX_NUM_PLANES; i++) {
		if (dec->gdm_bo_map.map[i] != NULL) {
			close(dec->gdm_bo_map.fd[i]);

			gbm_bo_unmap(dec->gdm_bo_map.bo[i],
				     dec->gdm_bo_map.map_data[i]);
			
			gbm_bo_destroy(dec->gdm_bo_map.bo[i]);

		}

		dec->gdm_bo_map.fd[i] = -1;
		dec->gdm_bo_map.bo[i] = NULL;
		dec->gdm_bo_map.map[i] = NULL;
		dec->gdm_bo_map.map_data[i] = NULL;
	}
	dec->gdm_bo_map.fd_index = 0;
#endif

	set_last_frame(dec, NULL, NULL);
	gst_element_set_state(dec->pipeline, GST_STATE_NULL);
	gst_object_unref(dec->sink);
	gst_object_unref(dec->pipeline);
	g_main_loop_quit(dec->loop);
	g_main_loop_unref(dec->loop);
	pthread_join(dec->gst_thread, 0);

	free(dec);
}
