# RawSourcePlus

### Loading raw video data from files

for Avisynth+ 3.7.5 or greater.
This filter is only for Avisynth+. Avisynth2.6 is not supported.

### requirements
	- Windows 7 sp1 or later
	- Avisynth+ 3.7.5 or greater
	- Visual C++ Redistributable Packages for Visual Studio 2022
	- AVX2 capable CPU

### How to use
```
RawSourcePlus(string "file", int "width", int "height", string "pixel_type", int "fpsnum", int "fpsden", string "index", bool "show", int "sarnum", int "sarden", int "frames")
```

RawSourcePlus opens a video file which contains 8bit, 9bit, 10bit, 16bit or float YUV444, YUV422, YUV411, YUV420, Gray, or RGB video data.
There are four ways how the positions of the video frame data are calculated:

	1. width, height and the others is given as arguments. Then the positions are calculated assuming that only video data is in the file. These values ​​will be overwritten if a valid header or filename are present.
		int "width": specify image width. default value is 720.
		int "height": specify image height. default value is 576.
		string "pixel_type": specify image format. see below "Supported pixel types" section for details.
		int "fpsnum": specify framerate numarator. default value is 25.
		int "fpsden": specify framerate denominator. default value is 1.
		string "index": specify the byte position of the video frames directory. see below "Using an index string" for details.
		bool "show": with show=true the actually used byteposition for that frame is displayed. default value is false.
			K = position given in index is used.
			D = position by adding current delta is used.
 			B = position by adding currend big_delta is used
		int "sarnum": specify sar numerator to set frame property. default value is 0.
		int "sarden": specify sar denominator to set frame property. default value is 0.
		int "frames": specify maximum number of frames. useful for pipe input. default value is 2147483647 (INT_MAX).

	2. a file name containing the parameters like "xxx_w=1440_h=1080_pt=YUV420P10_fn=30000_fd=1001_sn=4_sd=3_fr=100.raw".
		_w=WWW_h=HHH: specify width and height. WWW and HHH must be integers.
		_pt=XXX: specify pixel_type. XXX must be a string.
		_fn=NNN_fd=DDD: specify fpsnum and fpsden. NNN and DDD must be integers.
		_sn=NNN_sd=DDD: specify sarnum and sarden. NNN and DDD must be integers.
		_fr=FFF: specify frames. FFF must be an integer.

	3. a YUV4MPEG2-header is found, width / height / framerate / pixeltype / fieldorder is set according to the header data.
	although, if sar / colorrange / colorprimaries / transfer / colormatrix are specified, they will be set as frame properties. Only fixed-length FRAME headers without 'm'tag are supported.

	4. an "index" string (or file) is given together with width, height, pixel_type. The index string can contain all or partial positions of the frames.

some examples.
```
#use arguments
RawSourcePlus("D:/path/to/the/file.ext", width=1280, height=720, pixel_type="yv12", fpsnum=50, fpsden=1)
```
```
#use file name
RawSourcePlus("D:/path/to/the/file_w=1280_h=720_pt=yv12_fn=50_fd=1.ext")
```
```
#use yuv4mpeg2 header
RawSourcePlus("D:/path/to/the/file.y4m")
```
```
#use index string
RawSourcePlus("D:/path/to/the/file.ext", width=1280, height=720, pixel_type="yv12", fpsnum=50, fpsden=1, index="0:180 1:15180")
```

It has capability to read from a pipe by using "-". for example:
```
#contents of example.avs:
RawSourcePlus("-", width=960, height=540, pixel_type="bgr24", fpsnum=30, frames=3000)
prefetch(1)
ConvertToYV12()
ShowFrameNumber()
BicubicResize(1920, 1080)
prefetch(4)

comannd line:
ffmpeg -hide_banner -video_size 960x540 -framerate 30 -f gdigrab -i desktop -pix_fmt bgr24 -f rawvideo - | ffplay example.avs -hide_banner
```

If pipe input has YUV4MPEG2 header, no other arguments without frames are needed.
```
#contents of example2.avs:
RawSourcePlus("-", frames=10000)
prefetch(1)
flipVertical()
Spline36Resize(1280, 720)
prefetch(4)

command line:
ffmpeg -hide_banner -rtbufsize 30M -f dshow -i video="USB VIDEO DEVICE" -vf fps=30 -pix_fmt YUV420p -f yuv4mpegpipe - | ffplay example2.avs -hide_banner
```
### Supported pixel types

BE meaning BIG ENDIAN.

