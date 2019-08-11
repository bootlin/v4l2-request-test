# Copyright (C) 2019 Bootlin
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sub license, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice (including the
# next paragraph) shall be included in all copies or substantial portions
# of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
# IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
# ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

project('v4l2-request-test', 'c',
	version: '1.0.0',
	meson_version: '>= 0.43.0')

cc = meson.get_compiler('c')

libdrm_dependency = dependency('libdrm',
	version: '>= 2.4.52',
	method: 'pkg-config',
	required: true)

libudev_dependency = dependency('libudev',
	method: 'pkg-config',
	required: true)

sources = [
	'drm.c',
	'presets.c',
	'decoder_vector.c',
	'v4l2.c',
	'v4l2-topology.c',
	'v4l2-request-test.c'
]

headers = [
	'decoder_vector.h',
	'v4l2-topology.h',
	'v4l2-request-test.h'
]

includes = [
	include_directories('include')
]

cflags = [
	'-Wall',
	'-fvisibility=hidden'
]

deps = [
	libdrm_dependency,
	libudev_dependency
]

v4l2_request_test = executable('v4l2-request-test',
	name_prefix: '',
	install: true,
	install_dir: '/usr/bin/',
	c_args: cflags,
	sources: [ sources, headers ],
	include_directories: includes,
	dependencies: deps)

decoder_sources = [
	'decoder_vector.c',
	'decoder_test.c'
]

decoder_headers = [
	'decoder_vector.h',
	'decoder_test.h'
]

decoder_vector = executable('decoder_vector',
	name_prefix: '',
	install: true,
	install_dir: '/usr/bin/',
	c_args: cflags,
	sources: [ decoder_sources, decoder_headers ],
	include_directories: includes,
	dependencies: deps)

test('decoder_vector test',
	decoder_vector)

#v4l2_request_test_data = data('v4l2_request_test',)
