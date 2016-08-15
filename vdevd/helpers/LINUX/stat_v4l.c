/*
 * stat_v4l - reads video4linux device properties
 * 
 * Forked from systemd/src/udev/v4l_id/v4l_id.c on January 24, 2015
 * (original from http://cgit.freedesktop.org/systemd/systemd/tree/src/udev/v4l_id/v4l_id.c)
 * 
 * All modifications to the original source file are Copyright (C) 2015 Jude Nelson <judecn@gmail.com>
 *
 * Original copyright and license text produced below.
 */

/*
 * Copyright (C) 2009 Kay Sievers <kay@vrfy.org>
 * Copyright (c) 2009 Filippo Argiolas <filippo.argiolas@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details:
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

#include "common.h"

int main(int argc, char *argv[])
{
	static const struct option options[] = {
		{"help", no_argument, NULL, 'h'},
		{}
	};

	int fd = 0;
	char *device;
	struct v4l2_capability v2cap;
	char cap_str[4097];

	memset(cap_str, 0, 4097);

	for (;;) {
		int option;

		option = getopt_long(argc, argv, "h", options, NULL);
		if (option == -1) {
			break;
		}

		switch (option) {
		case 'h':
			printf("%s [-h,--help] <device file>\n\n"
			       "Video4Linux device identification.\n\n"
			       "  -h  Print this message\n", argv[0]);
			return 0;
		default:
			return 1;
		}
	}

	device = argv[optind];

	if (device == NULL) {
		return 2;
	}

	fd = open(device, O_RDONLY);
	if (fd < 0) {
		return 3;
	}

	if (ioctl(fd, VIDIOC_QUERYCAP, &v2cap) == 0) {

		vdev_property_add("VDEV_V4L_VERSION", "2");
		vdev_property_add("VDEV_V4L_PRODUCT", (char *)v2cap.card);

		strcat(cap_str, ":");

		if ((v2cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) > 0) {

			strcat(cap_str, "capture:");
		}
		if ((v2cap.capabilities & V4L2_CAP_VIDEO_OUTPUT) > 0) {

			strcat(cap_str, "video_output:");
		}
		if ((v2cap.capabilities & V4L2_CAP_VIDEO_OVERLAY) > 0) {

			strcat(cap_str, "video_overlay:");
		}
		if ((v2cap.capabilities & V4L2_CAP_AUDIO) > 0) {

			strcat(cap_str, "audio:");
		}
		if ((v2cap.capabilities & V4L2_CAP_TUNER) > 0) {

			strcat(cap_str, "tuner:");
		}
		if ((v2cap.capabilities & V4L2_CAP_RADIO) > 0) {

			strcat(cap_str, "radio:");
		}

		vdev_property_add("VDEV_V4L_CAPABILITIES", cap_str);
	}

	close(fd);

	vdev_property_print();
	vdev_property_free_all();

	return 0;
}
