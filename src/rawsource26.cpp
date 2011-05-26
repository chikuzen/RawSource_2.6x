/*
 RawSource26 - reads raw video data files

    Author: Oka Motofumi (chikuzen.mo at gmail dot com)

    This program is rewriting of RawSource.dll(original author is Pech Ernst)
    for avisynth2.6.
*/

#include "rawsource26.h"

RawSource::RawSource (const char *sourcefile, const int a_width, const int a_height,
                      const char *a_pix_type, const int a_fpsnum, const int a_fpsden,
                      const char *a_index, const bool a_show, IScriptEnvironment *env)
{
    if ((h_rawfile = _open(sourcefile, _O_BINARY | _O_RDONLY)) == -1)
        env->ThrowError("Cannot open videofile.");

    if ((filelen = _filelengthi64(h_rawfile)) == -1L)
        env->ThrowError("Cannot get videofile length.");

    memset(&vi, 0, sizeof(vi));
    vi.width = a_width;
    vi.height = a_height;
    vi.fps_numerator = a_fpsnum;
    vi.fps_denominator = a_fpsden;
    vi.SetFieldBased(false);

    strcpy(pix_type, a_pix_type);

    level = 0;
    show = a_show;

    y4m_headerlen = 0;
    headeroffset = 0;

    if (!strcmp(a_index, "")) {    //use header if valid else width, height, pixel_type from AVS are used
        ret = _read(h_rawfile, y4m_headerbuf, MAX_Y4M_HEADER);    //read some bytes and test on header
        ret = RawSource::ParseHeader();
        if (vi.width > MAX_WIDTH)
            env->ThrowError("Width too big(%d). Maximum acceptable width is %d.", vi.width, MAX_WIDTH);
        switch (ret) {
            case 4444:
                strncpy(pix_type, "AYUV", 4);
                break;
            case 444:
                strncpy(pix_type, "I444", 4);
                break;
            case 422:
                strncpy(pix_type, "I422", 4);
                break;
            case 411:
                strncpy(pix_type, "I411", 4);
                break;
            case 420:
                strncpy(pix_type, "I420", 4);
                break;
            case 8:
                strncpy(pix_type, "GRAY", 4);
                break;
            case -1:
                env->ThrowError("YUV4MPEG2 header error.");
                break;
            case -2:
                env->ThrowError("This file's YUV4MPEG2 HEADER is unsupported.");
            default:
                break;
        }
    }

/*    const pix_fmt pixelformats[] = {
        {"BGR",   VideoInfo::CS_BGR24, {0, 1, 2, 9}, 3},
        {"BGR24", VideoInfo::CS_BGR24, {0, 1, 2, 9}, 3},
        {"RGB",   VideoInfo::CS_BGR24, {2, 1, 0, 9}, 3},
        {"RGB24", VideoInfo::CS_BGR24, {2, 1, 0, 9}, 3},
        {"BGRA",  VideoInfo::CS_BGR32, {0, 1, 2, 3}, 4},
        {"BGR32", VideoInfo::CS_BGR32, {0, 1, 2, 3}, 4},
        {"RGBA",  VideoInfo::CS_BGR32, {2, 1, 0, 3}, 4},
        {"RGB32", VideoInfo::CS_BGR32, {2, 1, 0, 3}, 4},
        {"ARGB",  VideoInfo::CS_BGR32, {3, 2, 1, 0}, 4},
        {"ABGR",  VideoInfo::CS_BGR32, {3, 0, 1, 2}, 4},
        {"YUY2",  VideoInfo::CS_YUY2,  {0, 1, 2, 3}, 4},
        {"YUYV",  VideoInfo::CS_YUY2,  {0, 1, 2, 3}, 4},
        {"UYVY",  VideoInfo::CS_YUY2,  {1, 0, 3, 2}, 4},
        {"VYUY",  VideoInfo::CS_YUY2,  {3, 0, 1, 2}, 4},
        {"YV24",  VideoInfo::CS_YV24,  {PLANAR_Y, PLANAR_V, PLANAR_U, 0}, 3},
        {"I444",  VideoInfo::CS_YV24,  {PLANAR_Y, PLANAR_U, PLANAR_V, 0}, 3},
        {"YV16",  VideoInfo::CS_YV16,  {PLANAR_Y, PLANAR_V, PLANAR_U, 0}, 3},
        {"I422",  VideoInfo::CS_YV16,  {PLANAR_Y, PLANAR_U, PLANAR_V, 0}, 3},
        {"YV411", VideoInfo::CS_YV411, {PLANAR_Y, PLANAR_V, PLANAR_U, 0}, 3},
        {"Y41B",  VideoInfo::CS_YV411, {PLANAR_Y, PLANAR_V, PLANAR_U, 0}, 3},
        {"I411",  VideoInfo::CS_YV411, {PLANAR_Y, PLANAR_U, PLANAR_V, 0}, 3},
        {"I420",  VideoInfo::CS_I420,  {PLANAR_Y, PLANAR_U, PLANAR_V, 0}, 3},
        {"IYUV",  VideoInfo::CS_I420,  {PLANAR_Y, PLANAR_U, PLANAR_V, 0}, 3},
        {"YV12",  VideoInfo::CS_YV12,  {PLANAR_Y, PLANAR_V, PLANAR_U, 0}, 3},
        {"NV12",  VideoInfo::CS_I420,  {PLANAR_Y, PLANAR_U, PLANAR_V, 0}, 2},
        {"NV21",  VideoInfo::CS_YV12,  {PLANAR_Y, PLANAR_V, PLANAR_U, 0}, 2},
        {"Y8",    VideoInfo::CS_Y8,    {PLANAR_Y, 0,        0,        0}, 1},
        {"GRAY",  VideoInfo::CS_Y8,    {PLANAR_Y, 0,        0,        0}, 1}
    };
    for (int i = 0; i < sizeof(pixelformats) / sizeof(pix_fmt); i++) {
        if(!stricmp(pix_type, pixelformats[i])) {
*/
    if (!stricmp(pix_type, "RGBA") || !stricmp(pix_type, "RGB32")) {
        vi.pixel_type = VideoInfo::CS_BGR32;
        mapping[0] = 2;
        mapping[1] = 1;
        mapping[2] = 0;
        mapping[3] = 3;
        mapcnt = 4;
    } else if (!stricmp(pix_type, "BGRA") || !stricmp(pix_type, "BGR32")) {
        vi.pixel_type = VideoInfo::CS_BGR32;
        mapping[0] = 0;
        mapping[1] = 1;
        mapping[2] = 2;
        mapping[3] = 3;
        mapcnt = 4;
    } else if (!stricmp(pix_type,"ARGB")) {
        vi.pixel_type = VideoInfo::CS_BGR32;
        mapping[0] = 3;
        mapping[1] = 2;
        mapping[2] = 1;
        mapping[3] = 0;
        mapcnt = 4;
    } else if (!stricmp(pix_type, "ABGR")) {
        vi.pixel_type = VideoInfo::CS_BGR32;
        mapping[0] = 1;
        mapping[1] = 2;
        mapping[2] = 3;
        mapping[3] = 0;
        mapcnt = 4;
    } else if (!stricmp(pix_type, "RGB")) {
        vi.pixel_type = VideoInfo::CS_BGR24;
        mapping[0] = 2;
        mapping[1] = 1;
        mapping[2] = 0;
        mapcnt = 3;
    } else if (!stricmp(pix_type, "BGR")) {
        vi.pixel_type = VideoInfo::CS_BGR24;
        mapping[0] = 0;
        mapping[1] = 1;
        mapping[2] = 2;
        mapcnt = 3;
    } else if (!stricmp(pix_type, "YUYV") || !stricmp(pix_type, "YUY2")) {
        vi.pixel_type = VideoInfo::CS_YUY2;
        mapping[0] = 0;
        mapping[1] = 1;
        mapcnt = 2;
    } else if (!stricmp(pix_type, "UYVY")) {
        vi.pixel_type = VideoInfo::CS_YUY2;
        mapping[0] = 1;
        mapping[1] = 0;
        mapcnt = 2;
    } else if (!stricmp(pix_type, "YVYU")) {
        vi.pixel_type = VideoInfo::CS_YUY2;
        mapping[0] = 0;
        mapping[1] = 3;
        mapping[2] = 2;
        mapping[3] = 1;
        mapcnt = 4;
    } else if (!stricmp(pix_type, "VYUY")) {
        vi.pixel_type = VideoInfo::CS_YUY2;
        mapping[0] = 1;
        mapping[1] = 2;
        mapping[2] = 3;
        mapping[3] = 0;
        mapcnt = 4;
    } else if (!stricmp(pix_type, "YV24")) {
        vi.pixel_type = VideoInfo::CS_YV24;
        mapping[0] = PLANAR_Y;
        mapping[1] = PLANAR_V;
        mapping[2] = PLANAR_U;
        mapcnt = 3;
    } else if (!stricmp(pix_type, "I444")) {
        vi.pixel_type = VideoInfo::CS_YV24;
        mapping[0] = PLANAR_Y;
        mapping[1] = PLANAR_U;
        mapping[2] = PLANAR_V;
        mapcnt = 3;
    } else if (!stricmp(pix_type, "I444")) {
        vi.pixel_type = VideoInfo::CS_YV24;
        mapping[0] = PLANAR_Y;
        mapping[1] = PLANAR_U;
        mapping[2] = PLANAR_V;
        mapcnt = 3;
    } else if (!stricmp(pix_type, "YV16")) {
        vi.pixel_type = VideoInfo::CS_YV16;
        mapping[0] = PLANAR_Y;
        mapping[1] = PLANAR_V;
        mapping[2] = PLANAR_U;
        mapcnt = 3;
    } else if (!stricmp(pix_type, "I422")) {
        vi.pixel_type = VideoInfo::CS_YV16;
        mapping[0] = PLANAR_Y;
        mapping[1] = PLANAR_U;
        mapping[2] = PLANAR_V;
        mapcnt = 3;
    } else if (!stricmp(pix_type, "YV411") || !stricmp(pix_type, "Y41B")) {
        vi.pixel_type = VideoInfo::CS_YV411;
        mapping[0] = PLANAR_Y;
        mapping[1] = PLANAR_V;
        mapping[2] = PLANAR_U;
        mapcnt = 3;
    } else if (!stricmp(pix_type, "I411")) {
        vi.pixel_type = VideoInfo::CS_YV411;
        mapping[0] = PLANAR_Y;
        mapping[1] = PLANAR_U;
        mapping[2] = PLANAR_V;
        mapcnt = 3;
    } else if (!stricmp(pix_type, "I420") ||
               !stricmp(pix_type, "IYUV")) {
        vi.pixel_type = VideoInfo::CS_I420;
        mapping[0] = PLANAR_Y;
        mapping[1] = PLANAR_U;
        mapping[2] = PLANAR_V;
        mapcnt = 3;
    } else if (!stricmp(pix_type, "YV12")) {
        vi.pixel_type = VideoInfo::CS_YV12;
        mapping[0] = PLANAR_Y;
        mapping[1] = PLANAR_V;
        mapping[2] = PLANAR_U;
        mapcnt = 3;
    } else if (!stricmp(pix_type, "Y8") || !stricmp(pix_type, "GRAY")) {
            vi.pixel_type = VideoInfo::CS_Y8;
            mapping[0] = PLANAR_Y;
            mapcnt = 1;
    } else if (!stricmp(pix_type, "NV12")) {
        vi.pixel_type = VideoInfo::CS_I420;
        mapping[0] = PLANAR_Y;
        mapping[1] = PLANAR_U;
        mapping[2] = PLANAR_V;
        mapcnt = 2;
    } else if (!stricmp(pix_type, "NV21")) {
        vi.pixel_type = VideoInfo::CS_YV12;
        mapping[0] = PLANAR_Y;
        mapping[1] = PLANAR_V;
        mapping[2] = PLANAR_U;
        mapcnt = 2;
    } else
        env->ThrowError("Invalid pixel type. Supported: RGB, RGBA, BGR, BGRA, ARGB,"
                        " ABGR, YUY2, YUYV, UYVY, YVYU, VYUY, YV16, I422, YV411,"
                        " I411, YV12, I420, IYUV, NV12, NV21, Y8, GRAY");

    int framesize = (vi.width * vi.height * vi.BitsPerPixel()) >> 3;

    int maxframe = (int)(filelen / (__int64)framesize);    //1 = one frame

    if (maxframe < 1)
        env->ThrowError("File too small for even one frame.");

    index = new i_struct[maxframe + 1];
    rawindex = new ri_struct[maxframe + 1];
    rawbuf = new unsigned char[vi.IsPlanar() ? vi.width : vi.width * (vi.BitsPerPixel() >> 3)];

//index build using string descriptor
    char * indstr;
    FILE * indexfile;
    char seps[] = " \n";
    char * token;
    int frame;
    __int64 bytepos;
    int p_ri = 0;
    int rimax;
    char * p_del;
    int num1, num2;
    int delta;    //delta between 1 frame
    int big_delta;    //delta between many e.g. 25 frames
    int big_frame_step;    //how many frames is big_delta for?
    int big_steps;    //how many big deltas have occured

//read all framenr:bytepos pairs
    if (!strcmp(a_index, "")) {
        rawindex[0].framenr = 0;
        rawindex[0].bytepos = headeroffset;
        rawindex[1].framenr = 1;
        rawindex[1].bytepos = headeroffset + y4m_headerlen + framesize;
        rimax = 1;
    } else {
        const char * pos = strchr(a_index, '.');
        if (pos != NULL) {    //assume indexstring is a filename
            indexfile = fopen(a_index, "r");
            if (indexfile == 0)
                env->ThrowError("Cannot open indexfile.");
            fseek(indexfile, 0, SEEK_END);
            ret = ftell(indexfile);
            indstr = new char[ret + 1];
            fseek(indexfile, 0, SEEK_SET);
            fread(indstr, 1, ret, indexfile);
            fclose(indexfile);
        } else {
            indstr = new char[strlen(a_index) + 1];
            strcpy(indstr, a_index);
        }
        token = strtok(indstr, seps);
        while (token) {
            num1 = -1;
            num2 = -1;
            p_del = strchr(token, ':');
            if (!p_del)
                break;
            ret = sscanf(token, "%d", &num1);
            ret = sscanf(p_del + 1, "%d", &num2);

            if ((num1 < 0) || (num2 < 0))
                break;
            rawindex[p_ri].framenr = num1;
            rawindex[p_ri].bytepos = num2;
            p_ri++;
            token = strtok(0, seps);
        }
        rimax = p_ri - 1;
        delete indstr;
    }

    if ((rimax < 0) || rawindex[0].framenr)
        env->ThrowError("When using an index: frame 0 is mandatory");    //at least an entries for frame0

//fill up missing bytepos (create full index)
    frame = 0;    //framenumber
    p_ri = 0;    //pointer to raw index
    big_frame_step = 0;
    big_steps = 0;
    big_delta = 0;

    delta = framesize;
    //rawindex[1].bytepos - rawindex[0].bytepos;    //current bytepos delta
    bytepos = rawindex[0].bytepos;
    index[frame].type = 'K';
    while ((frame < maxframe) && ((bytepos + framesize) <= filelen)) {    //next frame must be readable
        index[frame].index = bytepos;

        if ((p_ri < rimax) && (rawindex[p_ri].framenr <= frame)) {
            p_ri++;
            big_steps = 1;
        }
        frame++;

        if ((p_ri > 0) && (rawindex[p_ri - 1].framenr + big_steps * big_frame_step == frame)) {
            bytepos = rawindex[p_ri - 1].bytepos + big_delta * big_steps;
            big_steps++;
            index[frame].type = 'B';
        } else {
            if (rawindex[p_ri].framenr == frame) {
                bytepos = rawindex[p_ri].bytepos;    //sync if framenumber is given in raw index
                index[frame].type = 'K';
            } else {
                bytepos = bytepos + delta;
                index[frame].type = 'D';
            }
        }

//check for new delta and big_delta
        if ((p_ri > 0) && (rawindex[p_ri].framenr == rawindex[p_ri-1].framenr + 1)) {
            delta = (int)(rawindex[p_ri].bytepos - rawindex[p_ri - 1].bytepos);
        } else if (p_ri > 1) {
//if more than 1 frame difference and
//2 successive equal distances then remember as big_delta
//if second delta < first delta then reset
            if (rawindex[p_ri].framenr - rawindex[p_ri - 1].framenr == rawindex[p_ri - 1].framenr - rawindex[p_ri - 2].framenr) {
                big_frame_step = rawindex[p_ri].framenr - rawindex[p_ri - 1].framenr;
                big_delta = (int)(rawindex[p_ri].bytepos - rawindex[p_ri - 1].bytepos);
            } else {
                if ((rawindex[p_ri].framenr - rawindex[p_ri - 1].framenr) < (rawindex[p_ri - 1].framenr - rawindex[p_ri - 2].framenr)) {
                    big_delta = 0;
                    big_frame_step = 0;
                }
                if (frame >= rawindex[p_ri].framenr) {
                    big_delta = 0;
                    big_frame_step = 0;
                }
            }
        }
        frame = frame;
    }
    vi.num_frames = frame;
}


