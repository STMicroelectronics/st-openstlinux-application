/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (C) 2024 ST Microelectronics.
 */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/media.h>
#include "v4l2-controls.h"
#include "videodev2.h"
#include <linux/v4l2-subdev.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include "stm32-dcmipp-config.h"

#define STR_MAX_LEN	32
struct isp_descriptor {
	char media_dev_name[STR_MAX_LEN];
	char isp_subdev_name[STR_MAX_LEN];
	char params_dev_name[STR_MAX_LEN];
	char stat_dev_name[STR_MAX_LEN];
	char sensor_subdev_name[STR_MAX_LEN];
	int isp_fd;
	int params_fd;
	int stat_fd;
	int width;
	int height;
	int fmt;
	char fmt_str[STR_MAX_LEN];
	struct stm32_dcmipp_stat_buf *stats[4];
	int stats_buf_nb;
	size_t stats_buf_len;
	struct stm32_dcmipp_params_cfg *params[4];
	int params_buf_nb;
	size_t params_buf_len;
};

/*
 * Compute luminance value from R/G/B components
 * using the BT 601 coefficients
 */
static int luminance_from_rgb(int rgb[3])
{
	/* BT 601 coefficients */
	return rgb[0] * 0.299 + rgb[1] * 0.587 + rgb[2] * 0.114;
}

/*
 * Clamp a value within a range
 */
static int clamp(int val, int lo, int hi)
{
	if (val < lo)
		return lo;
	else if (val > hi)
		return hi;
	else
		return val;
}

/*
 * Search for the device file name of the DCMIPP media device
 */
#define DCMIPP_DRV	"dcmipp"
static int find_media(struct isp_descriptor *isp_desc)
{
	struct media_device_info info;
	char media_dev_name[STR_MAX_LEN];
	int ret, fd, i;

	for (i = 0; i < 255; i++) {
		/* Search for the dcmipp media */
		snprintf(media_dev_name, STR_MAX_LEN, "/dev/media%d", i);
		fd = open(media_dev_name, O_RDWR);
		if (fd < 0)
			continue;

		ret = ioctl(fd, MEDIA_IOC_DEVICE_INFO, &info);
		close(fd);

		if (!ret && !strcmp(info.driver, DCMIPP_DRV)) {
			/* dcmipp found */
			strncpy(isp_desc->media_dev_name, media_dev_name, STR_MAX_LEN);
			return 0;
		}
	}

	printf("Can't find media device\n");
	return -ENXIO;
}

/*
 * Search for the device file name of a DCMIPP video device for stats
 */
static int find_video_dev(char *media_dev, char *v4l2_name, char *video_dev)
{
	struct media_entity_desc info;
	struct stat devstat;
	char dev_name[STR_MAX_LEN];
	int fd, i, ret;
	__u32 id;

	fd = open(media_dev, O_RDWR);
	if (fd < 0)
		return fd;

	for (id = 0, ret = 0; ; id = info.id) {
		/* Search across all the entities */
		info.id = id | MEDIA_ENT_ID_FLAG_NEXT;
		ret = ioctl(fd, MEDIA_IOC_ENUM_ENTITIES, &info);
		if (ret < 0)
			break;

		if (!strcmp(info.name, v4l2_name)) {
			/* isp entity found. Now search for its /dev/v4l-subdevx */
			for (i = 0; i < 255; i++) {
				/* check for a sub dev that matches the major/minor */
				snprintf(dev_name, STR_MAX_LEN, "/dev/video%d", i);
				if (stat(dev_name, &devstat) < 0)
					continue;

				if ((major(devstat.st_rdev) == info.dev.major) &&
				    (minor(devstat.st_rdev) == info.dev.minor)) {
					/* found */
					strncpy(video_dev, dev_name, STR_MAX_LEN);
					close(fd);
					return 0;
				}
			}
		}
	}

	close(fd);
	printf("Can't find video device\n");
	return -ENXIO;
}

/*
 * Search for the device file name of a DCMIPP ISP subdevice
 */
#define DCMIPP_ISP_NAME		"dcmipp_main_isp"
static int find_isp_subdev(struct isp_descriptor *isp_desc)
{
	struct media_entity_desc info;
	struct stat devstat;
	char dev_name[STR_MAX_LEN];
	int fd, i, ret;
	__u32 id;

	fd = open(isp_desc->media_dev_name, O_RDWR);
	if (fd < 0)
		return fd;

	for (id = 0, ret = 0; ; id = info.id) {
		/* Search across all the entities */
		info.id = id | MEDIA_ENT_ID_FLAG_NEXT;
		ret = ioctl(fd, MEDIA_IOC_ENUM_ENTITIES, &info);
		if (ret < 0)
			break;

		if (!strcmp(info.name, DCMIPP_ISP_NAME)) {
			/* isp entity found. Now search for its /dev/v4l-subdevx */
			for (i = 0; i < 255; i++) {
				/* check for a sub dev that matches the major/minor */
				snprintf(dev_name, STR_MAX_LEN, "/dev/v4l-subdev%d", i);
				if (stat(dev_name, &devstat) < 0)
					continue;

				if ((major(devstat.st_rdev) == info.dev.major) &&
				    (minor(devstat.st_rdev) == info.dev.minor)) {
					/* found */
					strncpy(isp_desc->isp_subdev_name, dev_name, STR_MAX_LEN);
					close(fd);
					return 0;
				}
			}
		}
	}

	close(fd);
	printf("Can't find isp subdev\n");
	return -ENXIO;
}

/*
 * Search for the device file name of the sensor subdevice
 */
