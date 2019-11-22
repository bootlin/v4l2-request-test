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
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <drm_fourcc.h>
#include <linux/media.h>
#include <linux/videodev2.h>
#include <mpeg2-ctrls.h>
#include <xf86drm.h>

#include "v4l2-request-test.h"

struct format_description formats[] = {
	{
		.description		= "NV12 YUV",
		.v4l2_format		= V4L2_PIX_FMT_NV12,
		.v4l2_buffers_count	= 1,
		.v4l2_mplane		= false,
		.drm_format		= DRM_FORMAT_NV12,
		.drm_modifier		= DRM_FORMAT_MOD_NONE,
		.planes_count		= 2,
		.bpp			= 16,
	},
#ifdef DRM_FORMAT_MOD_ALLWINNER_TILED
	{
		.description		= "Sunxi Tiled NV12 YUV",
		.v4l2_format		= V4L2_PIX_FMT_SUNXI_TILED_NV12,
		.v4l2_buffers_count	= 1,
		.v4l2_mplane		= false,
		.drm_format		= DRM_FORMAT_NV12,
		.drm_modifier		= DRM_FORMAT_MOD_ALLWINNER_TILED,
		.planes_count		= 2,
		.bpp			= 16
	},
#endif
};

static void print_help(void)
{
	printf("Usage: v4l2-request-test [OPTIONS]\n\n"
		"Options:\n"
		" -v  --video-device <dev>  Use device <dev> as the video device.\n"
		"     --device\n"
		" -m, --media-device <dev>  Use device <dev> as the media device.\n"
		" -d, --drm-device <dev>    Use device <dev > as DRM device.\n"
		" -D, --drm-driver <name>   Use given DRM driver.\n"
		" -s, --slices-path <path>  Use <path> to find stored video slices.\n"
		" -S, --slices-format <slices format>\n"
		"                           Regex/format describing filenames stored in the slices path.\n"
		" -f, --fps <fps>           Display given number of frames per seconds.\n"
		" -P, --preset-name <name>  Use given preset-name for video decoding.\n"
		" -i, --interactive         Enable interactive mode.\n"
		" -l, --loop                Loop preset frames.\n"
		" -q, --quiet               Enable quiet mode.\n"
		" -h, --help                This help message.\n\n");

	presets_usage();
}

static void print_summary(struct config *config, struct preset *preset)
{
	printf("Config:\n");
	printf(" Video device:  %s\n", config->video_path);
	printf(" Media device:  %s\n", config->media_path);
	printf(" DRM device:    %s\n", config->drm_path);
	printf(" DRM driver:    %s\n", config->drm_driver);
	printf(" Slices path:   %s\n", config->slices_path);
	printf(" Slices format: %s\n", config->slices_filename_format);
	printf(" FPS:           %d\n\n", config->fps);

	printf("Preset:\n");
	printf(" Name:         %s\n", preset->name);
	printf(" Description:  %s\n", preset->description);
	printf(" License:      %s\n", preset->license);
	printf(" Attribution:  %s\n", preset->attribution);
	printf(" Width:        %d\n", preset->width);
	printf(" Height:       %d\n", preset->height);
	printf(" Frames count: %d\n", preset->frames_count);

	printf(" Format: ");

	switch (preset->type) {
	case CODEC_TYPE_MPEG2:
		printf("MPEG2");
		break;
	case CODEC_TYPE_H264:
		printf("H264");
		break;
	case CODEC_TYPE_H265:
		printf("H265");
		break;
	default:
		printf("Invalid");
		break;
	}

	printf("\n\n");
}

static long time_diff(struct timespec *before, struct timespec *after)
{
	long before_time = before->tv_sec * 1000000 + before->tv_nsec / 1000;
	long after_time = after->tv_sec * 1000000 + after->tv_nsec / 1000;

	return (after_time - before_time);
}

