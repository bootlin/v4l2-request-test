/*
 * Copyright (C) 2018 Paul Kocialkowski <paul.kocialkowski@bootlin.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <getopt.h>
#include <time.h>

#include <linux/videodev2.h>
#include <linux/media.h>
#include <xf86drm.h>

#include "cedrus-frame-test.h"

static void print_help(void)
{
	printf("Usage: cedrus-frame-test [OPTIONS] [SLICES PATH]\n\n"
		"Options:\n"
		" -v [video path]                path for the video node\n"
		" -m [media path]                path for the media node\n"
		" -d [DRM path]                  path for the DRM node\n"
		" -D [DRM driver]                DRM driver to use\n"
		" -c [CRTC ID]                   DRM CRTC to use\n"
		" -p [Plane ID]                  DRM plane to use\n"
		" -s [slices filename format]    format for filenames in the slices path\n"
		" -f [fps]                       number of frames to display per second\n"
		" -P [video preset]              video preset to use\n"
		" -i                             enable interactive mode\n"
		" -l                             loop preset frames\n"
		" -q                             enable quiet mode\n"
		" -h                             help\n\n"
		"Video presets:\n");

	presets_usage();
}

static void print_summary(struct config *config, struct preset *preset)
{
	printf("Config:\n");
	printf(" Video path: %s\n", config->video_path);
	printf(" Media path: %s\n", config->media_path);
	printf(" DRM path: %s\n", config->drm_path);
	printf(" DRM driver: %s\n", config->drm_driver);
	printf(" DRM CRTC ID: %d\n", config->crtc_id);
	printf(" DRM plane ID: %d\n", config->plane_id);
	printf(" Slices path: %s\n", config->slices_path);
	printf(" Slices filename format: %s\n", config->slices_filename_format);
	printf(" FPS: %d\n\n", config->fps);

	printf("Preset:\n");
	printf(" Name: %s\n", preset->name);
	printf(" Description: %s\n", preset->description);
	printf(" License: %s\n", preset->license);
	printf(" Attribution: %s\n", preset->attribution);
	printf(" Width: %d\n", preset->width);
	printf(" Height: %d\n", preset->height);
	printf(" Frames count: %d\n\n", preset->frames_count);
}

static long time_diff(struct timespec *before, struct timespec *after)
{
	long before_time = before->tv_sec * 1000000 + before->tv_nsec / 1000;
	long after_time = after->tv_sec * 1000000 + after->tv_nsec / 1000;

	return (after_time - before_time);
}

static void print_time_diff(struct timespec *before, struct timespec *after, const char *prefix)
{
	long diff = time_diff(before, after);
	printf("%s time: %ld us\n", prefix, diff);
}

static int load_data(const char *path, void **data, unsigned int *size)
{
	void *buffer = NULL;
	unsigned int length;
	struct stat st;
	int fd;
	int rc;

	rc = stat(path, &st);
	if (rc < 0) {
		fprintf(stderr, "Stating file failed\n");
		goto error;
	}

	length = st.st_size;

	buffer = malloc(length);

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Unable to open file path: %s\n", strerror(errno));
		goto error;
	}

	rc = read(fd, buffer, length);
	if (rc < 0) {
		fprintf(stderr, "Unable to read file data: %s\n", strerror(errno));
		goto error;
	}

	close(fd);

	*data = buffer;
	*size = length;

	rc = 0;
	goto complete;

error:
	if (buffer != NULL)
		free(buffer);

	rc = -1;

complete:
	return rc;
}

static void setup_config(struct config *config)
{
	memset(config, 0, sizeof(*config));

	config->video_path = strdup("/dev/video0");
	config->media_path = strdup("/dev/media0");
	config->drm_path = strdup("/dev/dri/card0");
	config->drm_driver = strdup("sun4i-drm");
	config->crtc_id = 40;
	config->plane_id = 34;

	config->preset_name = strdup("bbb-mpeg2");
	config->slices_path = strdup("data/bbb-mpeg2");
	config->slices_filename_format = strdup("slice-%d.dump");

	config->fps = 0;
	config->quiet = false;
	config->interactive = false;
	config->loop = false;
}

static void cleanup_config(struct config *config)
{
	free(config->video_path);
	free(config->media_path);
	free(config->drm_path);
	free(config->drm_driver);

	free(config->preset_name);
	free(config->slices_path);
	free(config->slices_filename_format);
}

int main(int argc, char *argv[])
{
	struct preset *preset;
	struct config config;
	struct video_buffer *video_buffers;
	struct gem_buffer *gem_buffers;
	struct display_setup setup;
	struct media_device_info device_info;
	struct v4l2_ctrl_mpeg2_frame_hdr header;
	struct timespec before, after;
	struct timespec video_before, video_after;
	struct timespec display_before, display_after;
	void *destination_data[2] = { NULL };
	void *slice_data = NULL;
	char *slice_filename = NULL;
	char *slice_path = NULL;
	unsigned int slice_size;
	unsigned int buffers_count = 6; // TODO: Estimate from largest gap between associated frames.
	unsigned int width;
	unsigned int height;
	unsigned int v4l2_index;
	unsigned int index;
	unsigned int index_origin;
	unsigned int display_index;
	unsigned int display_count;
	long frame_time;
	long frame_diff;
	int video_fd = -1;
	int media_fd = -1;
	int drm_fd = -1;
	int opt;
	int rc;

	setup_config(&config);

	while (1) {
		opt = getopt(argc, argv, "v:m:d:D:c:p:s:f:P:ilqh");
		if (opt == -1)
			break;

		switch (opt) {
			case 'v':
				free(config.video_path);
				config.video_path = strdup(optarg);
				break;
			case 'm':
				free(config.media_path);
				config.media_path = strdup(optarg);
				break;
			case 'd':
				free(config.drm_path);
				config.drm_path = strdup(optarg);
				break;
			case 'D':
				free(config.drm_driver);
				config.drm_driver = strdup(optarg);
				break;
			case 'c':
				config.crtc_id = atoi(optarg);
				break;
			case 'p':
				config.plane_id = atoi(optarg);
				break;
			case 's':
				free(config.slices_filename_format);
				config.slices_filename_format = strdup(optarg);
				break;
			case 'f':
				config.fps = atoi(optarg);
				break;
			case 'P':
				free(config.preset_name);
				config.preset_name = strdup(optarg);
				break;
			case 'i':
				config.interactive = true;
				break;
			case 'l':
				config.loop = true;
				break;
			case 'q':
				config.quiet = true;
				break;
			case 'h':
				print_help();

				rc = 0;
				goto complete;
			case '?':
				print_help();
				goto error;
		}
	}

	if (optind < argc) {
		free(config.slices_path);
		config.slices_path = strdup(argv[optind]);
	}

	preset = preset_find(config.preset_name);
	if (preset == NULL) {
		fprintf(stderr, "Unable to find preset for name: %s\n", config.preset_name);
		goto error;
	}

	width = preset->width;
	height = preset->height;

	print_summary(&config, preset);

	video_fd = open(config.video_path, O_RDWR | O_NONBLOCK, 0);
	if (video_fd < 0) {
		fprintf(stderr, "Unable to open video node: %s\n", strerror(errno));
		goto error;
	}

	media_fd = open(config.media_path, O_RDWR | O_NONBLOCK, 0);
	if (video_fd < 0) {
		fprintf(stderr, "Unable to open video node: %s\n", strerror(errno));
		goto error;
	}

	rc = ioctl(media_fd, MEDIA_IOC_DEVICE_INFO, &device_info);
	if (rc < 0) {
		fprintf(stderr, "Unable to get media device info: %s\n", strerror(errno));
		goto error;
	}

	printf("Media device driver: %s\n", device_info.driver);

	drm_fd = drmOpen(config.drm_driver, config.drm_path);
	if (drm_fd < 0) {
		fprintf(stderr, "Unable to open DRM node: %s\n", strerror(errno));
		goto error;
	}

	rc = video_engine_start(video_fd, media_fd, width, height, &video_buffers, buffers_count);
	if (rc < 0) {
		fprintf(stderr, "Unable to start video engine\n");
		goto error;
	}

	rc = display_engine_start(drm_fd, config.crtc_id, config.plane_id, width, height, &gem_buffers, &setup);
	if (rc < 0) {
		fprintf(stderr, "Unable to start display engine\n");
		goto error;
	}

	if (config.fps > 0)
		frame_time = 1000000 / config.fps;

	display_count = 0;
	display_index = 0;
	index_origin = index = 0;

	while (display_count < preset->frames_count) {
		if (!config.quiet)
			printf("\nProcessing frame %d/%d\n", index + 1, preset->frames_count);

		if ((index_origin != index && index < preset->frames_count) || (index == 0 && index == index_origin)) {
			rc = frame_gop_schedule(preset, index);
			if (rc < 0) {
				fprintf(stderr, "Unable to schedule GOP frames order\n");
				goto error;
			}
		}

		index_origin = index;

		rc = frame_gop_next(&display_index);
		if (rc < 0) {
			fprintf(stderr, "Unable to get next GOP frame index for display\n");
			goto error;
		}

		/* Catch-up with already rendered frames. */
		if (display_index < index)
			goto frame_display;

		clock_gettime(CLOCK_MONOTONIC, &before);

		asprintf(&slice_filename, config.slices_filename_format, index);
		asprintf(&slice_path, "%s/%s", config.slices_path, slice_filename);

		free(slice_filename);
		slice_filename = NULL;

		rc = load_data(slice_path, &slice_data, &slice_size);
		if (rc < 0) {
			fprintf(stderr, "Unable to load slice data\n");
			goto error;
		}

		free(slice_path);
		slice_path = NULL;

		if (!config.quiet)
			printf("Loaded %d bytes of video slice data\n", slice_size);

		frame_header_fill(&header, preset, index, slice_size);

		v4l2_index = index % buffers_count;
		header.forward_ref_index %= buffers_count;
		header.backward_ref_index %= buffers_count;

		clock_gettime(CLOCK_MONOTONIC, &video_before);

		rc = video_engine_decode(video_fd, v4l2_index, &header, slice_data, slice_size, video_buffers);
		if (rc < 0) {
			fprintf(stderr, "Unable to decode video frame\n");
			goto error;
		}

		clock_gettime(CLOCK_MONOTONIC, &video_after);

		free(slice_data);
		slice_data = NULL;

		if (!config.quiet) {
			printf("Decoded video frame successfuly!\n");
			print_time_diff(&video_before, &video_after, "Frame decode");
		}

		/* Keep decoding until we can display a frame. */
		if (display_index > index) {
			index++;
			continue;
		}