#define SENSOR_DRV_NAME		"imx335"
static int find_sensor_subdev(struct isp_descriptor *isp_desc)
{
	struct media_entity_desc info;
	struct stat devstat;
	char dev_name[STR_MAX_LEN];
	int fd, i, ret;
	__u32 id;

	fd = open(isp_desc->media_dev_name, O_RDWR);
	if (fd < 0)
		return fd;

	for (id = 0, ret = 0; ; id = info.id) {
		/* Search across all the entities */
		info.id = id | MEDIA_ENT_ID_FLAG_NEXT;
		ret = ioctl(fd, MEDIA_IOC_ENUM_ENTITIES, &info);
		if (ret < 0)
			break;

		if (!strncmp(info.name, SENSOR_DRV_NAME, strlen(SENSOR_DRV_NAME))) {
			/* entity found. Now search for its /dev/v4l-subdevx */
			for (i = 0; i < 255; i++) {
				/* check for a sub dev that matches the major/minor */
				snprintf(dev_name, STR_MAX_LEN, "/dev/v4l-subdev%d", i);
				if (stat(dev_name, &devstat) < 0)
					continue;

				if ((major(devstat.st_rdev) == info.dev.major) &&
				    (minor(devstat.st_rdev) == info.dev.minor)) {
					/* found */
					strncpy(isp_desc->sensor_subdev_name, dev_name, STR_MAX_LEN);
					close(fd);
					return 0;
				}
			}
		}
	}

	close(fd);
	printf("Can't find sensor subdev\n");
	return -ENXIO;
}

/*
 * Open and initialize the stats capture video device
 */
static int open_stats_vdev(struct isp_descriptor *isp_desc)
{
	struct v4l2_requestbuffers req;
	struct v4l2_capability cap;
	struct v4l2_format fmt;
	struct v4l2_buffer buf;
	int i, ret;

	isp_desc->stat_fd = open(isp_desc->stat_dev_name, O_RDWR);
	if (isp_desc->stat_fd == -1) {
		ret = -errno;
		printf("Failed to open video device %s\n", isp_desc->stat_dev_name);
		return ret;
	}

	ret = ioctl(isp_desc->stat_fd, VIDIOC_QUERYCAP, &cap);
	if (ret) {
		printf("Failed to query cap\n");
		return ret;
	}

	if ((!(cap.capabilities & V4L2_CAP_META_CAPTURE)) || (!(cap.capabilities & V4L2_CAP_STREAMING))) {
		printf("Incorrect capabilities (%x)\n", cap.capabilities);
		return -ENXIO;
	}

	fmt.type = V4L2_BUF_TYPE_META_CAPTURE;
	ret = ioctl(isp_desc->stat_fd, VIDIOC_G_FMT, &fmt);
	if (ret) {
		printf("Failed to get format\n");
		return ret;
	}

	if (fmt.fmt.meta.dataformat != V4L2_META_FMT_ST_DCMIPP_ISP_STAT) {
		printf("Invalid format (%x)\n", fmt.fmt.meta.dataformat);
		return -ENXIO;
	}

	/* Get one meta buffer */
	req.count = 1;
	req.type = V4L2_BUF_TYPE_META_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;
	ret = ioctl(isp_desc->stat_fd, VIDIOC_REQBUFS, &req);
	if (ret) {
		printf("Failed to request buffers\n");
		return ret;
	}

	isp_desc->stats_buf_nb = req.count;

	for (i = 0; i < req.count; i++) {
		buf.type = V4L2_BUF_TYPE_META_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		ret = ioctl(isp_desc->stat_fd, VIDIOC_QUERYBUF, &buf);
		if (ret) {
			printf("Failed to query buffer %d\n", i);
			return ret;
		}
		isp_desc->stats_buf_len = buf.length;

		isp_desc->stats[i] = mmap(NULL, isp_desc->stats_buf_len, PROT_READ | PROT_WRITE, MAP_SHARED, isp_desc->stat_fd, buf.m.offset);
		if (isp_desc->stats[i] == MAP_FAILED) {
			printf("Failed to map buffer %d\n", i);
			return -ENOMEM;
		}
	}

	return 0;
}

/*
 * Close the stats video device
 */
static void close_stats_vdev(struct isp_descriptor *isp_desc)
{
	int i;

	for (i = 0; i < isp_desc->stats_buf_nb; i++)
		munmap(isp_desc->stats[i], isp_desc->stats_buf_len);

	close(isp_desc->stat_fd);
}

/*
 * Open and initialize the params video output device
 */