These are supported packed formats.
|pixel_type string|output colorspace|description|
|:----------------|:----------------|:----------|
|BGR, BGR24, RGB, RGB24|RGB24|interleaved 8-bit R, G, and B without subsampling.|
|BGR32, BGR0, BGRA, RGB32, RGB0, RGBA, ABGR, ARGB, ABGR32, ARGB32, 0BGR, 0RGB|RGB32|interleaved 8bit R, G, B, and A (or 0 padding) without subsampling|
|BGR48, RGB48, BGR48BE, RGB48BE|RGB48|interleaved 16-bit R, G and B without subsampling.|
|BGR64, BGRA64, RGB64, RGBA64, BGR64BE, BGRA64BE, RGB64BE, RGBA64BE|RGB64|interleaved 16-bit R, G, B and A (or 0 padding) without subsampling.|
|YUY2, YUYV, YVYU, UYVY, VYUY|YUY2|interleaved 8-bit Y, U and V horizontally subsampled.|
|AYUV, VUYA, VUYX, UYVA|YUVA444P8|interleaved 8-bit Y, U, V and A (or 0 padding) without subsampling.|
|AYUV64, AYUV64BE|YUVA444P16|interleaved 16-bit Y, U, V and A without subsampling.|
|Y210, Y216, Y210BE, Y216BE|YUV422P16|interleaved 16-bit Y, U and V horizontally subsampled like YUY2. Y210 and Y210BE has only 10-bit precision, but the data is left-shifted by 6-bit to make it to 16-bit.|
|UYVY10, UYVY10BE|YUV422P10|interleaved 10-bit Y, U andV horizontally subsampled.|
|UYVY12, UYVY12BE|YUV422P12|interleaved 12-bit Y, U andV horizontally subsampled.|
|UYVY14, UYVY14BE|YUV422P14|interleaved 14-bit Y, U andV horizontally subsampled.|
|UYVY16, UYVY16BE|YUV422P16|interleaved 16-bit Y, U andV horizontally subsampled.|


