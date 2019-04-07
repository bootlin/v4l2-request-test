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

#ifndef _V4L2_TOPOLOGY_H_
#define _V4L2_TOPOLOGY_H_

#include <libudev.h>
#include <linux/media.h>

#include "decoder_vector.h"

/*
 * Functions
 */

/* V4L2 */

static inline void *media_get_uptr(uint64_t arg);
//static inline const char *media_gobj_type(uint32_t id);
static inline const char *media_interface_type(uint32_t intf_type);
static inline uint32_t media_localid(uint32_t id);
//static char *media_objname(uint32_t id, char delimiter);
int media_scan_topology(struct v4l2_decoder *decoder);
//static uint32_t media_type(uint32_t id);

/* udev */

static char *udev_get_devpath(struct media_v2_intf_devnode *devnode);

#endif