static int open_params_vdev(struct isp_descriptor *isp_desc)
{
	struct v4l2_requestbuffers req;
	struct v4l2_capability cap;
	struct v4l2_format fmt;
	struct v4l2_buffer buf;
	int i, ret;

	isp_desc->params_fd = open(isp_desc->params_dev_name, O_RDWR);
	if (isp_desc->params_fd == -1) {
		ret = -errno;
		printf("Failed to open video device %s\n", isp_desc->params_dev_name);
		return ret;
	}

	ret = ioctl(isp_desc->params_fd, VIDIOC_QUERYCAP, &cap);
	if (ret) {
		printf("Failed to query cap\n");
		return ret;
	}

	if ((!(cap.capabilities & V4L2_CAP_META_OUTPUT)) || (!(cap.capabilities & V4L2_CAP_STREAMING))) {
		printf("Incorrect capabilities (%x)\n", cap.capabilities);
		return -ENXIO;
	}

	fmt.type = V4L2_BUF_TYPE_META_OUTPUT;
	ret = ioctl(isp_desc->params_fd, VIDIOC_G_FMT, &fmt);
	if (ret) {
		printf("Failed to get format\n");
		return ret;
	}

	if (fmt.fmt.meta.dataformat != V4L2_META_FMT_ST_DCMIPP_ISP_PARAMS) {
		printf("Invalid format (%x)\n", fmt.fmt.meta.dataformat);
		return -ENXIO;
	}

	/* Get one meta buffer */
	req.count = 1;
	req.type = V4L2_BUF_TYPE_META_OUTPUT;
	req.memory = V4L2_MEMORY_MMAP;
	ret = ioctl(isp_desc->params_fd, VIDIOC_REQBUFS, &req);
	if (ret) {
		printf("Failed to request buffers\n");
		return ret;
	}

	isp_desc->params_buf_nb = req.count;

	for (i = 0; i < req.count; i++) {
		buf.type = V4L2_BUF_TYPE_META_OUTPUT;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		ret = ioctl(isp_desc->params_fd, VIDIOC_QUERYBUF, &buf);
		if (ret) {
			printf("Failed to query buffer %d\n", i);
			return ret;
		}
		isp_desc->params_buf_len = buf.length;

		isp_desc->params[i] = mmap(NULL, isp_desc->params_buf_len, PROT_READ | PROT_WRITE, MAP_SHARED, isp_desc->params_fd, buf.m.offset);
		if (isp_desc->params[i] == MAP_FAILED) {
			printf("Failed to map buffer %d\n", i);
			return -ENOMEM;
		}
	}

	return 0;

}

/*
 * Close the params video output device
 */
static void close_params_vdev(struct isp_descriptor *isp_desc)
{
	int i;

	for (i = 0; i < isp_desc->params_buf_nb; i++)
		munmap(isp_desc->params[i], isp_desc->params_buf_len);

	close(isp_desc->params_fd);
}

static void to_fmt_str(char *fmt_str, int fmt)
{
	if (fmt >= MEDIA_BUS_FMT_SBGGR8_1X8 && fmt <= MEDIA_BUS_FMT_SRGGB16_1X16)
		strncpy(fmt_str, "Raw Bayer", STR_MAX_LEN);
	else if (fmt == MEDIA_BUS_FMT_RGB565_2X8_LE)
		strncpy(fmt_str, "RGB565", STR_MAX_LEN);
	else if (fmt == MEDIA_BUS_FMT_RGB888_1X24)
		strncpy(fmt_str, "RGB888", STR_MAX_LEN);
	else if (fmt == MEDIA_BUS_FMT_YUV8_1X24)
		strncpy(fmt_str, "YUV 420", STR_MAX_LEN);
	else if (fmt >= MEDIA_BUS_FMT_UYVY8_2X8 && fmt <= MEDIA_BUS_FMT_YVYU8_2X8)
		strncpy(fmt_str, "YUV 422", STR_MAX_LEN);
	else
		snprintf(fmt_str, STR_MAX_LEN, "Format = 0x%x", fmt);
}

#define DCMIPP_ISP_PARAMS_NAME		"dcmipp_main_isp_params_output"
#define DCMIPP_ISP_STAT_NAME		"dcmipp_main_isp_stat_capture"
/*
 * Search, open and initialize all DCMIPP devices
 */
static int discover_dcmipp(struct isp_descriptor *isp_desc)
{
	struct v4l2_subdev_selection sel;
	struct v4l2_subdev_format fmt;
	int ret;

	/* find dcmipp media dev */
	ret = find_media(isp_desc);
	if (ret)
		return ret;

	/* find isp sub dev */
	ret = find_isp_subdev(isp_desc);
	if (ret)
		return ret;

	/* find params video dev */
	ret = find_video_dev(isp_desc->media_dev_name, DCMIPP_ISP_PARAMS_NAME,
			     isp_desc->params_dev_name);
	if (ret)
		return ret;

	/* find stat video dev */
	ret = find_video_dev(isp_desc->media_dev_name, DCMIPP_ISP_STAT_NAME,
			     isp_desc->stat_dev_name);
	if (ret)
		return ret;

	/* find sensor sub dev */
	ret = find_sensor_subdev(isp_desc);
	if (ret)
		return ret;

	/* Get input frame params */
	isp_desc->isp_fd = open(isp_desc->isp_subdev_name, O_RDWR);
	if (isp_desc->isp_fd == -1) {
		ret = -errno;
		printf("Failed to open isp subdev %s\n", isp_desc->isp_subdev_name);
		return ret;
	}

	fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	fmt.pad = 0;
	ret = ioctl(isp_desc->isp_fd, VIDIOC_SUBDEV_G_FMT, &fmt);
	if (ret) {
		printf("Failed to get format\n");
		return ret;
	}
	isp_desc->fmt = fmt.format.code;
	to_fmt_str(isp_desc->fmt_str, isp_desc->fmt);

	sel.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	sel.pad = 0;
	sel.target = V4L2_SEL_TGT_COMPOSE;
	ret = ioctl(isp_desc->isp_fd, VIDIOC_SUBDEV_G_SELECTION, &sel);
	if (ret) {
		printf("Failed to get selection\n");
		return ret;
	}
	isp_desc->width = sel.r.width;
	isp_desc->height = sel.r.height;

	ret = open_params_vdev(isp_desc);
	if (ret)
		return ret;

	ret = open_stats_vdev(isp_desc);
	if (ret)
		return ret;

	return 0;
}

static int get_ctrl(int fd, int id, int *value)
{
	struct v4l2_control ctrl;
	int ret;

	memset(&ctrl, 0, sizeof(struct v4l2_control));
	ctrl.id = id;

	ret = ioctl(fd, VIDIOC_G_CTRL, &ctrl);
	if (ret)
		printf("VIDIOC_S_CTRL setting failed\n");

	*value = ctrl.value;
	return ret;
}

