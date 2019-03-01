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

#ifndef _V4L2_REQUEST_TEST_H_
#define _V4L2_REQUEST_TEST_H_

#include <stdbool.h>

#include <linux/types.h>
#include <linux/v4l2-controls.h>
#include <linux/videodev2.h>
#include <mpeg2-ctrls.h>
#include <h264-ctrls.h>
#include <hevc-ctrls.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define TS_REF_INDEX(index) ((index + 1) * 1000)
#define INDEX_REF_TS(ts) (((ts) / 1000) - 1)

/*
 * Structures
 */

struct config {
	char *video_path;
	char *media_path;
	char *drm_path;
	char *drm_driver;

	char *preset_name;
	char *slices_path;
	char *slices_filename_format;

	unsigned int buffers_count;
	unsigned int fps;
	bool quiet;
	bool interactive;
	bool loop;
};

struct format_description {
	char *description;
	unsigned int v4l2_format;
	unsigned int v4l2_buffers_count;
	bool v4l2_mplane;
	unsigned int drm_format;
	uint64_t drm_modifier;
	unsigned int planes_count;
	unsigned int bpp;
};

/* Presets */

enum codec_type {
	CODEC_TYPE_MPEG2,
	CODEC_TYPE_H264,
	CODEC_TYPE_H265,
};

enum pct {
	PCT_I,
	PCT_P,
	PCT_B,
	PCT_SI,
	PCT_SP
};

union controls {
	struct {
		struct v4l2_ctrl_mpeg2_slice_params slice_params;
		struct v4l2_ctrl_mpeg2_quantization quantization;
	} mpeg2;
#ifdef V4L2_PIX_FMT_H264_SLICE
	struct {
		struct v4l2_ctrl_h264_decode_param decode_param;
		struct v4l2_ctrl_h264_pps pps;
		struct v4l2_h264_pred_weight_table pred_weight;
		struct v4l2_ctrl_h264_scaling_matrix scaling_matrix;
		struct v4l2_ctrl_h264_slice_param slice_param;
		struct v4l2_ctrl_h264_sps sps;
	} h264;
#endif
#ifdef V4L2_PIX_FMT_HEVC_SLICE
	struct {
		struct v4l2_ctrl_hevc_sps sps;
		struct v4l2_ctrl_hevc_pps pps;
		struct v4l2_ctrl_hevc_slice_params slice_params;
	} h265;
#endif
};

struct frame {
	unsigned int index;
	union controls frame;
};

struct preset {
	char *name;
	char *description;
	char *license;
	char *attribution;

	unsigned int width;
	unsigned int height;
	unsigned int buffers_count;

	enum codec_type type;
	struct frame *frames;
	unsigned int frames_count;
	unsigned int display_count;
};

/* V4L2 */

struct video_setup {
	unsigned int output_type;
	unsigned int capture_type;
};

struct video_buffer {
	void *source_map;
	void *source_data;
	unsigned int source_size;

	void *destination_map[VIDEO_MAX_PLANES];
	unsigned int destination_map_lengths[VIDEO_MAX_PLANES];
	void *destination_data[VIDEO_MAX_PLANES];
	unsigned int destination_sizes[VIDEO_MAX_PLANES];
	unsigned int destination_offsets[VIDEO_MAX_PLANES];
	unsigned int destination_bytesperlines[VIDEO_MAX_PLANES];
	unsigned int destination_planes_count;
	unsigned int destination_buffers_count;

	int export_fds[VIDEO_MAX_PLANES];
	int request_fd;
};

/* DRM */

struct gem_buffer {
	void *data;
	unsigned int size;
	unsigned int handles[4];
	unsigned int pitches[4];
	unsigned int offsets[4];
	unsigned int planes_count;

	unsigned int framebuffer_id;
};

struct display_properties_ids {
	uint32_t connector_crtc_id;
	uint32_t crtc_mode_id;
	uint32_t crtc_active;
	uint32_t plane_fb_id;
	uint32_t plane_crtc_id;
	uint32_t plane_src_x;
	uint32_t plane_src_y;
	uint32_t plane_src_w;
	uint32_t plane_src_h;
	uint32_t plane_crtc_x;
	uint32_t plane_crtc_y;
	uint32_t plane_crtc_w;
	uint32_t plane_crtc_h;
	uint32_t plane_zpos;
};

struct display_setup {
	unsigned int connector_id;
	unsigned int encoder_id;
	unsigned int crtc_id;
	unsigned int plane_id;

	unsigned int width;
	unsigned int height;
	unsigned int x;
	unsigned int y;
	unsigned int scaled_width;
	unsigned int scaled_height;

	unsigned int buffers_count;
	bool use_dmabuf;

	struct display_properties_ids properties_ids;
};

/*
 * Functions
 */

/* Presets */

void presets_usage(void);
struct preset *preset_find(char *name);
int frame_controls_fill(struct frame *frame, struct preset *preset,
			unsigned int buffers_count, unsigned int index,
			unsigned int slice_size);
unsigned int frame_pct(struct preset *preset, unsigned int index);
unsigned int frame_backward_ref_index(struct preset *preset,
				      unsigned int index);
int frame_gop_next(unsigned int *index);
int frame_gop_dequeue(void);
int frame_gop_queue(unsigned int index);
int frame_gop_schedule(struct preset *preset, unsigned int index);

/* V4L2 */

bool video_engine_capabilities_test(int video_fd,
				    unsigned int capabilities_required);
bool video_engine_format_test(int video_fd, bool mplane, unsigned int width,
			      unsigned int height, unsigned int format);
int video_engine_start(int video_fd, int media_fd, unsigned int width,
		       unsigned int height, struct format_description *format,
		       enum codec_type type, struct video_buffer **buffers,
		       unsigned int buffers_count, struct video_setup *setup);
int video_engine_stop(int video_fd, struct video_buffer *buffers,
		      unsigned int buffers_count, struct video_setup *setup);
int video_engine_decode(int video_fd, unsigned int index, union controls *frame,
			enum codec_type type, uint64_t ts, void *source_data,
			unsigned int source_size, struct video_buffer *buffers,
			struct video_setup *setup);

/* DRM */

int display_engine_start(int drm_fd, unsigned int width, unsigned int height,
			 struct format_description *format,
			 struct video_buffer *video_buffers, unsigned int count,
			 struct gem_buffer **buffers,
			 struct display_setup *setup);
int display_engine_stop(int drm_fd, struct gem_buffer *buffers,
			struct display_setup *setup);
int display_engine_show(int drm_fd, unsigned int index,
			struct video_buffer *video_buffers,
			struct gem_buffer *buffers,
			struct display_setup *setup);

#endif
