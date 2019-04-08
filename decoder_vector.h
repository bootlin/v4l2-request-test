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

#ifndef _V4L2_VECTOR_H_
#define _V4L2_VECTOR_H_

//#include <linux/media.h>

/*
 * Structures
 */

/* media topology */

struct v4l2_decoder {
	int id;
	char *name;
	char *media_path;
	char *video_path;
};


/*  Type:  v4l2_decoder vector */

struct vector {
	int num_entities;
	int capacity;
	void **v4l2_decoders;
} decoder_vector;

/*
typedef struct {
	int num_entities;
	int capacity;
	void **v4l2_decoders;
} decoder_vector;
*/

/*
 * Functions
 */

/* Decoder Vector */

void decoder_vector_append(struct vector *decoder_vector, struct v4l2_decoder *value);
void decoder_vector_delete(struct vector *decoder_vector, int index);
static void decoder_vector_extend(struct vector *decoder_vector);
void decoder_vector_free(struct vector *decoder_vector);
void *decoder_vector_get(struct vector *decoder_vector, int index);
void decoder_vector_init(struct vector *decoder_vector);
int decoder_vector_num_entities(struct vector *decoder_vector);
void decoder_vector_print(struct vector *decoder_vector);
static void decoder_vector_reduce(struct vector *decoder_vector);
void decoder_vector_set(struct vector *decoder_vector, int index, void *value);

#endif
