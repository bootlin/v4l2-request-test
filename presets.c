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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <linux/videodev2.h>
#include <linux/media.h>

#include "cedrus-frame-test.h"

static struct frame bbb_mpeg2_frames[] = {
#include "data/bbb-mpeg2/frames.h"
};

static struct frame ed_mpeg2_frames[] = {
#include "data/ed-mpeg2/frames.h"
};

static struct frame bbb_h264_all_i_32_frames[] = {
#include "data/bbb-h264-all-i-32/frames.h"
};

static struct frame bbb_h264_32_frames[] = {
#include "data/bbb-h264-32/frames.h"
};

static struct preset presets[] = {
	{
		.name = "bbb-mpeg2",
		.description = "big_buck_bunny_480p_MPEG2_MP2_25fps_1800K.MPG",
		.license = "Creative Commons Attribution 3.0",
		.attribution = "Blender Foundation | www.blender.org",
		.width = 854,
		.height = 480,
		.type = FORMAT_TYPE_MPEG2,
		.buffers_count = 6,
		.frames = bbb_mpeg2_frames,
		.frames_count = sizeof(bbb_mpeg2_frames) / sizeof(bbb_mpeg2_frames[0]),
	},
	{
		.name = "ed-mpeg2",
		.description = "Elephants Dream",
		.license = "Creative Commons Attribution 3.0",
		.attribution = "Blender Foundation | www.blender.org",
		.width = 1280,
		.height = 720,
		.type = FORMAT_TYPE_MPEG2,
		.buffers_count = 6,
		.frames = ed_mpeg2_frames,
		.frames_count = sizeof(ed_mpeg2_frames) / sizeof(ed_mpeg2_frames[0]),
	},
	{
		.name = "bbb-h264-all-i-32",
		.description = "big_buck_bunny_480p_H264_AAC_25fps_1800K.MP4",
		.license = "Creative Commons Attribution 3.0",
		.attribution = "Blender Foundation | www.blender.org",
		.width = 854,
		.height = 480,
		.type = FORMAT_TYPE_H264,
		.buffers_count = 16,
		.frames = bbb_h264_all_i_32_frames,
		.frames_count = sizeof(bbb_h264_all_i_32_frames) / sizeof(bbb_h264_all_i_32_frames[0]),
	},
	{
		.name = "bbb-h264-32",
		.description = "big_buck_bunny_480p_H264_AAC_25fps_1800K.MP4",
		.license = "Creative Commons Attribution 3.0",
		.attribution = "Blender Foundation | www.blender.org",
		.width = 854,
		.height = 480,
		.type = FORMAT_TYPE_H264,
		.buffers_count = 16,
		.frames = bbb_h264_32_frames,
		.frames_count = sizeof(bbb_h264_32_frames) / sizeof(bbb_h264_32_frames[0]),
	},
};

static unsigned int presets_count = sizeof(presets) / sizeof(presets[0]);

static unsigned int frame_gop_list[64];
static unsigned int frame_gop_list_size = sizeof(frame_gop_list) / sizeof(frame_gop_list[0]);
static unsigned int frame_gop_count = 0;
static unsigned int frame_gop_start = 0;

void presets_usage(void)
{
	struct preset *p;
	unsigned int i;

	for (i = 0; i < presets_count; i++) {
		p = &presets[i];

		printf(" %s: %s\n", p->name, p->description);
	}
}

struct preset *preset_find(char *name)
{
	struct preset *p;
	unsigned int i;

	for (i = 0; i < presets_count; i++) {
		p = &presets[i];

		if (strcmp(p->name, name) == 0)
			return p;
	}

	return NULL;
}

