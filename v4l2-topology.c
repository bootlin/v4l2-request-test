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
 *
 * based on code:
 * Media controller Next Generation test app
 * Copyright (C) 2015 Mauro Carvalho Chehab <mchehab@osg.samsung.com>
 */

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <libudev.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <linux/videodev2.h>

#include "v4l2-request-test.h"
#include "v4l2-topology.h"
#include "decoder_vector.h"

static inline void *media_get_uptr(uint64_t arg)
{
	return (void *)(uintptr_t)arg;
}

enum media_gobj_type {
	MEDIA_GRAPH_ENTITY,
	MEDIA_GRAPH_PAD,
	MEDIA_GRAPH_LINK,
	MEDIA_GRAPH_INTF_DEVNODE,
};

static uint32_t media_type(uint32_t id)
{
	return id >> 24;
}

/*
static inline const char *media_gobj_type(uint32_t id)
{
	switch (media_type(id)) {
	case MEDIA_GRAPH_ENTITY:
		return "entity";
	case MEDIA_GRAPH_PAD:
		return "pad";
	case MEDIA_GRAPH_LINK:
		return "link";
	case MEDIA_GRAPH_INTF_DEVNODE:
		return "devnode";
	default:
		return "unknown interface type";
	}
}
*/

static inline const char *media_interface_type(uint32_t intf_type)
{
	switch (intf_type) {
	case MEDIA_INTF_T_DVB_FE:
		return "frontend";
	case MEDIA_INTF_T_DVB_DEMUX:
		return "demux";
	case MEDIA_INTF_T_DVB_DVR:
		return "DVR";
	case MEDIA_INTF_T_DVB_CA:
		return  "CA";
	case MEDIA_INTF_T_DVB_NET:
		return "dvbnet";

	case MEDIA_INTF_T_V4L_VIDEO:
		return "video";
	case MEDIA_INTF_T_V4L_VBI:
		return "vbi";
	case MEDIA_INTF_T_V4L_RADIO:
		return "radio";
	case MEDIA_INTF_T_V4L_SUBDEV:
		return "v4l2-subdev";
	case MEDIA_INTF_T_V4L_SWRADIO:
		return "swradio";

	case MEDIA_INTF_T_ALSA_PCM_CAPTURE:
		return "pcm-capture";
	case MEDIA_INTF_T_ALSA_PCM_PLAYBACK:
		return "pcm-playback";
	case MEDIA_INTF_T_ALSA_CONTROL:
		return "alsa-control";
	case MEDIA_INTF_T_ALSA_COMPRESS:
		return "compress";
	case MEDIA_INTF_T_ALSA_RAWMIDI:
		return "rawmidi";
	case MEDIA_INTF_T_ALSA_HWDEP:
		return "hwdep";
	case MEDIA_INTF_T_ALSA_SEQUENCER:
		return "sequencer";
	case MEDIA_INTF_T_ALSA_TIMER:
		return "ALSA timer";
	default:
		return "unknown_intf";
	}
}

static inline uint32_t media_localid(uint32_t id)
{
	return id & 0xffffff;
}

/*
static char *media_objname(uint32_t id, char delimiter)
{
	char *name;
	int ret;

	ret = asprintf(&name, "%s%c%d",
		       media_gobj_type(id),
		       delimiter,
		       media_localid(id));
	if (ret < 0)
		return NULL;

	return name;
}
*/