/*
 * Helpers to access V4L2 controls
 */
static int set_ctrl(int fd, int id, int value)
{
	struct v4l2_control ctrl;
	int ret;

	memset(&ctrl, 0, sizeof(struct v4l2_control));
	ctrl.id = id;
	ctrl.value = value;

	ret = ioctl(fd, VIDIOC_S_CTRL, &ctrl);
	if (ret)
		printf("VIDIOC_S_CTRL setting failed\n");

	return ret;
}

static int set_ext_ctrl(int fd, struct v4l2_ext_controls *extCtrls)
{
	int ret;

	ret = ioctl(fd, VIDIOC_S_EXT_CTRLS, extCtrls);
	if (ret)
		printf("VIDIOC_S_EXT_CTRLS setting failed (CID %x)\n", extCtrls->controls->id);

	return ret;
}

static int set_ext_ctrl_int(int fd, unsigned int class, int v4l2_cid, int value)
{
	struct v4l2_ext_controls extCtrls;
	struct v4l2_ext_control extCtrl;

	memset(&extCtrl, 0, sizeof(struct v4l2_ext_control));
	extCtrl.id = v4l2_cid;
	extCtrl.value = value;

	extCtrls.controls = &extCtrl;
	extCtrls.count = 1;
	extCtrls.ctrl_class = class;

	return set_ext_ctrl(fd, &extCtrls);
}

static int get_ext_ctrl_int(int fd, unsigned int class, int v4l2_cid, int *value)
{
	struct v4l2_ext_controls extCtrls;
	struct v4l2_ext_control extCtrl;
	int ret;

	memset(&extCtrl, 0, sizeof(struct v4l2_ext_control));
	extCtrl.id = v4l2_cid;

	extCtrls.controls = &extCtrl;
	extCtrls.count = 1;
	extCtrls.ctrl_class = class;

	ret = ioctl(fd, VIDIOC_G_EXT_CTRLS, &extCtrls);
	if (ret)
		printf("VIDIOC_G_EXT_CTRLS setting failed (CID %x)\n", extCtrls.controls->id);

	*value = extCtrl.value;

	return ret;
}

/*
 * Apply DCMIPP ISP params
 */
static int apply_params(struct isp_descriptor *isp_desc, struct stm32_dcmipp_params_cfg *params)
{
	enum v4l2_buf_type type;
	struct v4l2_format fmt;
	struct v4l2_buffer buf;
	struct timeval tv;
	size_t buflen = 0;
	fd_set fds;
	int ret;

	*isp_desc->params[0] = *params;

	/* Queue the buffer */
	buf.type = V4L2_BUF_TYPE_META_OUTPUT;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = 0;
	buf.bytesused = sizeof(*params);

	ret = ioctl(isp_desc->params_fd, VIDIOC_QBUF, &buf);
	if (ret) {
		printf("Failed to queue buffer\n");
		return ret;
	}

	/* Start stream */
	type = V4L2_BUF_TYPE_META_OUTPUT;
	ret = ioctl(isp_desc->params_fd, VIDIOC_STREAMON, &type);
	if (ret) {
		printf("Failed to start stream\n");
		return ret;
	}

	FD_ZERO(&fds);
	FD_SET(isp_desc->params_fd, &fds);
	tv.tv_sec = 2;
	tv.tv_usec = 0;

	/* Wait for buff */
	ret = select(isp_desc->params_fd + 1, NULL, &fds, NULL, &tv);
	if (ret < 0) {
		printf("Select failed (%d)\n", ret);
		return ret;
	}
	if (ret == 0) {
		printf("Select timeout\n");
		return -EBUSY;
	}

	/* Get the buff back, indicating the sequence into which it has been pushed */
	buf.type = V4L2_BUF_TYPE_META_OUTPUT;
	buf.memory = V4L2_MEMORY_MMAP;
	ret = ioctl(isp_desc->params_fd, VIDIOC_DQBUF, &buf);
	if (ret) {
		printf("Failed to dequeue buffer\n");
		return ret;
	}

	/* Stop stream */
	type = V4L2_BUF_TYPE_META_OUTPUT;
	ret = ioctl(isp_desc->params_fd, VIDIOC_STREAMOFF, &type);
	if (ret) {
		printf("Failed to stop stream\n");
		return ret;
	}

	return ret;
}

/*
 * Helper function to configure the stats video capture device and set the stats capture profile
 */
static int set_stat_profile(struct isp_descriptor *isp_desc, enum v4l2_isp_stat_profile profile)
{
	int ret;

	if (profile > V4L2_STAT_PROFILE_AVERAGE_POST) {
		printf("Invalid value : %d\n", profile);
		return -EINVAL;
	}

	ret = set_ext_ctrl_int(isp_desc->stat_fd, V4L2_CTRL_CLASS_IMAGE_PROC, V4L2_CID_ISP_STAT_PROFILE, profile);
	if (ret)
		printf("Failed to apply Stat capture profile\n");

	return ret;
}

/*
 * Helpers for statistics display
 */
static void print_range(int min, int max, int val, int nb_pix, char histo[20][STR_MAX_LEN])
{
	printf("    [%3d:%3d]   %7d\t%2d%%   %s\n", min, max, val, 100 * val / nb_pix, histo[clamp(20 * val / nb_pix, 0, 19)]);
}