RawSource::~RawSource() {
    _close(h_rawfile);
    delete [] rawbuf;
    delete [] index;
    delete [] rawindex;
}

PVideoFrame __stdcall RawSource::GetFrame(int n, IScriptEnvironment* env)
{
    unsigned char *pdst;
    int samples_per_line;
    int number_of_lines;
    int i, j, k;

    if (show && !level) {
    //output debug info - call Subtitle

        char message[255];
        sprintf(message, "%d : %I64d %c", n, index[n].index, index[n].type);

        const char* arg_names[11] = {0, 0, "x", "y", "font", "size", "text_color", "halo_color"};
        AVSValue args[8] = {this, AVSValue(message), 4, 12, "Arial", 15, 0xFFFFFF, 0x000000};

        level = 1;
        PClip resultClip = (env->Invoke("Subtitle", AVSValue(args, 8), arg_names )).AsClip();
        PVideoFrame src1 = resultClip->GetFrame(n, env);
        level = 0;
        return src1;
    //end debug
    }

    PVideoFrame dst = env->NewVideoFrame(vi);
    if ((ret = (int)_lseeki64(h_rawfile, index[n].index, SEEK_SET)) == -1L)
        return dst;    //error. do nothing

    if (!stricmp(pix_type, "NV12") || !stricmp(pix_type, "NV21")) {
        samples_per_line = dst->GetRowSize(PLANAR_Y);
        number_of_lines = dst->GetHeight(PLANAR_Y);
        pdst = dst->GetWritePtr(PLANAR_Y);
        for (i = 0; i < number_of_lines; i++) {
            memset(rawbuf, 0, vi.width);
            ret = _read(h_rawfile, rawbuf, samples_per_line);
            memcpy(pdst, rawbuf, samples_per_line);
            pdst += dst->GetPitch(PLANAR_Y);
        }
        number_of_lines >>= 1;
        pdst = dst->GetWritePtr(mapping[1]);
        unsigned char *pdst2 = dst->GetWritePtr(mapping[2]);
        for (i = 0; i < number_of_lines; i++) {
            memset(rawbuf, 0, vi.width);
            ret = _read(h_rawfile, rawbuf, samples_per_line);
            for (j = 0; j < (samples_per_line >> 1); j++) {
                pdst[j]  = rawbuf[j << 1];
                pdst2[j] = rawbuf[(j << 1) + 1];
            }
            pdst  += dst->GetPitch(mapping[1]);
            pdst2 += dst->GetPitch(mapping[2]);
        }
    } else if (vi.IsPlanar()) {
        for (i = 0; i < mapcnt; i++) {
            samples_per_line = dst->GetRowSize(mapping[i]);
            number_of_lines = dst->GetHeight(mapping[i]);
            pdst = dst->GetWritePtr(mapping[i]);
            for (j = 0; j < number_of_lines; j++) {
                memset(rawbuf, 0, vi.width);
                ret = _read(h_rawfile, rawbuf, samples_per_line);
                memcpy (pdst, rawbuf, samples_per_line);
                pdst += dst->GetPitch(mapping[i]);
            }
        }
    } else if (!strnicmp(pix_type, "BGR", 3) || !strnicmp(pix_type, "YUY", 3)) {
        samples_per_line = dst->GetRowSize();
        number_of_lines = dst->GetHeight();
        pdst = dst->GetWritePtr();
        for (i = 0; i < number_of_lines; i++) {
            memset(rawbuf, 0, samples_per_line);
            ret = _read(h_rawfile, rawbuf, samples_per_line);
            memcpy(pdst, rawbuf, samples_per_line);
            pdst += dst->GetPitch();
        }
    } else {
        samples_per_line = dst->GetRowSize();
        number_of_lines = dst->GetHeight();
        pdst = dst->GetWritePtr();
        for (i = 0; i < number_of_lines; i++) {
            memset(rawbuf, 0, samples_per_line);
            ret = _read(h_rawfile, rawbuf, samples_per_line);
            for (j = 0; j < samples_per_line / mapcnt; j++) {
                for (k = 0; k < mapcnt; k++) {
                    pdst[j * mapcnt + k] = rawbuf[j * mapcnt + mapping[k]];
                }
            }
            pdst += dst->GetPitch();
        }
    }
    return dst;
}

