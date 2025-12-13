#!/bin/env python

import os
import sys
import subprocess

def format_dict():
    return {
        "BGR": "bgr24",
        "BGR24": "bgr24",
        "RGB": "rgb24",
        "RGB24": "rgb24",

        "BGR48": "bgr48le",
        "RGB48": "rgb48le",
        "BGR48BE": "bgr48be",
        "RGB48BE": "rgb48be",

        "BGRA": "bgra",
        "BGR32": "bgr0",
        "RGBA": "rgba",
        "RGB32": "rgb0",
        "0BGR": "0bgr",
        "0RGB": "0rgb",
        "ABGR": "abgr",
        "ABGR32": "abgr",
        "ARGB": "argb",
        "ARGB32": "argb",

        "BGR64": "bgra64le",
        "BGRA64": "bgra64le",
        "RGB64": "rgba64le",
        "RGBA64": "rgba64le",
        "BGR64BE": "bgra64be",
        "BGRA64BE": "bgra64be",
        "RGB64BE": "rgba64be",
        "RGBA64BE": "rgba64be",

        "YUY2": "yuyv422",
        "YUYV": "yuyv422",
        "UYVY": "uyvy422",
        "YVYU": "yvyu422",

        "Y210": "y210le",
        "Y216": "y216le",

        "GBRP": "gbrp",
        "GBRP9": "gbrp9le",
        "GBRP10": "gbrp10le",
        "GBRP12": "gbrp12le",
        "GBRP14": "gbrp14le",
        "GBRP16": "gbrp16le",
        "GBRPS" : "gbrpf32le",
        "GBRP9BE": "gbrp9be",
        "GBRP10BE": "gbrp10be",
        "GBRP12BE": "gbrp12be",
        "GBRP14BE": "gbrp14be",
        "GBRP16BE": "gbrp16be",

        "GBRAP": "gbrap",
        "GBRAP10": "gbrap10le",
        "GBRAP12": "gbrap12le",
        "GBRAP14": "gbrap14le",
        "GBRAP16": "gbrap16le",
        "GBRAPS": "gbrapf32le",
        "GBRAP10BE": "gbrap10be",
        "GBRAP12BE": "gbrap12be",
        "GBRAP14BE": "gbrap14be",
        "GBRAP16BE": "gbrap16be",

        "YV24": "yuv444p",
        "YUV444P8" : "yuv444p",
        "YUV444P9": "yuv444p9le",
        "YUV444P10": "yuv444p10le",
        "YUV444P12": "yuv444p12le",
        "YUV444P14": "yuv444p14le",
        "YUV444P16": "yuv444p16le",
        "YUV444P9BE": "yuv444p9be",
        "YUV444P10BE": "yuv444p10be",
        "YUV444P12BE": "yuv444p12be",
        "YUV444P14BE": "yuv444p14be",
        "YUV444P16BE": "yuv444p16be",

        "YUVA444" : "yuva444p",
        "YUVA444P9": "yuva444p9le",
        "YUVA444P10": "yuva444p10le",
        "YUVA444P12": "yuva444p12le",
        "YUVA444P16": "yuva444p16le",
        "YUVA444P9BE": "yuva444p9be",
        "YUVA444P10BE": "yuva444p10be",
        "YUVA444P12BE": "yuva444p12be",
        "YUVA444P16BE": "yuva444p16be",

        "YV16": "yuv422p",
        "YUV422P8" : "yuv422p",
        "YUV422P9": "yuv422p9le",
        "YUV422P10": "yuv422p10le",
        "YUV422P12": "yuv422p12le",
        "YUV422P14": "yuv422p14le",
        "YUV422P16": "yuv422p16le",
        "YUV422P9BE": "yuv422p9be",
        "YUV422P10BE": "yuv422p10be",
        "YUV422P12BE": "yuv422p12be",
        "YUV422P14BE": "yuv422p14be",
        "YUV422P16BE": "yuv422p16be",

        "YUVA422P8" : "yuva422p",
        "YUVA422P9": "yuva422p9le",
        "YUVA422P10": "yuva422p10le",
        "YUVA422P12": "yuva422p12le",
        "YUVA422P16": "yuva422p16le",
        "YUVA422P9BE": "yuva422p9be",
        "YUVA422P10BE": "yuva422p10be",
        "YUVA422P12BE": "yuva422p12be",
        "YUVA422P16BE": "yuva422p16be",

        "YV12": "yuv420p",
        "I420": "yuv420p",
        "IYUV": "yuv420p",
        "YUV420P9": "yuv420p9le",
        "YUV420P10": "yuv420p10le",
        "YUV420P12": "yuv420p12le",
        "YUV420P14": "yuv420p14le",
        "YUV420P16": "yuv420p16le",
        "YUV420P9BE": "yuv420p9be",
        "YUV420P10BE": "yuv420p10be",
        "YUV420P12BE": "yuv420p12be",
        "YUV420P14BE": "yuv420p14be",
        "YUV420P16BE": "yuv420p16be",

        "YUVA420" : "yuva420p",
        "YUVA420P9": "yuva420p9le",
        "YUVA420P10": "yuva420p10le",
        "YUVA420P16": "yuva420p16le",
        "YUVA420P9BE": "yuva420p9be",
        "YUVA420P10BE": "yuva420p10be",
        "YUVA420P16BE": "yuva420p16be",

        "Y41B": "yuv411p",
        "YV411": "yuv411p",

        "NV12": "nv12",
        "NV21": "nv21",
        "NV16": "nv16",
        "NV24": "nv24",
        "NV42": "nv42",
        "NV20": "nv20le",
        "NV20BE": "nv20be",

        "P010": "p010le",
        "P012": "p012le",
        "P016": "p016le",
        "P210": "p210le",
        "P212": "p212le",
        "P216": "p216le",
        "P410": "p410le",
        "P412": "p412le",
        "P416": "p416le",

        "P010BE": "p010be",
        "P012BE": "p012be",
        "P016BE": "p016be",
        "P210BE": "p210be",
        "P212BE": "p212be",
        "P216BE": "p216be",
        "P410BE": "p410be",
        "P412BE": "p412be",
        "P416BE": "p416be",

        "Y8": "gray",
        "Y10": "gray10le",
        "Y12": "gray12le",
        "Y14": "gray14le",
        "Y16": "gray16le",
        "Y32": "grayf32le",
        "GREY8": "gray",
        "GREY9": "gray9le",
        "GREY10": "gray10le",
        "GREY12": "gray12le",
        "GREY14": "gray14le",
        "GREY16": "gray16le",
        "GREY9BE": "gray9be",
        "GREY10BE": "gray10be",
        "GREY12BE": "gray12be",
        "GREY14BE": "gray14be",
        "GREY16BE": "gray16be",
    }