static void print_average(__u32 average_RGB[3])
{
	printf("Average:\n");
	printf("    Red             %d\n", average_RGB[0]);
	printf("    Green           %d\n", average_RGB[1]);
	printf("    Red             %d\n", average_RGB[2]);
	printf("    Lum             %d\n", luminance_from_rgb(average_RGB));
}

static void print_bins(__u32 bins[12])
{
	int range0, range4, range8, range16, range32, range64;
	int range128, range192, range224, range240, range248, range252;
	char histo[20][STR_MAX_LEN];
	int nb_pix, i;

	for (i = 0; i < 20; i++) {
		strncpy(histo[i], "--------------------", STR_MAX_LEN);
		histo[i][i + 1] = '\0';
	}

	range0 = bins[0];
	range4 = bins[1] - bins[0];
	range8 = bins[2] - bins[1];
	range16 = bins[3] - bins[2];
	range32 = bins[4] - bins[3];
	range64 = bins[5] - bins[4];
	range128 = bins[6] - bins[7];
	range192 = bins[7] - bins[8];
	range224 = bins[8] - bins[9];
	range240 = bins[9] - bins[10];
	range248 = bins[10] - bins[11];
	range252 = bins[11];
	nb_pix = bins[5] + bins[6];

	printf("\nHistogram (bins):\n");
	printf("    <    4      %7d\n", bins[0]);
	printf("    <    8      %7d\n", bins[1]);
	printf("    <   16      %7d\n", bins[2]);
	printf("    <   32      %7d\n", bins[3]);
	printf("    <   64      %7d\n", bins[4]);
	printf("    <  128      %7d\n", bins[5]);
	printf("    >= 128      %7d\n", bins[6]);
	printf("    >= 192      %7d\n", bins[7]);
	printf("    >= 224      %7d\n", bins[8]);
	printf("    >= 240      %7d\n", bins[9]);
	printf("    >= 248      %7d\n", bins[10]);
	printf("    >= 252      %7d\n", bins[11]);

	printf("\nHistogram (range):\n");
	print_range(0, 3, range0, nb_pix, histo);
	print_range(4, 7, range4, nb_pix, histo);
	print_range(8, 15, range8, nb_pix, histo);
	print_range(16, 31, range16, nb_pix, histo);
	print_range(32, 63, range32, nb_pix, histo);
	print_range(64, 127, range64, nb_pix, histo);
	print_range(128, 191, range128, nb_pix, histo);
	print_range(192, 223, range192, nb_pix, histo);
	print_range(224, 239, range224, nb_pix, histo);
	print_range(240, 247, range240, nb_pix, histo);
	print_range(248, 251, range248, nb_pix, histo);
	print_range(252, 255, range252, nb_pix, histo);
}

/*
 * Read stats from the DCMIPP ISP
 */
static int get_stat(struct isp_descriptor *isp_desc, bool loop, struct stm32_dcmipp_stat_buf **stats, enum v4l2_isp_stat_profile profile)
{
	enum v4l2_buf_type type;
	struct v4l2_format fmt;
	struct v4l2_buffer buf;
	struct timeval tv;
	size_t buflen = 0;
	fd_set fds;
	int i, ret;

	/* Set the stat profile */
	ret = set_stat_profile(isp_desc, profile);
	if (ret) {
		printf("Failed to set stat capture profile\n");
		return ret;
	}

	/* Queue buff */
	for (i = 0; i < isp_desc->stats_buf_nb; i++) {
		buf.type = V4L2_BUF_TYPE_META_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		ret = ioctl(isp_desc->stat_fd, VIDIOC_QBUF, &buf);
		if (ret) {
			printf("Failed to queue buffer %d\n", i);
			return ret;
		}
	}

	/* Start stream */
	type = V4L2_BUF_TYPE_META_CAPTURE;
	ret = ioctl(isp_desc->stat_fd, VIDIOC_STREAMON, &type);
	if (ret) {
		printf("Failed to start stream\n");
		return ret;
	}

	FD_ZERO(&fds);
	FD_SET(isp_desc->stat_fd, &fds);
	tv.tv_sec = 2;
	tv.tv_usec = 0;

	do {
		/* Wait for buff */
		ret = select(isp_desc->stat_fd + 1, &fds, NULL, NULL, &tv);

		if (ret < 0) {
			printf("Select failed (%d)\n", ret);
			return ret;
		}
		if (ret == 0) {
			printf("Select timeout\n");
			return -EBUSY;
		}

		/* Get a buff */
		buf.type = V4L2_BUF_TYPE_META_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		ret = ioctl(isp_desc->stat_fd, VIDIOC_DQBUF, &buf);
		if (ret) {
			printf("Failed to dequeue buffer\n");
			return ret;
		}

		/* Return result (if requested) */
		if (stats)
			*stats = isp_desc->stats[buf.index];

		/* Print result */
		if (loop) {
			printf("\f");
			printf("Location Pre-demosaicing\n");
			print_average(isp_desc->stats[buf.index]->pre.average_RGB);
			print_bins(isp_desc->stats[buf.index]->pre.bins);
			printf("Location Post-demosaicing\n");
			print_average(isp_desc->stats[buf.index]->post.average_RGB);
			print_bins(isp_desc->stats[buf.index]->post.bins);
		}

		/* Queue buf */
		ret = ioctl(isp_desc->stat_fd, VIDIOC_QBUF, &buf);
		if (ret) {
			printf("Failed to queue buffer %d\n", i);
			return ret;
		}

		/* Wait to let user see printf output */
		if (loop)
			usleep(100 * 1000);
	} while (loop);

	/* Stop stream */
	type = V4L2_BUF_TYPE_META_CAPTURE;
	ret = ioctl(isp_desc->stat_fd, VIDIOC_STREAMOFF, &type);
	if (ret) {
		printf("Failed to stop stream\n");
		return ret;
	}

	return ret;
}