int frame_controls_fill(union controls *frame, struct preset *preset, unsigned int buffers_count, unsigned int index, unsigned int slice_size)
{
	struct v4l2_ctrl_h264_decode_param *decode_param;
	struct v4l2_h264_dpb_entry *dpb;
	unsigned int i;

	if (frame == NULL || preset == NULL)
		return -1;

	if (index >= preset->frames_count) {
		fprintf(stderr, "Frame index %d is too big for frames count: %d\n", index, preset->frames_count);
		return -1;
	}

	memcpy(frame, &preset->frames[index].frame, sizeof(*frame));

	switch (preset->type) {
	case FORMAT_TYPE_MPEG2:
		frame->mpeg2.slice_params.slice_pos = 0;
		frame->mpeg2.slice_params.slice_len = slice_size * 8;

		frame->mpeg2.slice_params.width = preset->width;
		frame->mpeg2.slice_params.height = preset->height;

		frame->mpeg2.slice_params.forward_ref_index %= buffers_count;
		frame->mpeg2.slice_params.backward_ref_index %= buffers_count;
		break;
	case FORMAT_TYPE_H264:
		for (i = 0; i < 16; i++) {
			decode_param = &frame->h264.decode_param;
			dpb = &decode_param->dpb[i];
			dpb->buf_index = dpb->frame_num;
		}
		break;
	default:
		return -1;
	}


	return 0;
}

unsigned int frame_pct(struct preset *preset, unsigned int index)
{
	if (preset == NULL)
		return PCT_I;

	switch (preset->type) {
	case FORMAT_TYPE_MPEG2:
		switch (preset->frames[index].frame.mpeg2.slice_params.slice_type) {
		case V4L2_MPEG2_SLICE_TYPE_I: return PCT_I;
		case V4L2_MPEG2_SLICE_TYPE_P: return PCT_P;
		case V4L2_MPEG2_SLICE_TYPE_B: return PCT_B;
		default: return PCT_I;
		}
	default:
		return PCT_I;
	}
}

unsigned int frame_backward_ref_index(struct preset *preset, unsigned int index)
{
	if (preset == NULL)
		return 0;

	switch (preset->type) {
	case FORMAT_TYPE_MPEG2:
		return preset->frames[index].frame.mpeg2.slice_params.backward_ref_index;
	default:
		return 0;
	}
}

int frame_gop_next(unsigned int *index)
{
	if (frame_gop_count == 0)
		return -1;

	if (index != NULL)
		*index = frame_gop_list[frame_gop_start];

	return 0;
}

int frame_gop_dequeue(void)
{
	if (frame_gop_count == 0)
		return -1;

	frame_gop_start = (frame_gop_start + 1) % frame_gop_list_size;
	frame_gop_count--;

	return 0;
}

int frame_gop_queue(unsigned int index)
{
	unsigned int i;

	if (frame_gop_count >= frame_gop_list_size)
		return -1;

	i = (frame_gop_start + frame_gop_count) % frame_gop_list_size;
	frame_gop_list[i] = index;

	frame_gop_count++;

	return 0;
}

int frame_gop_schedule(struct preset *preset, unsigned int index)
{
	unsigned int gop_start_index;
	unsigned int pct, pct_next;
	unsigned int backward_ref_index, backward_ref_index_next;
	unsigned int i;
	int rc;

	if (preset == NULL)
		return -1;

	if (index >= preset->frames_count) {
		fprintf(stderr, "Frame index %d is too big for frames count: %d\n", index, preset->frames_count);
		return -1;
	}

	pct = frame_pct(preset, index);

	/* Only perform scheduling at GOP start. */
	if (pct != PCT_I)
		return 0;

	rc = 0;

	for (gop_start_index = index; index < preset->frames_count; index++) {
		pct = frame_pct(preset, index);
		backward_ref_index = frame_backward_ref_index(preset, index);

		/* I frames mark GOP end. */
		if (pct == PCT_I && index > gop_start_index) {
			break;
		} else if (pct == PCT_B) {
			/* The required backward reference frame is already available, queue now. */
			if (backward_ref_index >= index)
				rc |= frame_gop_queue(index);

			/* The B frame was already queued before the associated backward reference frame. */
			continue;
		}

		/* Queue B frames before their associated backward reference frames. */
		for (i = (index + 1); i < preset->frames_count; i++) {
			pct_next = frame_pct(preset, i);
			backward_ref_index_next = frame_backward_ref_index(preset, i);

			if (pct_next != PCT_B)
				continue;

			if (backward_ref_index_next == index)
				rc |= frame_gop_queue(i);
		}

		/* Queue the non-B frame at this point. */
		rc |= frame_gop_queue(index);
	}

	return rc;
}
