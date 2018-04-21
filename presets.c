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
