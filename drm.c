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

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <drm_fourcc.h>
#include <sun4i_drm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "v4l2-request-test.h"

static int create_dumb_buffer(int drm_fd, unsigned int width,
			      unsigned int height, unsigned int bpp,
			      struct gem_buffer *buffer)
{
	struct drm_mode_create_dumb create_dumb;
	int rc;

	memset(&create_dumb, 0, sizeof(create_dumb));
	create_dumb.width = width;
	create_dumb.height = height;
	create_dumb.bpp = bpp;

	rc = drmIoctl(drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_dumb);
	if (rc < 0) {
		fprintf(stderr, "Unable to create dumb buffer: %s\n",
			strerror(errno));
		return -1;
	}

	buffer->size = create_dumb.size;
	buffer->pitches[0] = create_dumb.pitch;
	buffer->offsets[0] = 0;
	buffer->handles[0] = create_dumb.handle;

	return 0;
}

static int create_tiled_buffer(int drm_fd, unsigned int width,
			       unsigned int height, unsigned int format,
			       struct gem_buffer *buffer)
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
		fprintf(stderr, "Unable to create tiled buffer: %s\n",
			strerror(errno));
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

static int create_imported_buffer(int drm_fd, int *import_fds,
				  unsigned int import_fds_count,
				  unsigned int *offsets, unsigned int *pitches,
				  struct gem_buffer *buffer)
{
	uint32_t handles[4];
	unsigned int i;
	int rc;

	memset(buffer->handles, 0, sizeof(buffer->handles));
	memset(buffer->pitches, 0, sizeof(buffer->pitches));
	memset(buffer->offsets, 0, sizeof(buffer->offsets));

	for (i = 0; i < import_fds_count; i++) {
		rc = drmPrimeFDToHandle(drm_fd, import_fds[i], &handles[i]);
		if (rc < 0) {
			fprintf(stderr, "Unable to create imported buffer: %s\n",
				strerror(errno));
			return -1;
		}
	}