These are supported planar formats.
|pixel_type string|output colorspace|description|
|:----------------|:----------------|:----------|
|GBRP|RGBP|planar 8-bit R, G and B without subsampling.|
|GBRP9, GBRP9BE|RGBP10|planar 9-bit R, G and B without subsampling. the data are shifted left by one bit on output.|
|GBRP10, GBRP10BE|RGBP10|planar 10-bit R, G and B without subsampling.|
|GBRP12, GBRP12BE|RGBP12|planar 12-bit R, G and B without subsampling.|
|GBRP14, GBRP14BE|RGBP14|planar 14-bit R, G and B without subsampling.|
|GBRP16, GBRP16BE|RGBP16|planar 16-bit R, G and B without subsampling.|
|GBRAP|RGBAP|planar 8-bit R, G, B and A without subsampling.|
|GBRAP10, GBRAP10BE|RGBAP10|planar 10-bit R, G, B and A without subsampling.|
|GBRAP12, GBRAP12BE|RGBAP12|planar 12-bit R, G, B and A without subsampling.|
|GBRAP14, GBRAP14BE|RGBAP14|planar 14-bit R, G, B and A without subsampling.|
|GBRAP16, GBRAP16BE|RGBAP16|planar 16-bit R, G, B and A without subsampling.|
|GBRAPS|RGBAPS|planar single-precision floating point R, G, B and A without subsampling.|
|YV24, YUV444P8|YV24|planar 8-bit Y, U and V without subsampling.|
|YUV444P9, YUV444P9BE|YUV444P10|planar 9-bit Y, U and V without subsampling. the data are shifted left by one bit on output.|
|YUV444P10, YUV444P10BE|YUV444P10|planar 10-bit Y, U and V without subsampling.|
|YUV444P12, YUV444P12BE|YUV444P12|planar 12-bit Y, U and V without subsampling.|
|YUV444P14, YUV444P14BE|YUV444P14|planar 14-bit Y, U and V without subsampling.|
|YUV444P16, YUV444P16BE|YUV444P16|planar 16-bit Y, U and V without subsampling.|
|YUVA444|YUVA444|planar 8-bit Y, U, V and A without subsampling.|
|YUVA444P9, YUVA444P9BE|YUVA444P10|planar 9-bit Y, U, V and A without subsampling. the data are shifted left by one bit on output.|
|YUVA444P10, YUVA444P10BE|YUVA444P10|planar 10-bit Y, U, V and A without subsampling.|
|YUVA444P12, YUVA444P12BE|YUVA444P12|planar 12-bit Y, U, V and A without subsampling.|
|YUVA444P14, YUVA444P14BE|YUVA444P14|planar 14-bit Y, U, V and A without subsampling.|
|YUVA444P16, YUVA444P16BE|YUVA444P16|planar 16-bit Y, U, V and A without subsampling.|
|YV16, YUV422P8|YV16|planar 8-bit Y, U and V horizontally subsampled.|
|YUV422P9, YUV422P9BE|YUV422P10|planar 9-bit Y, U and V horizontally subsampled. the data are shifted left by one bit on output.|
|YUV422P10, YUV422P10BE|YUV422P10|planar 10-bit Y, U and V horizontally subsampled.|
|YUV422P12, YUV422P12BE|YUV422P12|planar 12-bit Y, U and V horizontally subsampled.|
|YUV422P14, YUV422P14BE|YUV422P14|planar 14-bit Y, U and V horizontally subsampled.|
|YUV422P16, YUV422P16BE|YUV422P16|planar 16-bit Y, U and V horizontally subsampled.|
|YUVA422|YUVA422|planar 8-bit Y, U, V and A horizontally subsampled.|
|YUVA422P9, YUVA422P9BE|YUVA422P10|planar 9-bit Y, U, V and A horizontally subsampled. the data are shifted left by one bit on output.|
|YUVA422P10, YUVA422P10BE|YUVA422P10|planar 10-bit Y, U, V and A horizontally subsampled.|
|YUVA422P12, YUVA422P12BE|YUVA422P12|planar 12-bit Y, U, V and A horizontally subsampled.|
|YUVA422P14, YUVA422P14BE|YUVA422P14|planar 14-bit Y, U, V and A horizontally subsampled.|
|YUVA422P16, YUVA422P16BE|YUVA422P16|planar 16-bit Y, U, V and A horizontally subsampled.|
|YV12, I420, IYUV, YUV420P|YV12|planar 8-bit Y, U and V horizontally and vertically subsampled.|
|YUV420P9, YUV420P9BE|YUV420P10|planar 9-bit Y, U and V horizontally and vertically subsampled. the data are shifted left by one bit on output.|
|YUV420P10, YUV420P10BE|YUV420P10|planar 10-bit Y, U and V horizontally and vertically subsampled.|
|YUV420P12, YUV420P12BE|YUV420P12|planar 12-bit Y, U and V horizontally and vertically subsampled.|
|YUV420P14, YUV420P14BE|YUV420P14|planar 14-bit Y, U and V horizontally and vertically subsampled.|
|YUV420P16, YUV420P16BE|YUV420P16|planar 16-bit Y, U and V horizontally and vertically subsampled.|
|YUVA420|YUVA420|planar 8-bit Y, U, V and A horizontally and vertically subsampled.|
|YUVA420P9, YUVA420P9BE|YUVA420P10|planar 9-bit Y, U, V and A horizontally and vertically subsampled.the data are shifted left by one bit on output.|
|YUVA420P10, YUVA420P10BE|YUVA420P10|planar 10-bit Y, U, V and A horizontally and vertically subsampled.|
|YUVA420P12, YUVA420P12BE|YUVA420P12|planar 12-bit Y, U, V and A horizontally and vertically subsampled.|
|YUVA420P14, YUVA420P14BE|YUVA420P14|planar 14-bit Y, U, V and A horizontally and vertically subsampled.|
|YUVA420P16, YUVA420P16BE|YUVA420P16|planar 16-bit Y, U, V and A horizontally and vertically subsampled.|
|YV411, Y41B|YV411|planar 8-bit Y, U and V horizontally subsampled.|

These are supported chroma packed formats.
|pixel_type string|output colorspace|description|
|:----------------|:----------------|:----------|
|NV24, NV42|YV24|chroma interleaved 8bit without subsampling|
|NV16|YV16|chroma interleaved 8bit horizontally subsampled|
|NV12, NV21|YV12|chroma interleaved 8bit horizontally and vertically subsampled|
|NV20, NV20BE|YUV422P10|chroma interleaved 10bit horizontally subsampled|
|P410, P410BE|YUV444P16|chroma interleaved 16bit without subpampling. P410 and P410BE has only 10-bit precision, but the data is left-shifted by 6-bit to make it to 16-bit.|
|P412, P412BE|YUV444P16|chroma interleaved 16bit without subpampling. P412 and P412BE has only 12-bit precision, but the data is left-shifted by 4-bit to make it to 16-bit.|
|P416, P416BE|YUV444P16|chroma interleaved 16bit without subpampling.|
|P210, P210BE|YUV422P16|chroma interleaved 16bit horizontally subsampled. P210 and P210BE has only 10-bit precision, but the data is left-shifted by 6-bit to make it to 16-bit.|
|P212, P212BE|YUV422P16|chroma interleaved 16bit horizontally subsampled. P212 and P212BE has only 12-bit precision, but the data is left-shifted by 4-bit to make it to 16-bit.|
|P216, P216BE|YUV422P16|chroma interleaved 16bit horizontally subsampled.|
|P010, P010BE|YUV420P16|chroma interleaved 16bit horizontally and vertically subsampled. P010 and P010BE has only 10-bit precision, but the data is left-shifted by 6-bit to make it to 16-bit.|
|P012, P012BE|YUV420P16|chroma interleaved 16bit horizontally and vertically subsampled. P012 and P012BE has only 12-bit precision, but the data is left-shifted by 4-bit to make it to 16-bit.|
|P016, P016BE|YUV420P16|chroma interleaved 16bit horizontally and vertically subsampled.|

