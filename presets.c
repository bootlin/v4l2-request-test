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

static struct frame bbb_frames[] = {
#include "data/bbb-mpeg2/frames.h"
};

static struct preset presets[] = {
	{
		.name = "bbb-mpeg2",
		.description = "big_buck_bunny_480p_MPEG2_MP2_25fps_1800K.MPG",
		.license = "Creative Commons Attribution 3.0",
		.attribution = "Blender Foundation | www.blender.org",
		.width = 854,
		.height = 480,
		.frames = bbb_frames,
		.frames_count = sizeof(bbb_frames) / sizeof(bbb_frames[0]),
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

int frame_header_fill(struct v4l2_ctrl_mpeg2_frame_hdr *header, struct preset *preset, unsigned int index, unsigned int slice_size)
{
	if (header == NULL || preset == NULL)
		return -1;

	if (index >= preset->frames_count) {
		fprintf(stderr, "Frame index %d is too big for frames count: %d\n", index, preset->frames_count);
		return -1;
	}

	memcpy(header, &preset->frames[index].header, sizeof(*header));

	header->slice_pos = 0;
	header->slice_len = slice_size * 8;
	header->type = MPEG2;
	header->width = preset->width;
	header->height = preset->height;

	return 0;
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
	struct v4l2_ctrl_mpeg2_frame_hdr *header;
	struct v4l2_ctrl_mpeg2_frame_hdr *header_next;
	unsigned int gop_start_index;
	unsigned int i;
	bool gop_start = false;
	int rc;

	if (preset == NULL)
		return -1;

	if (index >= preset->frames_count) {
		fprintf(stderr, "Frame index %d is too big for frames count: %d\n", index, preset->frames_count);
		return -1;
	}

	header = &preset->frames[index].header;

	/* Only perform scheduling at GOP start. */
	if (header->picture_coding_type != PCT_I)
		return 0;

	rc = 0;

	for (gop_start_index = index; index < preset->frames_count; index++) {
		header = &preset->frames[index].header;

		/* I frames mark GOP end. */
		if (header->picture_coding_type == PCT_I && index > gop_start_index) {
			break;
		} else if (header->picture_coding_type == PCT_B) {
			/* The required backward reference frame is already available, queue now. */
			if (header->backward_ref_index >= index)
				rc |= frame_gop_queue(index);

			/* The B frame was already queued before the associated backward reference frame. */
			continue;
		}

		/* Queue B frames before their associated backward reference frames. */
		for (i = (index + 1); i < preset->frames_count; i++) {
			header_next = &preset->frames[i].header;

			if (header_next->picture_coding_type != PCT_B)
				continue;

			if (header_next->backward_ref_index == index)
				rc |= frame_gop_queue(i);
		}

		/* Queue the non-B frame at this point. */
		rc |= frame_gop_queue(index);
	}

	return rc;
}