#define IMX335_EXPOSURE_MAX		4491
#define IMX335_EXPOSURE_MIN		50
#define IMX335_GAIN_MIN			0
#define IMX335_GAIN_MAX			240
#define IMX335_GAIN_DB_UNIT		0.3

#define AEC_ATTEMPT_MAX			20
#define AEC_EXPOSURE_UPDATE		400
#define AEC_GAIN_UPDATE_MAX		5
#define AEC_TOLERANCE			15
#define AEC_COEFF_LUM_GAIN		0.1
#define AEC_TARGET			56 /* Note: 56 is transformed to 128 after gamma correction */
/*
 * Function to configure both DCMIPP ISP & Sensor gain
 *
 * This algorithm updates the sensor gain and exposure so the average Luminance fits with a target.
 * Update the sensor gain until it reaches 0 : from that point, update the sensor exposure
 */
static int set_sensor_gain_exposure(struct isp_descriptor *isp_desc, bool verbose)
{
	float gain_db, gain_update_db;
	bool limit_reached = false, do_exposure_update, exposure_dec = false, exposure_inc = false;
	int ret, avgL, gain, exposure, attempt = 0;
	struct stm32_dcmipp_stat_buf *stats;
	int sensor_fd;

	sensor_fd = open(isp_desc->sensor_subdev_name, O_RDWR);
	if (sensor_fd == -1) {
		ret = -errno;
		printf("Failed to open sensor subdev %s\n", isp_desc->sensor_subdev_name);
		return ret;
	}

	/* Get sensor exposure */
	ret = get_ctrl(sensor_fd, V4L2_CID_EXPOSURE, &exposure);
	if (ret) {
		printf("Failed to get sensor exposure\n");
		close(sensor_fd);
		return ret;
	}

	if (exposure != IMX335_EXPOSURE_MAX)
		/* Start with exposure update (gain is expected to be 0) */
		do_exposure_update = true;
	else
		/* Start with gain update (exposure is at its max) */
		do_exposure_update = false;

	/* Get sensor gain (unit = 0.3dB) */
	ret = get_ext_ctrl_int(sensor_fd, V4L2_CTRL_CLASS_IMAGE_SOURCE, V4L2_CID_ANALOGUE_GAIN, &gain);
	gain_db = gain * IMX335_GAIN_DB_UNIT;
	if (ret) {
		printf("Failed to get sensor gain\n");
		close(sensor_fd);
		return ret;
	}

	do {
		/* Measure the luminance */
		ret = get_stat(isp_desc, false, &stats, V4L2_STAT_PROFILE_AVERAGE_POST);
		if (ret) {
			close(sensor_fd);
			return ret;
		}

		/* Compare the average luminance with the AEC_TARGET */
		avgL = luminance_from_rgb(stats->post.average_RGB);
		gain_update_db = 0;

		if (verbose) {
			printf("\nAttempt %d\n", attempt);
			printf(" Current AvgL = %d\n", avgL);
			printf(" Current gain = %d\n", gain);
			printf(" Current expo = %d\n", exposure);
		}

		if (avgL > AEC_TARGET + AEC_TOLERANCE) {
			/* Too bright, decrease gain */
			gain_update_db = (float)(AEC_TARGET - avgL) * AEC_COEFF_LUM_GAIN;
			if (gain_update_db < -AEC_GAIN_UPDATE_MAX)
				gain_update_db = -AEC_GAIN_UPDATE_MAX;
		} else if (avgL < AEC_TARGET - AEC_TOLERANCE) {
			/* Too dark vador, call a Jedi and increase gain */
			gain_update_db = (float)(AEC_TARGET - avgL) * AEC_COEFF_LUM_GAIN;
			if (gain_update_db > AEC_GAIN_UPDATE_MAX)
				gain_update_db = AEC_GAIN_UPDATE_MAX;
		}

		if (gain_update_db) {
			/* Need to change something (gain or exposure) */
			if (!do_exposure_update) {
				/* Update gain as it has not reached its min value */
				gain_db += gain_update_db;
				gain = gain_db / IMX335_GAIN_DB_UNIT;

				if (gain < IMX335_GAIN_MIN) {
					gain = IMX335_GAIN_MIN;
					/* Can't decrease gain anymore: we will have to decrease exposure */
					do_exposure_update = true;
				} else if (gain > IMX335_GAIN_MAX) {
					gain = IMX335_GAIN_MAX;
					limit_reached = true;
				}

				if (verbose)
					printf(">New gain = %d\n", gain);

				/* Set sensor gain */
				ret = set_ext_ctrl_int(sensor_fd, V4L2_CTRL_CLASS_IMAGE_SOURCE, V4L2_CID_ANALOGUE_GAIN, gain);
				if (ret) {
					printf("Failed to set sensor gain\n");
					close(sensor_fd);
					return ret;
				}
			} else {
				/* Update exposure since gain has reached its min value */
				if (gain_update_db < 0) {
					if (exposure_inc) {
						/* We previously increased exposure, so do not try to decrease it from now */
						limit_reached = true;
					}
					else
					{
						/* Decrease exposure */
						exposure -= AEC_EXPOSURE_UPDATE;
						exposure_dec = true;
						if (exposure < IMX335_EXPOSURE_MIN)
						{
							exposure = IMX335_EXPOSURE_MIN;
							limit_reached = true;
						}
					}
				} else {
					if (exposure_dec) {
						/* We previously decreased exposure, so do not try to increase it from now */
						limit_reached = true;
					}
					else
					{
						/* Increase exposure */
						exposure += AEC_EXPOSURE_UPDATE;
						exposure_inc = true;
						if (exposure > IMX335_EXPOSURE_MAX) {
							exposure = IMX335_EXPOSURE_MAX;
							/* Can't increase exposure anymore: we will have to increase gain */
							do_exposure_update = false;
						}
					}
				}

				if (verbose)
					printf(">New expo = %d\n", exposure);

				/* Set sensor exposure */
				ret = set_ctrl(sensor_fd, V4L2_CID_EXPOSURE, exposure);
				if (ret) {
					printf("Failed to set sensor exposure\n");
					close(sensor_fd);
					return ret;
				}
			}

			/* Note: we shall wait for 2 frames before checking the luminance update, but since it takes
			   more time than 2 frames to get some updated statistics, there is no need to call sleep() here */

			if (++attempt == AEC_ATTEMPT_MAX)
				limit_reached = true;
		}
	} while (gain_update_db && !limit_reached);

	close(sensor_fd);
	return ret;
}