These are supported Grayscale formats.
|pixel_type string|output colorspace|description|
|:----------------|:----------------|:----------|
|Y8, GRAY8, GREY8|Y8|8-bit luma only.|
|Y9, GRAY9, GREY9, GRAY9BE, GREY9BE|Y10|9-bit luma only. the data are shifted left by one bit on output.|
|Y10, GRAY10, GREY10, GRAY10BE, GREY10BE|Y10|10-bit luma only.|
|Y12, GRAY12, GREY12, GRAY12BE, GREY12BE|Y12|12-bit luma only.|
|Y14, GRAY14, GREY14, GRAY14BE, GREY14BE|Y14|14-bit luma only.|
|Y16, GRAY16, GREY16, GRAY16BE, GREY16BE|Y16|16-bit luma only.|


### Using an index-string:

You can enter the byte positions of the video frames directly.
```
RawSourcePlus("d:\yuv.mov",720,576,"UYVY", index="0:192512 1:1021952 25:21120512 50:42048512 75:62976512")
```
This is useful if it's not really raw video, but e.g. uncompressed MOV files or a file with some kind of header.<br>
It will work whenever the spacing of the frames is fixed or has at least two fixed intervalls (e.g. audio data interleaved with the video every 25th frame).<br>
You enter pairs of framenumber:byteposition.<br>
Internally there are two step values (for the byte positions): delta and big_delta.<br>
delta is stored everytime when two adjacent framenumbers are given, the default value is width*height*bytes_per_pixel.<br>
big_delta is stored, when three framenumbers with the same two intervals are given. The default value is 0 (meaning there is no useful big_delta present)..<br>
If those conditions are not met, the internal values of delta and big_delta is not updated, only the given bytepositions in the index are used.
big_delta is reset to 0 if the resulting position would be behind the given one (see beyond).<br>

Here are some possible cases:
|index string|description|
|-----------:|:----------|
|0:    0|frame 0 starts at byte 0, step to frame 1 is default = width*height*bytes_per_pixel|
|0:10000|frame 0 starts at 10000|
|0: 5000<br>1:15000|frame 0 at 5000<br>frame 1 at 15000 (delta is set to 10000)<br>frame 2 at 25000 (using delta)<br>frame 3 at 35000 (using delta)|
|  0:  5000<br>1: 15000<br>25:290000<br>50:590000<br>75:890000|frame 0 at 5000<br>frame 1 at 15000 (delta=10000)<br>frame 2 at 25000<br>...<br>frame 25 at 290000 (using entry instead of delta which would be at 255000)<br>frame 26 at 300000 (still using delta)<br>...<br>frame 50 at 590000 (using entry instead of delta)<br>>> because 25...50 = 50...75 now big_delta is set to 300000 (590000-290000)<br>frame 51 at 600000 (still using delta)<br>...<br>frame 75 at 890000 (using entry which is the same as using big_delta)<br>...<br>frame 100 at 1190000 (using big_delta)<br>frame 101 at 1200000 (using delta)<br>...<br>frame 125 at 1490000 (using big_delta)|
|0:  5000<br>1: 15000<br>25:290000<br>50:590000<br>75:890000<br>100:95000|the same as in the previous example<br>frame 75 at 890000<br>>> because 890000+300000 > 950000 now big_delta is reset to 0.<br>frame 100 at 950000<br>frame 101 at 960000 (using delta)<br>...<br>frame 125 at 1200000 (there is NO big_delta)|

The index string is treated as a filename, if there is an "." inside. The data is then read from that file, line breaks don't matter.

original author:Ernst Peché, 2005-10-13

modified by Oka Motofumi, 2011-06-14

Version 2016-07-07 - Modified RawSource26 to Avisynth+ plugin.

Version 2016-08-14 - Update for Avisynth+ r2150 or later.

Version 2025-12-13 - update for Avisynth+ 3.7.5 or later.