int RawSource::ParseHeader()
{
    if (strncmp(y4m_headerbuf, Y4M_STREAM_MAGIC, Y4M_STREAM_MAGIC_LEN))
        return 0;

    vi.height = 0;
    vi.width = 0;
    int colorspace = 420;

    unsigned int numerator = 0, denominator = 0;
    char ctag[9] = {0};
    int i = Y4M_STREAM_MAGIC_LEN;

    while ((i < MAX_Y4M_HEADER) && (y4m_headerbuf[i] != '\n')) {
        if (!strncmp(y4m_headerbuf + i, " W", 2)) {
            i += 2;
            sscanf(y4m_headerbuf + i, "%d", &vi.width);
        }

        if (!strncmp(y4m_headerbuf + i, " H", 2)) {
            i += 2;
            sscanf(y4m_headerbuf + i, "%d", &vi.height);
        }

        if (!strncmp(y4m_headerbuf + i, " I", 2)) {
            i += 2;
            if (y4m_headerbuf[i] == 'm')
                return -2;
            else if (y4m_headerbuf[i] == 't')
                vi.image_type = VideoInfo::IT_TFF;
            else if (y4m_headerbuf[i] == 'b')
                vi.image_type = VideoInfo::IT_BFF;
        }

        if (!strncmp(y4m_headerbuf + i, " F", 2)) {
            i += 2;
            sscanf(y4m_headerbuf + i, "%u:%u", &numerator, &denominator);
            if (numerator && denominator)
                vi.SetFPS(numerator, denominator);
            else
                return -1;
        }

        if (!strncmp(y4m_headerbuf + i, " C", 2)) {
            i += 2;
            sscanf(y4m_headerbuf + i, "%s", ctag);
            if (!strncmp(ctag, "444alpha", 8))
                colorspace = 4444;
            else if (!strncmp(ctag, "444", 3))
                colorspace = 444;
            else if (!strncmp(ctag, "422", 3))
                colorspace = 422;
            else if (!strncmp(ctag, "411", 3))
                colorspace = 411;
            else if (!strncmp(ctag, "420", 3))
                colorspace = 420;
            else if (!strncmp(ctag, "mono", 4))
                colorspace = 8;
            else
                return -1;
        }

        i++;
    }

    if (!numerator || !denominator || !vi.width || !vi.height)
        return -1;

    i++;

    if (strncmp(y4m_headerbuf + i, Y4M_FRAME_MAGIC, Y4M_FRAME_MAGIC_LEN))
        return -1;

    headeroffset = i;

    i += Y4M_FRAME_MAGIC_LEN;

    while ((i < 128) && (y4m_headerbuf[i] != '\n'))
        i++;

    y4m_headerlen = i - (int)headeroffset + 1;
    headeroffset = headeroffset + y4m_headerlen;

    return colorspace;
}