/*
 * Function to demonstrate the DCMIPP ISP contrast block control
 */
static int set_contrast(struct isp_descriptor *isp_desc, int type)
{
	struct stm32_dcmipp_params_cfg params = {
		.module_cfg_update = STM32_DCMIPP_ISP_CE,
	};
	__u8 dynamic[9] = { 32, 32, 32, 27, 23, 20, 18, 17, 16 };
	__u8 *lum;

	int i, ret;

	switch (type) {
	case 0:
		/* Disabled */
		break;
	case 1:
		/* 50% */
		params.ctrls.ce_cfg.en = 1;
		lum = params.ctrls.ce_cfg.lum;

		for (i = 0; i < 9; i++)
			lum[i] = 8;
		break;
	case 2:
		/* 200% */
		params.ctrls.ce_cfg.en = 1;
		lum = params.ctrls.ce_cfg.lum;
		for (i = 0; i < 9; i++)
			lum[i] = 32;
		break;
	case 3:
		/* Dynamic */
		params.ctrls.ce_cfg.en = 1;
		lum = params.ctrls.ce_cfg.lum;
		memcpy(lum, dynamic, sizeof(dynamic));
		break;
	default:
		printf("Unknown contrast type (%d)\n", type);
		return -EINVAL;
	}

	ret = apply_params(isp_desc, &params);
	if (ret)
		printf("Failed to apply contrast\n");

	return ret;
}

/*
 * Convert a float value in a couple shift / mult used by the DCMIPP ISP
 */
static void to_shift_mult_float(float f, __u8 *shift, __u8 *mult)
{
	int s;

	if (f > 255)
		printf("Warning, invalid value : %f\n", f);

	for (s = 0; s < 8; s++) {
		if (f < 2.0)
			break;
		f /= 2;
	}

	*shift = s;
	*mult = f * 128;
}

/*
 * Convert a float value to a reg 2.8 format (1.0 is coded by 0x100, the sign is
 * indicated as complement to 2
 */
static __u16 to_cconv_reg(float f)
{
	__s16 tmp = 256 * f;

	if (tmp < 0)
		tmp = ((-tmp ^ 0x7FF) + 1) & 0x7FF;

	return (__u16)tmp;
}

/*
 * Function to adapt the DCMIPP ISP configuration based on the ambiant light profile
 */
static int set_profile(struct isp_descriptor *isp_desc, int type)
{
	/* Exposure / colorconv settings for D50 profile */
	const float exposure_D50[3] = { 2.2, 1.0, 1.8 };
	const float colorconv_D50[3][3] =
		{ {  1.8008,	-0.6484,	-0.1523 },
		  { -0.3555,	 1.6992,	-0.3438 },
		  {  0.0977,	-0.957,		 1.8594 } };

	/* Exposure / colorconv settings for TL84 profile */
	const float exposure_TL84[3] = { 1.7, 1.0, 2.35 };
	const float colorconv_TL84[3][3] =
		{ {  1.551345,	-0.6937,	 0.13106 },
		  { -0.38671,	 1.676898,	-0.33936 },
		  {  0.055462,	-0.6677,	 1.599442 } };

	float exposure[3], colorconv[3][3];
	struct stm32_dcmipp_params_cfg params = {
		.module_cfg_update = STM32_DCMIPP_ISP_BLC |
				     STM32_DCMIPP_ISP_EX |
				     STM32_DCMIPP_ISP_CC,
		.ctrls = {
			/* IMX335 black level set to 12 */
			.blc_cfg = {
				.en = 1,
				.blc_r = 12,
				.blc_g = 12,
				.blc_b = 12,
			},
		},
	};
	struct stm32_dcmipp_isp_ex_cfg *exposure_wb = &params.ctrls.ex_cfg;
	struct stm32_dcmipp_isp_cc_cfg *cconv = &params.ctrls.cc_cfg;

	int i, j, ret;

	if (type == 0) {
		memcpy(exposure, exposure_D50, sizeof(exposure));
		memcpy(colorconv, colorconv_D50, sizeof(colorconv));
	} else if (type == 1) {
		memcpy(exposure, exposure_TL84, sizeof(exposure));
		memcpy(colorconv, colorconv_TL84, sizeof(colorconv));
	} else {
		printf("Invalid profile : %d\n", type);
		return -EINVAL;
	}

	/* Set exposure */
	to_shift_mult_float(exposure[0], &exposure_wb->shift_r, &exposure_wb->mult_r);
	to_shift_mult_float(exposure[1], &exposure_wb->shift_g, &exposure_wb->mult_g);
	to_shift_mult_float(exposure[2], &exposure_wb->shift_b, &exposure_wb->mult_b);
	exposure_wb->en = 1;

	/* Set colorconv */
	cconv->rr = to_cconv_reg(colorconv[0][0]);
	cconv->rg = to_cconv_reg(colorconv[0][1]);
	cconv->rb = to_cconv_reg(colorconv[0][2]);
	cconv->gr = to_cconv_reg(colorconv[1][0]);
	cconv->gg = to_cconv_reg(colorconv[1][1]);
	cconv->gb = to_cconv_reg(colorconv[1][2]);
	cconv->br = to_cconv_reg(colorconv[2][0]);
	cconv->bg = to_cconv_reg(colorconv[2][1]);
	cconv->bb = to_cconv_reg(colorconv[2][2]);
	cconv->en = 1;
	cconv->clamp = STM32_DCMIPP_ISP_CC_CLAMP_DISABLED;

	ret = apply_params(isp_desc, &params);
	if (ret)
		printf("Failed to apply exposure\n");

	return ret;
}