	for (i = 0; i < buffer->planes_count; i++) {
		buffer->handles[i] = import_fds_count == 1 ? handles[0] :
							     handles[i];
		buffer->pitches[i] = pitches[i];
		buffer->offsets[i] = offsets[i];
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
		fprintf(stderr, "Unable to destroy buffer: %s\n",
			strerror(errno));
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
		fprintf(stderr, "Unable to close buffer: %s\n",
			strerror(errno));
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

	data = mmap(0, buffer->size, PROT_READ | PROT_WRITE, MAP_SHARED, drm_fd,
		    map_dumb.offset);
	if (data == MAP_FAILED) {
		fprintf(stderr, "Unable to mmap buffer: %s\n", strerror(errno));
		return -1;
	}

	buffer->data = data;

	return 0;
}

static int unmap_buffer(int drm_fd, struct gem_buffer *buffer)
{
	if (buffer == NULL)
		return -1;

	munmap(buffer->data, buffer->size);

	return 0;
}

static int add_framebuffer(int drm_fd, struct gem_buffer *buffer,
			   unsigned int width, unsigned int height,
			   unsigned int format, uint64_t modifier)
{
	uint64_t modifiers[4] = { 0, 0, 0, 0 };
	uint32_t flags = 0;
	unsigned int id;
	unsigned int i;
	int rc;

	for (i = 0; i < buffer->planes_count; i++) {
		if (buffer->handles[i] != 0 &&
		    modifier != DRM_FORMAT_MOD_NONE) {
			flags |= DRM_MODE_FB_MODIFIERS;
			modifiers[i] = modifier;
		}
	}

	rc = drmModeAddFB2WithModifiers(drm_fd, width, height, format,
					buffer->handles, buffer->pitches,
					buffer->offsets, modifiers, &id, flags);
	if (rc < 0) {
		fprintf(stderr, "Unable to add framebuffer for plane: %s\n",
			strerror(errno));
		return -1;
	}

	buffer->framebuffer_id = id;

	return 0;
}

static int discover_properties(int drm_fd, int connector_id, int crtc_id,
			       int plane_id, struct display_properties_ids *ids)
{
	drmModeObjectPropertiesPtr properties = NULL;
	drmModePropertyPtr property = NULL;
	struct {
		uint32_t object_type;
		uint32_t object_id;
		char *name;
		uint32_t *value;
	} glue[] = {
		{ DRM_MODE_OBJECT_CONNECTOR, connector_id, "CRTC_ID", &ids->connector_crtc_id },
		{ DRM_MODE_OBJECT_CRTC, crtc_id, "MODE_ID", &ids->crtc_mode_id },
		{ DRM_MODE_OBJECT_CRTC, crtc_id, "ACTIVE", &ids->crtc_active },
		{ DRM_MODE_OBJECT_PLANE, plane_id, "FB_ID", &ids->plane_fb_id },
		{ DRM_MODE_OBJECT_PLANE, plane_id, "CRTC_ID", &ids->plane_crtc_id },
		{ DRM_MODE_OBJECT_PLANE, plane_id, "SRC_X", &ids->plane_src_x },
		{ DRM_MODE_OBJECT_PLANE, plane_id, "SRC_Y", &ids->plane_src_y },
		{ DRM_MODE_OBJECT_PLANE, plane_id, "SRC_W", &ids->plane_src_w },
		{ DRM_MODE_OBJECT_PLANE, plane_id, "SRC_H", &ids->plane_src_h },
		{ DRM_MODE_OBJECT_PLANE, plane_id, "CRTC_X", &ids->plane_crtc_x },
		{ DRM_MODE_OBJECT_PLANE, plane_id, "CRTC_Y", &ids->plane_crtc_y },
		{ DRM_MODE_OBJECT_PLANE, plane_id, "CRTC_W", &ids->plane_crtc_w },
		{ DRM_MODE_OBJECT_PLANE, plane_id, "CRTC_H", &ids->plane_crtc_h },
		{ DRM_MODE_OBJECT_PLANE, plane_id, "zpos", &ids->plane_zpos },
	};
	unsigned int i, j;
	int rc;

	for (i = 0; i < ARRAY_SIZE(glue); i++) {
		properties = drmModeObjectGetProperties(drm_fd,
							glue[i].object_id,
							glue[i].object_type);
		if (properties == NULL) {
			fprintf(stderr, "Unable to get DRM properties: %s\n",
				strerror(errno));
			goto error;
		}

		for (j = 0; j < properties->count_props; j++) {
			property = drmModeGetProperty(drm_fd,
						      properties->props[j]);
			if (property == NULL) {
				fprintf(stderr, "Unable to get DRM property: %s\n",
					strerror(errno));
				goto error;
			}

			if (strcmp(property->name, glue[i].name) == 0) {
				*glue[i].value = property->prop_id;
				break;
			}

			drmModeFreeProperty(property);
			property = NULL;
		}

		if (j == properties->count_props) {
			fprintf(stderr, "Unable to find property for %s\n",
				glue[i].name);
			goto error;
		}

		drmModeFreeProperty(property);
		property = NULL;

		drmModeFreeObjectProperties(properties);
		properties = NULL;
	}

	rc = 0;
	goto complete;

error:
	rc = -1;

complete:
	if (property != NULL)
		drmModeFreeProperty(property);

	if (properties != NULL)
		drmModeFreeObjectProperties(properties);

	return rc;
}

static int commit_atomic_mode(int drm_fd, unsigned int connector_id,
			      unsigned int crtc_id, unsigned int plane_id,
			      struct display_properties_ids *ids,
			      unsigned int framebuffer_id, unsigned int width,
			      unsigned int height, unsigned int x,
			      unsigned int y, unsigned int scaled_width,
			      unsigned int scaled_height, unsigned int zpos)
{
	drmModeAtomicReqPtr request;
	uint32_t flags = DRM_MODE_ATOMIC_ALLOW_MODESET;
	int rc;

	request = drmModeAtomicAlloc();

	drmModeAtomicAddProperty(request, plane_id, ids->plane_fb_id,
				 framebuffer_id);
	drmModeAtomicAddProperty(request, plane_id, ids->plane_crtc_id,
				 crtc_id);
	drmModeAtomicAddProperty(request, plane_id, ids->plane_src_x, 0);
	drmModeAtomicAddProperty(request, plane_id, ids->plane_src_y, 0);
	drmModeAtomicAddProperty(request, plane_id, ids->plane_src_w,
				 width << 16);
	drmModeAtomicAddProperty(request, plane_id, ids->plane_src_h,
				 height << 16);
	drmModeAtomicAddProperty(request, plane_id, ids->plane_crtc_x, x);
	drmModeAtomicAddProperty(request, plane_id, ids->plane_crtc_y, y);
	drmModeAtomicAddProperty(request, plane_id, ids->plane_crtc_w,
				 scaled_width);
	drmModeAtomicAddProperty(request, plane_id, ids->plane_crtc_h,
				 scaled_height);
	drmModeAtomicAddProperty(request, plane_id, ids->plane_zpos, zpos);

	rc = drmModeAtomicCommit(drm_fd, request, flags, NULL);
	if (rc < 0) {
		fprintf(stderr, "Unable to commit atomic mode: %s\n",
			strerror(errno));
		goto error;
	}

	rc = 0;
	goto complete;

error:
	rc = -1;

complete:
	drmModeAtomicFree(request);

	return rc;
}

static int page_flip(int drm_fd, unsigned int crtc_id, unsigned int plane_id,
		     struct display_properties_ids *ids,
		     unsigned int framebuffer_id)
{
	drmModeAtomicReqPtr request;
	uint32_t flags = DRM_MODE_ATOMIC_ALLOW_MODESET;
	int rc;

	request = drmModeAtomicAlloc();

	drmModeAtomicAddProperty(request, plane_id, ids->plane_fb_id,
				 framebuffer_id);
	drmModeAtomicAddProperty(request, plane_id, ids->plane_crtc_id,
				 crtc_id);

	rc = drmModeAtomicCommit(drm_fd, request, flags, NULL);
	if (rc < 0) {
		fprintf(stderr, "Unable to flip page: %s\n", strerror(errno));
		goto error;
	}

	rc = 0;
	goto complete;

error:
	rc = -1;

complete:
	drmModeAtomicFree(request);

	return rc;
}

static int select_connector_encoder(int drm_fd, unsigned int *connector_id,
				    unsigned int *encoder_id)
{
	drmModeResPtr ressources = NULL;
	drmModeConnectorPtr connector = NULL;
	unsigned int i;
	int rc;

	ressources = drmModeGetResources(drm_fd);
	if (ressources == NULL) {
		fprintf(stderr, "Unable to get DRM ressources: %s\n",
			strerror(errno));
		goto error;
	}

	for (i = 0; i < ressources->count_connectors; i++) {
		connector = drmModeGetConnector(drm_fd,
						ressources->connectors[i]);
		if (connector == NULL) {
			fprintf(stderr, "Unable to get DRM connector %d: %s\n",
				ressources->connectors[i], strerror(errno));
			goto error;
		}

		if (connector->connection == DRM_MODE_CONNECTED)
			break;

		drmModeFreeConnector(connector);
		connector = NULL;
	}

	if (connector == NULL || i == ressources->count_connectors) {
		fprintf(stderr, "Unable to find any connected connector\n");
		goto error;
	}

	if (connector_id != NULL)
		*connector_id = connector->connector_id;

	if (encoder_id != NULL)
		*encoder_id = connector->encoder_id;

	rc = 0;
	goto complete;

error:
	rc = -1;

complete:
	if (connector != NULL)
		drmModeFreeConnector(connector);

	if (ressources != NULL)
		drmModeFreeResources(ressources);

	return rc;
}

static int select_crtc(int drm_fd, unsigned int encoder_id,
		       unsigned int *crtc_id, drmModeModeInfoPtr mode)
{
	drmModeEncoderPtr encoder = NULL;
	drmModeCrtcPtr crtc = NULL;
	int rc;

	encoder = drmModeGetEncoder(drm_fd, encoder_id);
	if (encoder == NULL) {
		fprintf(stderr, "Unable to get DRM encoder: %s\n",
			strerror(errno));
		goto error;
	}

	if (crtc_id != NULL)
		*crtc_id = encoder->crtc_id;

	crtc = drmModeGetCrtc(drm_fd, encoder->crtc_id);
	if (crtc == NULL) {
		fprintf(stderr, "Unable to get CRTC mode: %s\n",
			strerror(errno));
		goto error;
	}

	if (!crtc->mode_valid) {
		fprintf(stderr, "Unable to get valid mode for CRTC %d\n",
			crtc_id);
		goto error;
	}

	if (mode != NULL)
		memcpy(mode, &crtc->mode, sizeof(drmModeModeInfo));

	rc = 0;
	goto complete;

error:
	rc = -1;

complete:
	if (encoder != NULL)
		drmModeFreeEncoder(encoder);

	if (crtc != NULL)
		drmModeFreeCrtc(crtc);

	return rc;
}

static int select_plane(int drm_fd, unsigned int crtc_id, unsigned int format,
			unsigned int *plane_id, unsigned int *zpos)
{
	drmModeResPtr ressources = NULL;
	drmModePlaneResPtr plane_ressources = NULL;
	drmModePlanePtr plane = NULL;
	drmModeObjectPropertiesPtr properties = NULL;
	drmModePropertyPtr property = NULL;
	unsigned int zpos_primary = 0;
	unsigned int zpos_value;
	unsigned int crtc_index;
	unsigned int type;
	unsigned int i, j;
	bool format_found;
	int rc;

	ressources = drmModeGetResources(drm_fd);
	if (ressources == NULL) {
		fprintf(stderr, "Unable to get DRM ressources: %s\n",
			strerror(errno));
		goto error;
	}

	for (i = 0; i < ressources->count_crtcs; i++) {
		if (ressources->crtcs[i] == crtc_id) {
			crtc_index = i;
			break;
		}
	}

	if (i == ressources->count_crtcs) {
		fprintf(stderr, "Unable to find CRTC index for CRTC %d\n",
			crtc_id);
		goto error;
	}

	plane_ressources = drmModeGetPlaneResources(drm_fd);
	if (plane_ressources == NULL) {
		fprintf(stderr, "Unable to get DRM plane ressources: %s\n",
			strerror(errno));
		goto error;
	}

	for (i = 0; i < plane_ressources->count_planes; i++) {
		plane = drmModeGetPlane(drm_fd, plane_ressources->planes[i]);
		if (plane == NULL) {
			fprintf(stderr, "Unable to get DRM plane %d: %s\n",
				plane_ressources->planes[i], strerror(errno));
			goto error;
		}

		if ((plane->possible_crtcs & (1 << crtc_index)) == 0) {
			drmModeFreePlane(plane);
			plane = NULL;
			continue;
		}

		properties = drmModeObjectGetProperties(drm_fd,
							plane_ressources->planes[i],
							DRM_MODE_OBJECT_PLANE);
		if (properties == NULL) {
			fprintf(stderr,
				"Unable to get DRM plane %d properties: %s\n",
				plane_ressources->planes[i], strerror(errno));
			goto error;
		}

		zpos_value = zpos_primary;

		for (j = 0; j < properties->count_props; j++) {
			property = drmModeGetProperty(drm_fd,
						      properties->props[j]);
			if (property == NULL) {
				fprintf(stderr,
					"Unable to get DRM plane %d property: %s\n",
					plane_ressources->planes[i],
					strerror(errno));
				goto error;
			}

			if (strcmp(property->name, "type") == 0) {
				type = properties->prop_values[j];
				break;
			} else if (strcmp(property->name, "zpos") == 0) {
				zpos_value = properties->prop_values[j];
				break;
			}

			drmModeFreeProperty(property);
			property = NULL;
		}

		if (j == properties->count_props) {
			fprintf(stderr,
				"Unable to find plane %d type property\n",
				plane_ressources->planes[i]);
			goto error;
		}

		if (property != NULL) {
			drmModeFreeProperty(property);
			property = NULL;
		}

		drmModeFreeObjectProperties(properties);
		properties = NULL;

		if (type == DRM_PLANE_TYPE_PRIMARY)
			zpos_primary = zpos_value;

		if (type != DRM_PLANE_TYPE_OVERLAY)
			continue;

		format_found = false;

		for (j = 0; j < plane->count_formats; j++)
			if (plane->formats[j] == format)
				format_found = true;

		if (format_found)
			break;

		drmModeFreePlane(plane);
		plane = NULL;
	}

	if (plane == NULL || i == plane_ressources->count_planes) {
		fprintf(stderr, "Unable to find any plane for CRTC %d\n",
			crtc_id);
		goto error;
	}

	if (plane_id != NULL)
		*plane_id = plane->plane_id;

	if (zpos != NULL) {
		if (zpos_value <= zpos_primary)
			zpos_value = zpos_primary + 1;

		*zpos = zpos_value;
	}

	rc = 0;
	goto complete;

error:
	rc = -1;

complete:
	if (property != NULL)
		drmModeFreeProperty(property);

	if (properties != NULL)
		drmModeFreeObjectProperties(properties);

	if (plane != NULL)
		drmModeFreePlane(plane);

	if (ressources != NULL)
		drmModeFreeResources(ressources);

	return rc;
}

int display_engine_start(int drm_fd, unsigned int width, unsigned int height,
			 struct format_description *format,
			 struct video_buffer *video_buffers, unsigned int count,
			 struct gem_buffer **buffers,
			 struct display_setup *setup)
{
	struct video_buffer *video_buffer;
	struct gem_buffer *buffer;
	unsigned int crtc_width, crtc_height;
	unsigned int scaled_width, scaled_height;
	unsigned int x, y;
	unsigned int i, j;
	unsigned int zpos;
	unsigned int export_fds_count;
	unsigned int connector_id;
	unsigned int encoder_id;
	unsigned int crtc_id;
	unsigned int plane_id;
	bool use_dmabuf = true;
	drmModeModeInfo mode;
	int rc;

