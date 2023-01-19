#include "shared.h"

struct VISUALIZEDIFFSData final {
    const VSVideoInfo* vi_in;
    VSVideoInfo vi_out;
    VSNode* node1;
    VSNode* node2;
    bool auto_gain;
    int type;
    void (*autogain_process)(const uint8_t* VS_RESTRICT srcp, uint8_t* VS_RESTRICT dstp, ptrdiff_t stride, const int w, const int h) noexcept;
};

extern void colormap_process(const uint8_t* srcp, VSFrame* dst, const ptrdiff_t stride, const int w, const int h, int type, const VSAPI* vsapi) noexcept;
extern void autogainUC_c(const uint8_t* VS_RESTRICT srcp, uint8_t* VS_RESTRICT dstp, ptrdiff_t stride, const int w, const int h) noexcept;
extern void autogainUC_avx2(const uint8_t* VS_RESTRICT srcp, uint8_t* VS_RESTRICT dstp, ptrdiff_t stride, const int w, const int h) noexcept;

static const VSFrame* VS_CC visualizediffsGetFrame(int n, int activationReason, void* instanceData, void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi) {
    auto d{static_cast<VISUALIZEDIFFSData*>(instanceData)};

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->node1, frameCtx);
        vsapi->requestFrameFilter(n, d->node2, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        const VSFrame* src1 = vsapi->getFrameFilter(n, d->node1, frameCtx);
        const VSFrame* src2 = vsapi->getFrameFilter(n, d->node2, frameCtx);
        const int width = vsapi->getFrameWidth(src1, 0);
        const int height = vsapi->getFrameHeight(src1, 0);
        const ptrdiff_t stride = vsapi->getStride(src1, 0);
        VSFrame* dst = vsapi->newVideoFrame(&d->vi_out.format, width, height, src1, core);
        uint8_t* tmpp = vsh::vsh_aligned_malloc<uint8_t>(sizeof(uint8_t) * stride * height, 32);
        uint8_t* tmpp2 = d->auto_gain ? vsapi->getWritePtr(dst, 0) : tmpp;

        for (int plane{0}; plane < d->vi_in->format.numPlanes; plane++) {
            const ptrdiff_t stride = vsapi->getStride(src1, plane);
            const uint8_t* src1p = vsapi->getReadPtr(src1, plane);
            const uint8_t* src2p = vsapi->getReadPtr(src2, plane);
            uint8_t* dstp = vsapi->getWritePtr(dst, plane);

            if (plane == 0) {
                for (int y{0}; y < height; y++) {
                    for (int x{0}; x < width; x++) {
                        tmpp2[x] = std::abs(src1p[x] - src2p[x]);
                    }
                    src1p += stride;
                    src2p += stride;
                    tmpp2 += stride;
                }
            } else {
                for (int y{0}; y < height; y++) {
                    for (int x{0}; x < width; x++) {
                        tmpp2[x] = tmpp2[x] + std::abs(src1p[x] - src2p[x]);
                    }
                    src1p += stride;
                    src2p += stride;
                    tmpp2 += stride;
                }
            }
            tmpp2 -= stride * height;
        }

        if (d->auto_gain) {
            d->autogain_process(tmpp2, tmpp, stride, width, height);
            colormap_process(tmpp, dst, stride, width, height, d->type, vsapi);
        } else {
            colormap_process(tmpp2, dst, stride, width, height, d->type, vsapi);
        }

        vsapi->freeFrame(src1);
        vsapi->freeFrame(src2);
        vsh::vsh_aligned_free(tmpp);
        VSMap* dstProps = vsapi->getFramePropertiesRW(dst);
        vsapi->mapSetInt(dstProps, "_ColorRange", 0, maReplace);
        return dst;
    }
    return nullptr;
}

static void VS_CC visualizediffsFree(void* instanceData, VSCore* core, const VSAPI* vsapi) {
    auto d{static_cast<VISUALIZEDIFFSData*>(instanceData)};
    vsapi->freeNode(d->node1);
    vsapi->freeNode(d->node2);
    delete d;
}

void VS_CC visualizediffsCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi) {
    auto d{std::make_unique<VISUALIZEDIFFSData>()};
    int err{0};

    d->node1 = vsapi->mapGetNode(in, "clip_a", 0, nullptr);
    d->node2 = vsapi->mapGetNode(in, "clip_b", 0, nullptr);

    d->vi_in = vsapi->getVideoInfo(d->node1);
    d->vi_out = *d->vi_in;
    vsapi->queryVideoFormat(&d->vi_out.format, cfRGB, stInteger, 8, 0, 0, core);

    d->auto_gain = !!vsapi->mapGetInt(in, "auto_gain", 0, &err);
    if (err)
        d->auto_gain = true;

    d->type = vsapi->mapGetIntSaturated(in, "type", 0, &err);
    if (err)
        d->type = 20;

    if (!vsh::isSameVideoInfo(vsapi->getVideoInfo(d->node2), d->vi_in)) {
        vsapi->mapSetError(out, "VisualizeDiffs: both clips must have the same format and dimensions.");
        vsapi->freeNode(d->node1);
        vsapi->freeNode(d->node2);
        return;
    }

    if (d->vi_in->format.bytesPerSample != 1) {
        vsapi->mapSetError(out, "VisualizeDiffs: only 8-bits clip is supported.");
        vsapi->freeNode(d->node1);
        vsapi->freeNode(d->node2);
        return;
    }

    if (d->vi_in->format.colorFamily == cfYUV) {
        int matrix = (d->vi_in->height > 650) ? 1 : 6;
        VSMap* args = vsapi->createMap();
        vsapi->mapConsumeNode(args, "clip", d->node1, maReplace);
        vsapi->mapSetInt(args, "matrix_in", matrix, maReplace);
        vsapi->mapSetInt(args, "format", pfRGB24, maReplace);

        VSPlugin* vsplugin = vsapi->getPluginByID(VSH_RESIZE_PLUGIN_ID, core);
        VSMap* ret = vsapi->invoke(vsplugin, "Bicubic", args);
        d->node1 = vsapi->mapGetNode(ret, "clip", 0, nullptr);
        vsapi->freeMap(ret);

        vsapi->mapConsumeNode(args, "clip", d->node2, maReplace);
        VSMap* ret2 = vsapi->invoke(vsplugin, "Bicubic", args);
        d->node2 = vsapi->mapGetNode(ret2, "clip", 0, nullptr);
        vsapi->freeMap(ret2);
        vsapi->freeMap(args);
    }

#ifdef PLUGIN_X86
    d->autogain_process = (instrset_detect() >= 8) ? autogainUC_avx2 : autogainUC_c;
#else
    d->autogain_process = autogainUC_c;
#endif

    VSFilterDependency deps[]{{d->node1, rpGeneral}, {d->node2, rpGeneral}};
    vsapi->createVideoFilter(out, "VisualizeDiffs", &d->vi_out, visualizediffsGetFrame, visualizediffsFree, fmParallel, deps, 2, d.get(), core);
    d.release();
}