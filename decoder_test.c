/*
 * Copyright (C) 2019 Ralf Zerres <ralf.zerres@networkx.de>
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

#include "decoder_test.h"

int main(int argc, char *argv[])
{
	int id;
	char *media_path;
	char *video_path;
	char *decoder_name;
	struct v4l2_decoder *decoder = NULL;
	struct v4l2_decoder *new_decoder = NULL;
	struct vector *decoder_vector = NULL;

	/* declare and initialize a new vector */
	decoder_vector = calloc(1, sizeof(struct vector));
	decoder_vector_init(decoder_vector);
	printf("Vector initialized (num_entities: %d, capacity: %d)\n",
	       decoder_vector->num_entities,
	       decoder_vector->capacity);

	/* initialize test entities */
	decoder = calloc(1, sizeof(struct v4l2_decoder));

	struct v4l2_decoder test0[] = {
		{ .id		= 3,
		  .name		= "cedrus-proc",
		  .media_path	= "/dev/media3",
		  .video_path	= "/dev/video5"
		},
	};

	struct v4l2_decoder test1[] = {
		{ .id		= 17,
		  .name		= "video-dec-proc",
		  .media_path	= "/dev/media2",
		  .video_path	= "/dev/video4"
		},
	};

	struct v4l2_decoder test2[] = {
		{ .id		= 99,
		  .name		= "test-proc",
		  .media_path	= "/dev/media0",
		  .video_path	= "/dev/video0"
		},
	};

	/* Tests: create, remove, set, get vector entities */
	printf("Test-1: Append 3 entities\n");
	decoder_vector_append(decoder_vector, test0);
	decoder_vector_append(decoder_vector, test1);
	decoder_vector_append(decoder_vector, test2);
	decoder_vector_print(decoder_vector);

	printf("Test-2: Delete entity index 1\n");
	decoder_vector_delete(decoder_vector, 1);
	decoder_vector_print(decoder_vector);

	printf("Test-3: Set entity 'test1 at index 2\n");
	decoder_vector_set(decoder_vector, 2, test1);
	decoder_vector_print(decoder_vector);

	printf("Test-4: Append new entity\n");
	id = 4;
	media_path = "/dev/media4";
	video_path = "/dev/video1";
	decoder_name = "mydriver-proc";

	decoder->id = id;
	asprintf(&decoder->media_path, "%s", media_path);
	asprintf(&decoder->video_path, "%s", video_path);
	asprintf(&decoder->name, "%s", decoder_name);

	decoder_vector_append(decoder_vector, decoder);
	decoder_vector_print(decoder_vector);

	printf("Test-5: Update entity at index 3\n");
	video_path = "/dev/video2";
	asprintf(&decoder->video_path, "%s", video_path);
	decoder_vector_set(decoder_vector, 3, decoder);
	decoder_vector_print(decoder_vector);

	printf("Test-6: Append new entity\n");
	//struct v4l2_decoder *new_decoder = malloc(sizeof(v4l2_decoder));
	new_decoder = calloc(1, sizeof(struct v4l2_decoder));
	id = 6;
	media_path = "/dev/media1";
	video_path = "/dev/video1";
	decoder_name = "testdrv-proc";

	new_decoder->id = id;
	asprintf(&new_decoder->media_path, "%s", media_path);
	asprintf(&new_decoder->video_path, "%s", video_path);
	asprintf(&new_decoder->name, "%s", decoder_name);

	decoder_vector_append(decoder_vector, new_decoder);
	free ((void *)new_decoder);
	decoder_vector_print(decoder_vector);

	/* free underlying data array */
	if (decoder)
		free ((void *)decoder);

	decoder_vector_free(decoder_vector);
	printf("Vector freed\n");
}