	rc = drmSetClientCap(drm_fd, DRM_CLIENT_CAP_ATOMIC, 1);
	if (rc < 0) {
		fprintf(stderr, "Unable to set DRM atomic capability: %s\n",
			strerror(errno));
		return -1;
	}

	rc = drmSetClientCap(drm_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
	if (rc < 0) {
		fprintf(stderr,
			"Unable to set DRM universal planes capability: %s\n",
			strerror(errno));
		return -1;
	}

	rc = select_connector_encoder(drm_fd, &connector_id, &encoder_id);
	if (rc < 0) {
		fprintf(stderr, "Unable to select DRM connector/encoder\n");
		return -1;
	}

	rc = select_crtc(drm_fd, encoder_id, &crtc_id, &mode);
	if (rc < 0) {
		fprintf(stderr, "Unable to selec DRM CRTC\n");
		return -1;
	}

	rc = select_plane(drm_fd, crtc_id, format->drm_format, &plane_id,
			  &zpos);
	if (rc < 0) {
		fprintf(stderr, "Unable to select DRM plane for CRTC %d\n",
			crtc_id);
		return -1;
	}

	crtc_width = mode.hdisplay;
	crtc_height = mode.vdisplay;

	memset(setup, 0, sizeof(*setup));

	rc = discover_properties(drm_fd, connector_id, crtc_id, plane_id,
				 &setup->properties_ids);
	if (rc < 0) {
		fprintf(stderr, "Unable to discover DRM properties\n");
		return -1;
	}

	/*
	 * Check for DMABUF support first and use as many (imported) gem buffers
	 * as video buffers. Otherwise, fallback to 2 dedicated GEM buffers.
	 */

	for (i = 0; i < count; i++) {
		video_buffer = &video_buffers[i];
		export_fds_count = video_buffer->destination_buffers_count;

		for (j = 0; j < export_fds_count; j++) {
			if (video_buffer->export_fds[j] < 0) {
				use_dmabuf = false;
				break;
			}
		}
	}

	/* Use double-buffering without DMABUF. */
	if (!use_dmabuf)
		count = 2;

	*buffers = malloc(count * sizeof(**buffers));
	memset(*buffers, 0, count * sizeof(**buffers));

	for (i = 0; i < count; i++) {
		buffer = &((*buffers)[i]);
		video_buffer = &video_buffers[i];
		export_fds_count = video_buffer->destination_buffers_count;

		buffer->planes_count = format->planes_count;

		if (use_dmabuf)
			rc = create_imported_buffer(drm_fd,
						    video_buffer->export_fds,
						    export_fds_count,
						    video_buffer->destination_offsets,
						    video_buffer->destination_bytesperlines,
						    buffer);
		else if (format->drm_modifier == DRM_FORMAT_MOD_ALLWINNER_TILED)
			rc = create_tiled_buffer(drm_fd, width, height,
						 format->drm_format, buffer);
		else
			rc = create_dumb_buffer(drm_fd, width, height,
						format->bpp, buffer);

		if (rc < 0) {
			fprintf(stderr,
				"Unable to create or import DRM buffer\n");
			return -1;
		}

		rc = add_framebuffer(drm_fd, buffer, width, height,
				     format->drm_format, format->drm_modifier);
		if (rc < 0) {
			fprintf(stderr, "Unable to add DRM framebuffer\n");
			return -1;
		}

		if (!use_dmabuf) {
			rc = map_buffer(drm_fd, buffer);
			if (rc < 0) {
				fprintf(stderr, "Unable to map DRM buffer\n");
				return -1;
			}
		}
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
		printf("Scaling video from %dx%d to %dx%d+%d+%d\n", width,
		       height, scaled_width, scaled_height, x, y);

	buffer = &((*buffers)[0]);

	rc = commit_atomic_mode(drm_fd, connector_id, crtc_id, plane_id,
				&setup->properties_ids, buffer->framebuffer_id,
				width, height, x, y, scaled_width,
				scaled_height, zpos);
	if (rc < 0) {
		fprintf(stderr, "Unable to commit initial plane\n");
		return -1;
	}

	setup->connector_id = connector_id;
	setup->encoder_id = encoder_id;
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

	return 0;
}

int display_engine_stop(int drm_fd, struct gem_buffer *buffers,
			struct display_setup *setup)
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

int display_engine_show(int drm_fd, unsigned int index,
			struct video_buffer *video_buffers,
			struct gem_buffer *buffers, struct display_setup *setup)
{
	struct video_buffer *video_buffer;
	struct gem_buffer *buffer;
	unsigned int i;
	int rc;

	if (buffers == NULL || setup == NULL)
		return -1;

	video_buffer = &video_buffers[index];
	buffer = &buffers[index];

	if (!setup->use_dmabuf) {
		for (i = 0; i < buffer->planes_count; i++)
			memcpy((unsigned char *)buffer->data +
				       buffer->offsets[i],
			       video_buffer->destination_data[i],
			       video_buffer->destination_sizes[i]);

		buffer = index % 2 == 0 ? &buffers[0] : &buffers[1];
	}

	rc = page_flip(drm_fd, setup->crtc_id, setup->plane_id,
		       &setup->properties_ids, buffer->framebuffer_id);
	if (rc < 0) {
		fprintf(stderr, "Unable to flip page to framebuffer %d\n",
			buffer->framebuffer_id);
		return -1;
	}

	return 0;
}