int media_scan_topology(struct v4l2_decoder *decoder)
{
	/* https://www.kernel.org/doc/html/v5.0/media/uapi/mediactl/media-ioc-g-topology.html */
	struct media_v2_topology *topology = NULL;
	struct media_device_info *device = NULL;
	int i = 0;
	int j = 0;
	int media_fd = -1;
	int rc = 0;
	bool is_decoder = false;
	__u64 topology_version;

	fprintf(stderr, "Scan topology for media-device %s ...\n",
		//decoder_vector->v4l2_decoders->media_path);
		decoder->media_path);

	//media_fd = open(decoder_vector->v4l2_decoders->media_path, O_RDWR | O_NONBLOCK, 0);
	media_fd = open(decoder->media_path, O_RDWR | O_NONBLOCK, 0);
	if (media_fd < 0) {
		fprintf(stderr, "Unable to open media node: %s (%d)\n",
			strerror(errno), errno);
		return rc;
	}

	device = calloc(1, sizeof(struct media_device_info));
	rc = ioctl(media_fd, MEDIA_IOC_DEVICE_INFO, device);
	if (rc < 0) {
		fprintf(stderr, " error: media device info can't be initialized!\n");
		return rc;
	}

	printf(" driver: %s (model: %s, bus: %s, api-version: %d, driver-version: %d)\n",
	       device->driver,
	       device->model,
	       device->bus_info,
	       device->media_version,
	       device->driver_version);

	/*
	 * Initialize topology structure
	 * a) zero the topology structure
	 * b) ioctl to get amount of elements
	 */
	topology = calloc(1, sizeof(struct media_v2_topology));

	rc = ioctl(media_fd, MEDIA_IOC_G_TOPOLOGY, topology);
	if (rc < 0) {
		fprintf(stderr, " error: topology can't be initialized!\n");
		return rc;
	}

	topology_version = topology->topology_version;
	printf(" topology: version %lld (entries: %d, interfaces: %d, pads: %d, links: %d\n",
	       topology->topology_version,
	       topology->num_entities,
	       topology->num_interfaces,
	       topology->num_pads,
	       topology->num_links);

	/*
	 * Update topology structures
	 * a) set pointers to media structures we are interested in (mem-allocation)
	 * b) ioctl to update structure element values
	 */
	do {
		if(topology->num_entities > 0)
			topology->ptr_entities = (__u64)calloc(topology->num_entities,
							      sizeof(struct media_v2_entity));
		if (topology->num_entities && !topology->ptr_entities)
			goto error;

		if(topology->num_interfaces > 0)
			topology->ptr_interfaces = (__u64)calloc(topology->num_interfaces,
								sizeof(struct media_v2_interface));
		if (topology->num_interfaces && !topology->ptr_interfaces)
			goto error;

		/* not interested in other structures */
		topology->ptr_pads = 0;
		topology->ptr_links = 0;

		/* 2nd call: get updated topology structure elements */
		rc = ioctl(media_fd, MEDIA_IOC_G_TOPOLOGY, topology);
		if (rc < 0) {
			if (topology->topology_version != topology_version) {
				fprintf(stderr, " Topology changed from version %lld to %lld. Trying again.\n",
					topology_version,
					topology->topology_version);
				free((void *)topology->ptr_entities);
				free((void *)topology->ptr_interfaces);
				topology_version = topology->topology_version;
				continue;
			}
			fprintf(stderr, " Topology %lld update error!\n",
				topology_version);
			goto error;
		}
	} while (rc < 0);

	/*
	 * Pick up all available video decoder entities, that support
	 * function 'MEDIA_ENT_F_PROC_VIDEO_DECODER':
	 * -> decompresing a compressd video stream into uncompressed video frames
	 * -> has one sink
	 * -> at least one source
	 */
	struct media_v2_entity *entities = media_get_uptr(topology->ptr_entities);
	for(i=0; i < topology->num_entities; i++) {
		struct media_v2_entity *entity = &entities[i];
		if (entity->function == MEDIA_ENT_F_PROC_VIDEO_DECODER) {
			is_decoder = true;
			rc = 1;
			decoder->id = entity->id;
			asprintf(&decoder->name, "%s", entity->name);
		}
		/*
		   else
			fprintf(stderr, " entity: %s (id: %d) can't decode\n",
				entity->name,
				entity->id);
		*/
	}

	/*
	 * Pick interface
	 * -> type: 'MEDIA_INTF_T_V4L_VIDEO'
	 * -> device: typically /dev/video?
	 */
	if (is_decoder) {
		struct media_v2_interface *interfaces = media_get_uptr(topology->ptr_interfaces);
		for (j=0; j < topology->num_interfaces; j++) {
			struct media_v2_interface *interface = &interfaces[j];
			struct media_v2_intf_devnode *devnode = &interface->devnode;
			//char *obj;
			char *video_path;

			if (interface->intf_type == MEDIA_INTF_T_V4L_VIDEO) {
				/* TODO: cedurs proc doesn't assing devnode->minor
				   fprintf(stderr, " devnode: major->%d, minor->%d\n",
				   devnode->major,
				   devnode->minor);
				*/
				video_path = udev_get_devpath(devnode);
				asprintf(&decoder->video_path, "%s", video_path);

				fprintf(stderr, " interface: type %s, device %s\n",
					media_interface_type(interface->intf_type),
					video_path);

				/*
				  obj = media_objname(interface->id, '#');
				  fprintf(stderr, "%s: interface-type %s, device %s\n",
				  obj,
				  media_interface_type(interface->intf_type),
				  video_path);
				*/
			}
		}
	}

	goto complete;
	//printf("Total number of suitable Video-Decoders: %u\n", index);

error:
	rc = -1;

complete:
	if (topology->ptr_entities) {
		free((void *)topology->ptr_entities);
		topology->ptr_entities = 0;
	}
	if (topology->ptr_interfaces) {
		free((void *)topology->ptr_interfaces);
		topology->ptr_interfaces = 0;
		}
	/*
	if (topology->ptr_pads)
		free((void *)topology->ptr_pads);
	if (topology->ptr_links)
		free((void *)topology->ptr_links);
	*/

	//topology->ptr_pads = 0;
	//topology->ptr_links = 0;

	if (device)
		free((void *)device);

	if (media_fd >= 0)
		close(media_fd);

	return rc;
}

static char *udev_get_devpath(struct media_v2_intf_devnode *devnode)
{
	struct udev *udev;
	struct udev_device *device;
	dev_t devnum;
	const char *ptr_devname;
	char *devname = NULL;
	int rs;

	udev = udev_new();
	if (!udev) {
		fprintf(stderr, " Can’t create udev object\n");
		return NULL;
	}

	devnum = makedev(devnode->major, devnode->minor);
	device = udev_device_new_from_devnum(udev, 'c', devnum);
	if (device) {
		ptr_devname = udev_device_get_devnode(device);
		if (ptr_devname) {
			rs = asprintf(&devname, "%s", ptr_devname);
			if (rs < 0)
				return NULL;
		}
	}

	udev_device_unref(device);

	return devname;
}