bool __stdcall RawSource::GetParity(int n)
{
    return vi.image_type == VideoInfo::IT_TFF;
}

AVSValue __cdecl Create_RawSource(AVSValue args, void* user_data, IScriptEnvironment* env)
{
    if (!args[0].Defined())
        env->ThrowError("RawSource: No source specified");

    const char *source = args[0].AsString();
    const int width = args[1].AsInt(720);
    const int height = args[2].AsInt(576);
    const char *pix_type = args[3].AsString("YUY2");
    const int fpsnum = args[4].AsInt(25);
    const int fpsden = args[5].AsInt(1);
    const char *index = args[6].AsString("");
    const bool show = args[7].AsBool(false);

    if (width < MIN_RESOLUTION || height < MIN_RESOLUTION)
        env->ThrowError("RawSource: width and height need to be %d or higher.", MIN_RESOLUTION);
    if (width > MAX_WIDTH)
        env->ThrowError("RawSource: width needs to be %d or lower.", MAX_WIDTH);
    if (strlen(pix_type) > MAX_PIXTYPE_LEN - 1)
        env->ThrowError("RawSource: pixel_type needs to be %d chars or shorter.", MAX_PIXTYPE_LEN - 1);
    if (fpsnum < 1 || fpsden < 1)
        env->ThrowError("RawSource: fpsnum and fpsden need to be 1 or higher.");

    return new RawSource(source, width, height, pix_type, fpsnum, fpsden, index, show, env);
}

extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit2(IScriptEnvironment* env)
{
  env->AddFunction("RawSource","[file]s[width]i[height]i[pixel_type]s[fpsnum]i[fpsden]i[index]s[show]b",Create_RawSource,0);
  return 0;
}