static void usage(const char *argv0)
{
	printf("%s [options]\n", argv0);
	printf("DCMIPP ISP Control application\n");
	printf("-g, --gain                  Update the Sensor Gain and Exposure (AutoExposure)\n");
	printf("-c, --contrast TYPE         Set the contrast\n");
	printf("                            TYPE  0 : None\n");
	printf("                                  1 :  50%%\n");
	printf("                                  2 : 200%%\n");
	printf("                                  3 : Dynamic\n");
	printf("-i, --illuminant TYPE       Apply settings (black level, color conv, exposure) for a specific illuminant\n");
	printf("                            TYPE  0 : D50 (daylight)\n");
	printf("                                  1 : TL84 (fluo lamp)\n");
	printf("-s, --stat                  Read the stat\n");
	printf("-S, --STAT                  Read the stat continuously\n");
	printf("--help                      Display usage\n");
	printf("-v                          Verbose output\n");
}

static struct option opts[] = {
	{"gain", no_argument, 0, 'g'},
	{"contrast", required_argument, 0, 'c'},
	{"illuminant", required_argument, 0, 'i'},
	{"stat", no_argument, 0, 's'},
	{"STAT", no_argument, 0, 'S'},
	{ },
};

int main(int argc, char *argv[])
{
	static struct isp_descriptor isp_desc;
	int ret, opt;
	bool do_call_stat, do_call_stat_cont;
	struct stm32_dcmipp_stat_buf *stats;
	bool verbose = false;

	if ((argc == 1) || ((argc == 2) && !strcmp(argv[1], "--help"))) {
		usage(argv[0]);
		return 1;
	}

	/*
	 * Detect the verbose -v option
	 * Need to ensure that -v is detected first in order to be able
	 * to give it as option to other functions
	 */
	while ((opt = getopt_long(argc, argv, ":v", opts, NULL)) != -1) {
		switch (opt) {
			case 'v':
				verbose = true;
				break;
			default:
				break;
		}
	}
	optind = 1;

	ret = discover_dcmipp(&isp_desc);
	if (ret)
		return ret;
	if (verbose) {
		printf("DCMIPP ISP information:\n");
		printf(" Media device:		%s\n", isp_desc.media_dev_name);
		printf(" ISP sub-device:	%s\n", isp_desc.isp_subdev_name);
		printf(" ISP stat device:	%s\n", isp_desc.stat_dev_name);
		printf(" ISP params device:	%s\n", isp_desc.params_dev_name);
		printf(" Sensor sub-device:	%s\n", isp_desc.sensor_subdev_name);
		printf(" ISP frame:		%d x %d  -  %s\n", isp_desc.width, isp_desc.height, isp_desc.fmt_str);
		printf("--------------------------------------------------\n\n");
	}

	do_call_stat = false;
	do_call_stat_cont = false;

	while ((opt = getopt_long(argc, argv, "vgc:i:sS", opts, NULL)) != -1) {
		switch (opt) {
		case 'g':
			ret = set_sensor_gain_exposure(&isp_desc, verbose);
			if (ret)
				return ret;
			if (verbose)
				printf("Sensor gain and exposure applied\n");
			break;
		case 'c':
			ret = set_contrast(&isp_desc, atoi(optarg));
			if (ret)
				return ret;
			if (verbose)
				printf("Contrast applied\n");
			break;
		case 'i':
			ret = set_profile(&isp_desc, atoi(optarg));
			if (ret)
				return ret;
			if (verbose)
				printf("Profile applied for BlackLevel, Exposure and ColorConversion\n");
			break;
		case 's':
			do_call_stat = true;
			break;
		case 'S':
			do_call_stat_cont = true;
			break;
		case 'v':
			/* Just to have getopt_long not complain */
			break;
		default:
			printf("Invalid option -%c\n", opt);
			return 1;
		}
	}

	if (do_call_stat) {
		ret = get_stat(&isp_desc, false, &stats, V4L2_STAT_PROFILE_FULL);
		if (ret)
			return 1;
		printf("Location Pre-demosaicing\n");
		print_average(stats->pre.average_RGB);
		print_bins(stats->pre.bins);
		printf("Location Post-demosaicing\n");
		print_average(stats->post.average_RGB);
		print_bins(stats->post.bins);
	} else if (do_call_stat_cont)
		ret = get_stat(&isp_desc, true, NULL, V4L2_STAT_PROFILE_FULL);

	close_params_vdev(&isp_desc);
	close_stats_vdev(&isp_desc);
	close(isp_desc.isp_fd);

	return ret;
}