def get_dict(args):
    if args[0] == "all":
        answer = input("Generating all samples will bring the total size to "\
            + "nearly 5GB. Do you want to continue? (y/n): ").lower()
        if answer != 'y' and answer != 'yes':
            print("Processing ends.", sys.stderr)
            exit(0)
        return format_dict()
    fmts = format_dict()
    ret = {}
    for k in args:
        k = k.upper()
        v = fmts.get(k)
        if v is not None:
            ret[k] = v;
    return ret

def get_swap(ptype):
    if ptype in ["YV12", "YV411", "YV16", "YV24"]:
        return ".SwapUV()"
    return ""


if __name__ == "__main__":

    if len(sys.argv) == 1:
        exit(1)

    release = "../vs2022/x64/Release/RawSourcePlus.dll"
    debug = "../vs2022/x64/Debug/RawSourcePlus.dll"
    src = "sintel_1280x546_rgb48_10frames_ffv1.nut"
    ffmt = "rawvideo"
    ext = "raw"
    w = 1280
    h = 546
    fn = 1
    fd = 1

    fmtdict = get_dict(sys.argv[1:])
    if len(fmtdict) == 0:
        print("No target. Processing ends.", file=sys.stderr)
        exit(1)

    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    os.makedirs("rawfiles", exist_ok=True)

    with open("testsamples.avs", mode="w") as avs:
        lines = []
        lines.append(f'#LoadPlugin("{release}")\n')
        lines.append(f'LoadPlugin("{debug}")\n\n\n')
        

        for ptype, pfmt in fmtdict.items():
            out = f"rawfiles/sample_pt={ptype}_w={w}_h={h}_fn={fn}_fd={fd}.{ext}"
            cmdline = f"ffmpeg -hide_banner -y -i {src} -pix_fmt {pfmt} " \
                + f"-f {ffmt} {out}"

            print("\n-----------------------------------------------------")
            print(cmdline)
            print("-----------------------------------------------------")

            subprocess.run(cmdline, shell=True)
            line = f'#RawSourcePlus("./{out}"){get_swap(ptype)}\n'
            lines.append(line)

        avs.write(''.join(lines))
