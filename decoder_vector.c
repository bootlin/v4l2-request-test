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
#include <string.h>

#include "decoder_vector.h"

//void decoder_vector_append(decoder_vector *decoder_vector, void *decoder) {
void decoder_vector_append(struct vector *decoder_vector, struct v4l2_decoder *decoder)
{
	// make sure there's room to expand into
	if (decoder_vector->capacity == decoder_vector->num_entities)
		decoder_vector_extend(decoder_vector);

	// append the decoder and increment element counter
	decoder_vector->v4l2_decoders[decoder_vector->num_entities++] = decoder;
}

void decoder_vector_delete(struct vector *decoder_vector, int index)
{
    if (index < 0 || index >= decoder_vector->num_entities)
	return;

    decoder_vector->v4l2_decoders[index] = NULL;

    for (int i = index; i < decoder_vector->capacity - 1; i++) {
	decoder_vector->v4l2_decoders[i] = decoder_vector->v4l2_decoders[i + 1];
	decoder_vector->v4l2_decoders[i + 1] = NULL;
    }

    decoder_vector->num_entities--;

    decoder_vector_reduce(decoder_vector);
}

int decoder_vector_num_entities(struct vector *decoder_vector)
{
	return decoder_vector->num_entities;
}

static void decoder_vector_extend(struct vector *decoder_vector)
{
	if (decoder_vector->num_entities >= decoder_vector->capacity) {
		/*
		 * extend decoder_vector->capacity to hold another struct entity
		 * and resize the allocated memory accordingly
		 */
		decoder_vector->capacity++;
		decoder_vector->v4l2_decoders = realloc(decoder_vector->v4l2_decoders, sizeof(struct v4l2_decoder) * decoder_vector->capacity);
	}
}

void decoder_vector_free(struct vector *decoder_vector)
{
	free(decoder_vector->v4l2_decoders);
}

void *decoder_vector_get(struct vector *decoder_vector, int index)
{
  if (index >= decoder_vector->num_entities || index < 0) {
    printf("Index %d out of bounds for vector of %d entities\n", index, decoder_vector->num_entities);
    return NULL;
  }
  return decoder_vector->v4l2_decoders[index];
}

void decoder_vector_init(struct vector *decoder_vector)
{
	// initialize num_entities and capacity
	decoder_vector->num_entities = 0;
	decoder_vector->capacity = 1;

	// allocate memory for decoder_vector->data
	decoder_vector->v4l2_decoders = calloc(decoder_vector->capacity, sizeof(struct v4l2_decoder));
}

void decoder_vector_print(struct vector *decoder_vector)
{
	printf("Vector: num_entities: %d, capacity: %d\n",
	       decoder_vector->num_entities,
	       decoder_vector->capacity);
	for (int i = 0; i < decoder_vector_num_entities(decoder_vector); i++) {
		struct v4l2_decoder *v = decoder_vector_get(decoder_vector, i);
		printf("entity[%i]: %s (id: %i, media_path: %s, video_path: %s)\n",
		       i,
		       v->name,
		       v->id,
		       v->media_path,
		       v->video_path);
	}
}

static void decoder_vector_reduce(struct vector *decoder_vector)
{
	if (decoder_vector->num_entities > 0 &&
	    decoder_vector->num_entities == decoder_vector->capacity -1) {
		/*
		 * reduce decoder_vector->capacity
		 * and resize the allocated memory accordingly
		 */
		decoder_vector->capacity--;
		decoder_vector->v4l2_decoders = realloc(decoder_vector->v4l2_decoders, sizeof(struct v4l2_decoder) * decoder_vector->capacity);
	}
}

void decoder_vector_set(struct vector *decoder_vector, int index, void *decoder)
{
	/* zero fill the decoder_vector up to the desired index */
	while (index >= decoder_vector->num_entities) {
		decoder_vector_append(decoder_vector, decoder);
	}

	/* memcopy the given source structure at the desired index */
	decoder_vector->v4l2_decoders[index] = decoder;
	//memcpy (decoder_vector->v4l2_decoders[index], decoder, sizeof(decoder));
}
