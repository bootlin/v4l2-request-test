<!-- v4l2-request-test README.md -->
<!-- version: 0.0.1 -->

# v4l2-request-test

<p align="center">
  <span>English</span> |
  <a href="lang/french#v4l2-request-test">Français</a> | -->
  <a href="lang/german#v4l2-request-test">Deutsch</a>
</p>

## Sur

`v4l2-request-test` was designed as a standalone tool to help with development
and testing of the `Cedrus driver`. The tool supports the Video Engine found on most
Allwinner SoCs. It implements the Linux [V4L2 Request API](https://www.linuxtv.org/downloads/v4l-dvb-apis-new/uapi/mediactl/request-api.html).
So it can help to test any driver being developed using that API.

It currently supports decoding MPEG2 and H264 frames from their slice data
(stored as files) and associated metadata, as well as displaying the decoded
frames through a dedicated DRM plane.

The behavior of the tool can be configured through command line arguments, that
are precised by its usage help.

## How does it work

TODO: summarize the functional structure of the code.

## Presets

### Preset Definition
To ease the testing of supported codecs, `v4l2-request-test` defines a preset structure.
This structure classifies its member as declarative or control fields.

* declarative fields

Declarative struct members document the source of the coded frames.

  * name
  * description
  * license
  * attribution
  * control fields

Each `preset` stores attributes that correspond to the nature of decoded frames. This
infomation will be used to process with correct algorithms.

  * codec type
  * buffer_count,
  * frames
  * frames_count
  * data path

The series of coded frames are stored as files in the `data subdirecotry`. The name of
the subdirectory correspond to the preset `name`.

### Preset Data

There are subdirectories with coded frames for the following presets:

* bbb-mpeg2
* bbb-happy-mpeg2
* ed-mpeg2
* bbb-h264-32
* bbb-h264-all-i-32
* bbb-h264-high-32
* caminandes-fall-h265
* caminandes-h265

## Contributing

Help is very welcome! Feel free to fork and issue a pull request to add
features or tackle open issues.

## License

<!-- License source -->
[Logo-CC_BY]: https://i.creativecommons.org/l/by/4.0/88x31.png "Creative Common Logo"
[License-CC_BY]: https://creativecommons.org/licenses/by/4.0/legalcode "Creative Common License"

This work is licensed under a [Creative Common License 4.0][License-CC_BY]

![Creative Common Logo][Logo-CC_BY]

© 2017 - 2019 Paul Kocialkowski (bootlin);
© 2019        Ralf Zerres (networkx)
