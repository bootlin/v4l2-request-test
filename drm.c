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
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>
#include <sun4i_drm.h>

#include "cedrus-frame-test.h"

static int create_tiled_buffer(int drm_fd, unsigned int width, unsigned int height, unsigned int format, struct gem_buffer *buffer)
{
	struct drm_sun4i_gem_create_tiled create_tiled;
	unsigned int i;
	int rc;

	if (buffer == NULL)
		return -1;

	memset(&create_tiled, 0, sizeof(create_tiled));
	create_tiled.width = width;
	create_tiled.height = height;
	create_tiled.format = format;

	rc = drmIoctl(drm_fd, DRM_IOCTL_SUN4I_GEM_CREATE_TILED, &create_tiled);
	if (rc < 0) {
		fprintf(stderr, "Unable to create tiled buffer: %s\n", strerror(errno));
		return -1;
	}

	buffer->size = create_tiled.size;

	for (i = 0; i < 4; i++) {
		buffer->pitches[i] = create_tiled.pitches[i];
		buffer->offsets[i] = create_tiled.offsets[i];

		if (create_tiled.pitches[i] != 0)
			buffer->handles[i] = create_tiled.handle;
	}

	return 0;
}

static int create_imported_buffer(int drm_fd, int *import_fds, unsigned int import_fds_count, unsigned int width, unsigned int height, struct gem_buffer *buffer)
{
	uint32_t handle;
	unsigned int i;
	int rc;

	memset(buffer->handles, 0, sizeof(buffer->handles));
	memset(buffer->pitches, 0, sizeof(buffer->pitches));
	memset(buffer->offsets, 0, sizeof(buffer->offsets));

	for (i = 0; i < import_fds_count; i++) {
		rc = drmPrimeFDToHandle(drm_fd, import_fds[i], &handle);
		if (rc < 0) {
			fprintf(stderr, "Unable to create imported buffer: %s\n", strerror(errno));
			return -1;
		}

		buffer->handles[i] = handle;
		buffer->pitches[i] = ALIGN(width, 32);
	}

	return 0;
}