frame_display:
		rc = frame_gop_dequeue();
		if (rc < 0) {
			fprintf(stderr, "Unable to dequeue next GOP frame index for display\n");
			goto error;
		}

		v4l2_index = display_index % buffers_count;

		clock_gettime(CLOCK_MONOTONIC, &display_before);

		rc = display_engine_show(drm_fd, v4l2_index, video_buffers, gem_buffers, &setup);
		if (rc < 0) {
			fprintf(stderr, "Unable to display video frame\n");
			goto error;
		}

		clock_gettime(CLOCK_MONOTONIC, &display_after);

		if (!config.quiet) {
			printf("Displayed video frame successfuly!\n");
			print_time_diff(&display_before, &display_after, "Frame display");
		}

		clock_gettime(CLOCK_MONOTONIC, &after);

		display_count++;

		if (config.interactive) {
			getchar();
		} else if (config.fps > 0) {
			frame_diff = time_diff(&before, &after);
			if (frame_diff > frame_time)
				fprintf(stderr, "Unable to meet %d fps target: %ld us late!\n", config.fps, frame_diff - frame_time);
			else
				usleep(frame_time - frame_diff);
		}

		if (display_index >= index)
			index++;

		if (config.loop && display_count == preset->frames_count) {
			display_count = 0;
			display_index = 0;
			index_origin = index = 0;
		}
	}

	rc = video_engine_stop(video_fd, video_buffers, buffers_count);
	if (rc < 0) {
		fprintf(stderr, "Unable to start video engine\n");
		goto error;
	}

	rc = 0;
	goto complete;

error:
	rc = 1;

complete:
	if (slice_data != NULL)
		free(slice_data);

	if (slice_path != NULL)
		free(slice_path);

	if (slice_filename != NULL)
		free(slice_filename);

	if (drm_fd >= 0)
		drmClose(drm_fd);

	if (media_fd >= 0)
		close(media_fd);

	if (video_fd >= 0)
		close(video_fd);

	cleanup_config(&config);

	return rc;
}