static void print_time_diff(struct timespec *before, struct timespec *after,
			    const char *prefix)
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
		fprintf(stderr, "Unable to open file path: %s\n",
			strerror(errno));
		goto error;
	}

	rc = read(fd, buffer, length);
	if (rc < 0) {
		fprintf(stderr, "Unable to read file data: %s\n",
			strerror(errno));
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

	config->preset_name = strdup("bbb-mpeg2");
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
	struct video_setup video_setup;
	struct gem_buffer *gem_buffers;
	struct display_setup display_setup;
	struct media_device_info device_info;
	struct frame frame;
	struct timespec before, after;
	struct timespec video_before, video_after;
	struct timespec display_before, display_after;
	struct format_description *selected_format = NULL;
	bool before_taken = false;
	void *slice_data = NULL;
	char *slice_filename = NULL;
	char *slice_path = NULL;
	unsigned int slice_size;
	unsigned int width;
	unsigned int height;
	unsigned int v4l2_index;
	unsigned int index;
	unsigned int index_origin;
	unsigned int display_index;
	unsigned int display_count;
	unsigned int i;
	long frame_time;
	long frame_diff;
	int video_fd = -1;
	int media_fd = -1;
	int drm_fd = -1;
	uint64_t ts;
	bool test;
	int opt;
	int rc;

	setup_config(&config);

	while (1) {
	        int option_index = 0;
		static struct option long_options[] = {
			{ "device",        required_argument, 0, 'v' },
			{ "video-device",  required_argument, 0, 'v' },
			{ "media-device",  required_argument, 0, 'm' },
			{ "drm-device",    required_argument, 0, 'd' },
			{ "drm-driver",    required_argument, 0, 'D' },
			{ "slices-path",   required_argument, 0, 's' },
			{ "slices-format", required_argument, 0, 'S' },
			{ "fps",           required_argument, 0, 'f' },
			{ "preset-name",   required_argument, 0, 'P' },
			{ "interactive",   no_argument,       0, 'i' },
			{ "loop",          no_argument,       0, 'l' },
			{ "quiet",         no_argument,       0, 'q' },
			{ "help",          no_argument,       0, 'h' },
			{ 0,               0,                 0,  0  }
		};

		opt = getopt_long(argc, argv, "v:m:d:D:s:S:f:P:ilqh",
			     long_options, &option_index);
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
		case 's':
			config.slices_path = strdup(optarg);
			break;
		case 'S':
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

	preset = preset_find(config.preset_name);
	if (preset == NULL) {
		fprintf(stderr, "Unable to find preset for name: %s\n",
			config.preset_name);
		goto error;
	}

	config.buffers_count = preset->buffers_count;

	width = preset->width;
	height = preset->height;
	if (config.slices_path == NULL)
		asprintf(&config.slices_path, "data/%s", config.preset_name);

	print_summary(&config, preset);

	video_fd = open(config.video_path, O_RDWR | O_NONBLOCK, 0);
	if (video_fd < 0) {
		fprintf(stderr, "Unable to open video node: %s\n",
			strerror(errno));
		goto error;
	}

	media_fd = open(config.media_path, O_RDWR | O_NONBLOCK, 0);
	if (media_fd < 0) {
		fprintf(stderr, "Unable to open media node: %s\n",
			strerror(errno));
		goto error;
	}

	rc = ioctl(media_fd, MEDIA_IOC_DEVICE_INFO, &device_info);
	if (rc < 0) {
		fprintf(stderr, "Unable to get media device info: %s\n",
			strerror(errno));
		goto error;
	}

	printf("Media device driver: %s\n", device_info.driver);

	drm_fd = drmOpen(config.drm_driver, config.drm_path);
	if (drm_fd < 0) {
		fprintf(stderr, "Unable to open DRM node: %s\n",
			strerror(errno));
		goto error;
	}

	for (i = 0; i < ARRAY_SIZE(formats); i++) {
		test = video_engine_format_test(video_fd,
						formats[i].v4l2_mplane, width,
						height, formats[i].v4l2_format);
		if (test) {
			selected_format = &formats[i];
			break;
		}
	}

	if (selected_format == NULL) {
		fprintf(stderr,
			"Unable to find any supported destination format\n");
		goto error;
	}

	printf("Destination format: %s\n", selected_format->description);

	test = video_engine_capabilities_test(video_fd, V4L2_CAP_STREAMING);
	if (!test) {
		fprintf(stderr, "Missing required driver streaming capability\n");
		goto error;
	}

	if (selected_format->v4l2_mplane)
		test = video_engine_capabilities_test(video_fd,
						      V4L2_CAP_VIDEO_M2M_MPLANE);
	else
		test = video_engine_capabilities_test(video_fd,
						      V4L2_CAP_VIDEO_M2M);

	if (!test) {
		fprintf(stderr, "Missing required driver M2M capability\n");
		goto error;
	}

	rc = video_engine_start(video_fd, media_fd, width, height,
				selected_format, preset->type, &video_buffers,
				config.buffers_count, &video_setup);
	if (rc < 0) {
		fprintf(stderr, "Unable to start video engine\n");
		goto error;
	}

	rc = display_engine_start(drm_fd, width, height, selected_format,
				  video_buffers, config.buffers_count,
				  &gem_buffers, &display_setup);
	if (rc < 0) {
		fprintf(stderr, "Unable to start display engine\n");
		goto error;
	}

	if (config.fps > 0)
		frame_time = 1000000 / config.fps;

	display_count = 0;
	display_index = 0;
	index_origin = index = 0;

	/*
	 * Display count might be lower than frames count due to potentially
	 * missing predicted frames. Adapt at GOP scheduling time.
	 */
	preset->display_count = preset->frames_count;

	while (display_count < preset->display_count) {
		if (!config.quiet)
			printf("\nProcessing frame %d/%d\n", index + 1,
			       preset->frames_count);

		if ((index_origin != index && index < preset->frames_count) ||
		    (index == 0 && index == index_origin)) {
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

		if (!before_taken)
			clock_gettime(CLOCK_MONOTONIC, &before);
		else
			before_taken = false;

		/* Catch-up with already rendered frames. */
		if (display_index < index)
			goto frame_display;

		asprintf(&slice_filename, config.slices_filename_format, index);
		asprintf(&slice_path, "%s/%s", config.slices_path,
			 slice_filename);

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
			printf("Loaded %d bytes of video slice data\n",
			       slice_size);

		rc = frame_controls_fill(&frame, preset, config.buffers_count,
					 index, slice_size);
		if (rc < 0) {
			fprintf(stderr, "Unable to fill frame controls\n");
			goto error;
		}

		v4l2_index = index % config.buffers_count;
		ts = TS_REF_INDEX(index);
		clock_gettime(CLOCK_MONOTONIC, &video_before);

		rc = video_engine_decode(video_fd, v4l2_index, &frame.frame,
					 preset->type, ts, slice_data,
					 slice_size, video_buffers,
					 &video_setup);
		if (rc < 0) {
			fprintf(stderr, "Unable to decode video frame\n");
			goto error;
		}

		clock_gettime(CLOCK_MONOTONIC, &video_after);

		free(slice_data);
		slice_data = NULL;

		if (!config.quiet) {
			printf("Decoded video frame successfuly!\n");
			print_time_diff(&video_before, &video_after,
					"Frame decode");
		}

		/* Keep decoding until we can display a frame. */
		if (display_index > index) {
			before_taken = true;
			index++;
			continue;
		}

frame_display:
		rc = frame_gop_dequeue();
		if (rc < 0) {
			fprintf(stderr,
				"Unable to dequeue next GOP frame index for display\n");
			goto error;
		}

		v4l2_index = display_index % config.buffers_count;
		clock_gettime(CLOCK_MONOTONIC, &display_before);

		rc = display_engine_show(drm_fd, v4l2_index, video_buffers,
					 gem_buffers, &display_setup);
		if (rc < 0) {
			fprintf(stderr, "Unable to display video frame\n");
			goto error;
		}

		clock_gettime(CLOCK_MONOTONIC, &display_after);

		if (!config.quiet) {
			printf("Displayed video frame successfuly!\n");
			print_time_diff(&display_before, &display_after,
					"Frame display");
		}

		clock_gettime(CLOCK_MONOTONIC, &after);

		display_count++;

		if (config.interactive) {
			getchar();
		} else if (config.fps > 0) {
			frame_diff = time_diff(&before, &after);
			if (frame_diff > frame_time)
				fprintf(stderr,
					"Unable to meet %d fps target: %ld us late!\n",
					config.fps, frame_diff - frame_time);
			else
				usleep(frame_time - frame_diff);
		}

		if (display_index >= index)
			index++;

		if (config.loop && display_count == preset->display_count) {
			display_count = 0;
			display_index = 0;
			index_origin = index = 0;
		}
	}

	rc = video_engine_stop(video_fd, video_buffers, config.buffers_count,
			       &video_setup);
	if (rc < 0) {
		fprintf(stderr, "Unable to stop video engine\n");
		goto error;
	}

	rc = display_engine_stop(drm_fd, gem_buffers, &display_setup);
	if (rc < 0) {
		fprintf(stderr, "Unable to stop display engine\n");
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