static int destroy_buffer(int drm_fd, struct gem_buffer *buffer)
{
	struct drm_mode_destroy_dumb destroy_dumb;
	int rc;

	if (buffer == NULL)
		return -1;

	memset(&destroy_dumb, 0, sizeof(destroy_dumb));
	destroy_dumb.handle = buffer->handles[0];

	rc = drmIoctl(drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_dumb);
	if (rc < 0) {
		fprintf(stderr, "Unable to destroy buffer: %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

static int close_buffer(int drm_fd, struct gem_buffer *buffer)
{
	struct drm_gem_close gem_close;
	int rc;

	memset(&gem_close, 0, sizeof(gem_close));
	gem_close.handle = buffer->handles[0];

	rc = drmIoctl(drm_fd, DRM_IOCTL_GEM_CLOSE, &gem_close);
	if (rc < 0) {
		fprintf(stderr, "Unable to close buffer: %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

static int map_buffer(int drm_fd, struct gem_buffer *buffer)
{
	struct drm_mode_map_dumb map_dumb;
	void *data;
	int rc;

	if (buffer == NULL)
		return -1;

	memset(&map_dumb, 0, sizeof(map_dumb));
	map_dumb.handle = buffer->handles[0];

	rc = drmIoctl(drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &map_dumb);
	if (rc < 0) {
		fprintf(stderr, "Unable to map buffer: %s\n", strerror(errno));
		return -1;
	}

	data = mmap(0, buffer->size, PROT_READ | PROT_WRITE, MAP_SHARED, drm_fd, map_dumb.offset);
	if (data == MAP_FAILED) {
		fprintf(stderr, "Unable to mmap buffer: %s\n", strerror(errno));
		return -1;
	}

	buffer->data = data;

	return 0;
}

static int unmap_buffer(int drm_fd, struct gem_buffer *buffer)
{
	int rc;

	if (buffer == NULL)
		return -1;

	munmap(buffer->data, buffer->size);

	return 0;
}

static int get_crtc_size(int drm_fd, unsigned int crtc_id, unsigned int *width, unsigned int *height)
{
	drmModeCrtcPtr crtc;

	crtc = drmModeGetCrtc(drm_fd, crtc_id);
	if (crtc == NULL) {
		fprintf(stderr, "Unable to get CRTC mode: %s\n", strerror(errno));
		return -1;
	}

	if (width != NULL)
		*width = crtc->width;

	if (height != NULL)
		*height = crtc->height;

	return 0;
}

static int add_framebuffer(int drm_fd, struct gem_buffer *buffer, unsigned int width, unsigned int height, unsigned int format)
{
	uint64_t modifiers[4] = { DRM_FORMAT_MOD_ALLWINNER_MB32_TILED, DRM_FORMAT_MOD_ALLWINNER_MB32_TILED, 0, 0 };
	uint32_t flags = DRM_MODE_FB_MODIFIERS;
	unsigned int id;
	int rc;

	rc = drmModeAddFB2WithModifiers(drm_fd, width, height, format, buffer->handles, buffer->pitches, buffer->offsets, modifiers, &id, flags);
	if (rc < 0) {
		fprintf(stderr, "Unable to add framebuffer for plane: %s\n", strerror(errno));
		return -1;
	}

	buffer->framebuffer_id = id;

	return 0;
}

static int set_plane(int drm_fd, unsigned int crtc_id, unsigned int plane_id, unsigned int framebuffer_id, unsigned int width, unsigned int height, unsigned int x, unsigned int y, unsigned int scaled_width, unsigned int scaled_height)
{
	uint32_t flags = DRM_MODE_FB_MODIFIERS;
	int rc;

	rc = drmModeSetPlane(drm_fd, plane_id, crtc_id, framebuffer_id, flags, x, y, scaled_width, scaled_height, 0, 0, width << 16, height << 16);
	if (rc < 0) {
		fprintf(stderr, "Unable to enable plane: %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

static int page_flip(int drm_fd, unsigned int crtc_id, unsigned int framebuffer_id)
{
	int rc;

	rc = drmModePageFlip(drm_fd, crtc_id, framebuffer_id, 0, NULL);
	if (rc < 0) {
		fprintf(stderr, "Unable to flip page: %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

int display_engine_start(int drm_fd, unsigned int crtc_id, unsigned int plane_id, unsigned int width, unsigned int height, struct video_buffer *video_buffers, unsigned int count, struct gem_buffer **buffers, struct display_setup *setup)
{
	struct video_buffer *video_buffer;
	struct gem_buffer *buffer;
	unsigned int crtc_width, crtc_height;
	unsigned int scaled_width, scaled_height;
	unsigned int x, y;
	unsigned int i, j;
	unsigned int export_fds_count;
	bool use_dmabuf = true;
	int rc;

	/*
	 * Check for DMABUF support first and use as many (imported) gem buffers
	 * as video buffers. Otherwise, fallback to 2 dedicated GEM buffers.
	 */

	for (i = 0; i < count; i++) {
		video_buffer = &video_buffers[i];
		export_fds_count = sizeof(video_buffer->export_fds) / sizeof(*video_buffer->export_fds);

		for (j = 0; j < export_fds_count; j++) {
			if (video_buffer->export_fds[j] < 0) {
				use_dmabuf = false;
				break;
			}
		}
	}

	if (!use_dmabuf)
		count = 2;

	*buffers = malloc(count * sizeof(**buffers));
	memset(*buffers, 0, count * sizeof(**buffers));

	for (i = 0; i < count; i++) {
		buffer = &((*buffers)[i]);
		video_buffer = &video_buffers[i];
		export_fds_count = sizeof(video_buffer->export_fds) / sizeof(*video_buffer->export_fds);

		if (use_dmabuf)
			rc = create_imported_buffer(drm_fd, video_buffer->export_fds, export_fds_count, width, height, buffer);
		else
			rc = create_tiled_buffer(drm_fd, width, height, DRM_FORMAT_NV12, buffer);

		if (rc < 0)
			return -1;

		rc = add_framebuffer(drm_fd, buffer, width, height, DRM_FORMAT_NV12);
		if (rc < 0)
			return -1;

		if (!use_dmabuf) {
			rc = map_buffer(drm_fd, buffer);
			if (rc < 0)
				return -1;
		}
	}

	rc = get_crtc_size(drm_fd, crtc_id, &crtc_width, &crtc_height);
	if (rc < 0) {
		fprintf(stderr, "Unable to get CRTC size\n");
		return -1;
	}

	scaled_height = (height * crtc_width) / width;

	if (scaled_height > crtc_height) {
		/* Scale to CRTC height. */
		scaled_width = (width * crtc_height) / height;
		scaled_height = crtc_height;
	} else {
		/* Scale to CRTC width. */
		scaled_width = crtc_width;
	}

	x = (crtc_width - scaled_width) / 2;
	y = (crtc_height - scaled_height) / 2;

	if (scaled_width != width || scaled_height != height)
		printf("Scaling video from %dx%d to %dx%d+%d+%d\n", width, height, scaled_width, scaled_height, x, y);

	memset(setup, 0, sizeof(*setup));
	setup->crtc_id = crtc_id;
	setup->plane_id = plane_id;
	setup->width = width;
	setup->height = height;
	setup->scaled_width = scaled_width;
	setup->scaled_height = scaled_height;
	setup->x = x;
	setup->y = y;
	setup->buffers_count = count;
	setup->use_dmabuf = use_dmabuf;

	buffer = &((*buffers)[0]);

	rc = set_plane(drm_fd, crtc_id, plane_id, buffer->framebuffer_id, width, height, x, y, scaled_width, scaled_height);
	if (rc < 0) {
		fprintf(stderr, "Unable to set plane\n");
		return -1;
	}

	return 0;
}

int display_engine_stop(int drm_fd, struct gem_buffer *buffers, struct display_setup *setup)
{
	struct gem_buffer *buffer;
	unsigned int i;
	int rc;

	if (buffers == NULL || setup == NULL)
		return -1;

	for (i = 0; i < setup->buffers_count; i++) {
		buffer = &buffers[i];

		if (setup->use_dmabuf) {
			rc = close_buffer(drm_fd, buffer);
			if (rc < 0) {
				fprintf(stderr, "Unable to close buffer %d\n", i);
				return -1;
			}
		} else {
			rc = unmap_buffer(drm_fd, buffer);
			if (rc < 0) {
				fprintf(stderr, "Unable to unmap buffer %d\n", i);
				return -1;
			}

			rc = destroy_buffer(drm_fd, buffer);
			if (rc < 0) {
				fprintf(stderr, "Unable to destroy buffer %d\n", i);
				return -1;
			}
		}
	}

	return 0;
}

int display_engine_show(int drm_fd, unsigned int index, struct video_buffer *video_buffers, struct gem_buffer *buffers, struct display_setup *setup)
{
	struct video_buffer *video_buffer;
	struct gem_buffer *buffer;
	unsigned int destination_size[2];
	unsigned int size;
	unsigned int i;
	int rc;

	if (buffers == NULL || setup == NULL)
		return -1;

	// FIXME: Page flip GEM buffers
	if (setup->use_dmabuf)
		return 0;

	video_buffer = &video_buffers[index];
	buffer = index % 2 == 0 ? &buffers[0] : &buffers[1];

	/* Use a single buffer for now. */
	buffer = &buffers[0];

	destination_size[0] = ALIGN(setup->width, 32) * ALIGN(setup->height, 32);
	destination_size[1] = ALIGN(setup->width, 32) * ALIGN(DIV_ROUND_UP(setup->height, 2), 32);

	size = destination_size[0] + destination_size[1];
	if (size > buffer->size) {
		fprintf(stderr, "Display data size is larger than buffer\n");
		return -1;
	}

	for (i = 0; i < 2; i++)
		memcpy((unsigned char *) buffer->data + buffer->offsets[i], video_buffer->destination_data[i], destination_size[i]);

	return 0;
}
