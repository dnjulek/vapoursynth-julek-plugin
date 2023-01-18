#include "shared.h"

struct AUTOGAINData final {
    const VSVideoInfo* vi;
    VSNode* node;
    bool process_p[3];
    void (*process)(const uint8_t* VS_RESTRICT srcp, uint8_t* VS_RESTRICT dstp, ptrdiff_t stride, const int w, const int h) noexcept;
};

extern void autogainUC_avx2(const uint8_t* VS_RESTRICT srcp, uint8_t* VS_RESTRICT dstp, ptrdiff_t stride, const int w, const int h) noexcept;
template <const uint16_t peak>
extern void autogainUS_avx2(const uint8_t* VS_RESTRICT srcp, uint8_t* VS_RESTRICT dstp, ptrdiff_t stride, const int w, const int h) noexcept;
extern void autogainF_avx2(const uint8_t* VS_RESTRICT srcp, uint8_t* VS_RESTRICT dstp, ptrdiff_t stride, const int w, const int h) noexcept;

void minmaxUC_c(const uint8_t* VS_RESTRICT srcp, uint8_t& dst_min, uint8_t& dst_max, ptrdiff_t stride, const int w, const int h) noexcept {
    uint8_t imin = UCHAR_MAX;
    uint8_t imax = 0;
    for (int y{0}; y < h; y++) {
        for (int x{0}; x < w; x++) {
            uint8_t v = srcp[x];
            imin = VSMIN(imin, v);
            imax = VSMAX(imax, v);
        }
        srcp += stride;
    }
    dst_min = imin;
    dst_max = imax;
}

void minmaxUS_c(const uint8_t* VS_RESTRICT srcp, uint16_t& dst_min, uint16_t& dst_max, ptrdiff_t stride, const int w, const int h) noexcept {
    uint16_t imin = USHRT_MAX;
    uint16_t imax = 0;
    for (int y{0}; y < h; y++) {
        for (int x{0}; x < w; x++) {
            uint16_t v = ((const uint16_t*)srcp)[x];
            imin = VSMIN(imin, v);
            imax = VSMAX(imax, v);
        }
        srcp += stride;
    }
    dst_min = imin;
    dst_max = imax;
}

void minmaxF_c(const uint8_t* VS_RESTRICT srcp, float& dst_min, float& dst_max, ptrdiff_t stride, const int w, const int h) noexcept {
    float imin = 1.0f;
    float imax = 0.0f;
    for (int y{0}; y < h; y++) {
        for (int x{0}; x < w; x++) {
            float v = ((const float*)srcp)[x];
            imin = VSMIN(imin, v);
            imax = VSMAX(imax, v);
        }
        srcp += stride;
    }
    dst_min = imin;
    dst_max = imax;
}

void autogainUC_c(const uint8_t* VS_RESTRICT srcp, uint8_t* VS_RESTRICT dstp, ptrdiff_t stride, const int w, const int h) noexcept {
    uint8_t pmin, pmax;
    minmaxUC_c(srcp, pmin, pmax, stride, w, h);
    for (int y{0}; y < h; y++) {
        for (int x{0}; x < w; x++) {
            uint8_t v = srcp[x];
            dstp[x] = (uint8_t)(((v - pmin) / (float)(pmax - pmin)) * 255 + 0.5f);
        }
        srcp += stride;
        dstp += stride;
    }
}

template <const uint16_t peak>
void autogainUS_c(const uint8_t* VS_RESTRICT srcp, uint8_t* VS_RESTRICT dstp, ptrdiff_t stride, const int w, const int h) noexcept {
    uint16_t pmin, pmax;
    minmaxUS_c(srcp, pmin, pmax, stride, w, h);
    for (int y{0}; y < h; y++) {
        for (int x{0}; x < w; x++) {
            uint16_t v = ((const uint16_t*)srcp)[x];
            ((uint16_t*)dstp)[x] = (uint16_t)(((v - pmin) / (float)(pmax - pmin)) * peak + 0.5f);
        }
        srcp += stride;
        dstp += stride;
    }
}

void autogainF_c(const uint8_t* VS_RESTRICT srcp, uint8_t* VS_RESTRICT dstp, ptrdiff_t stride, const int w, const int h) noexcept {
    float pmin, pmax;
    minmaxF_c(srcp, pmin, pmax, stride, w, h);
    for (int y{0}; y < h; y++) {
        for (int x{0}; x < w; x++) {
            float v = ((const float*)srcp)[x];
            ((float*)dstp)[x] = (v - pmin) / (pmax - pmin);
        }
        srcp += stride;
        dstp += stride;
    }
}

static const VSFrame* VS_CC autogainGetFrame(int n, int activationReason, void* instanceData, void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi) {
    auto d{static_cast<AUTOGAINData*>(instanceData)};

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        const VSFrame* src = vsapi->getFrameFilter(n, d->node, frameCtx);

        const int pl[] = {0, 1, 2};
        const VSFrame* fr[] = {d->process_p[0] ? nullptr : src, d->process_p[1] ? nullptr : src, d->process_p[2] ? nullptr : src};
        VSFrame* dst = vsapi->newVideoFrame2(&d->vi->format, d->vi->width, d->vi->height, fr, pl, src, core);

        for (int plane{0}; plane < d->vi->format.numPlanes; plane++) {
            if (d->process_p[plane]) {
                const ptrdiff_t stride = vsapi->getStride(src, plane);
                const int width = vsapi->getFrameWidth(src, plane);
                const int height = vsapi->getFrameHeight(src, plane);
                const uint8_t* srcp = vsapi->getReadPtr(src, plane);
                uint8_t* dstp = vsapi->getWritePtr(dst, plane);

                d->process(srcp, dstp, stride, width, height);
            }
        }

        vsapi->freeFrame(src);
        VSMap* dstProps = vsapi->getFramePropertiesRW(dst);
        vsapi->mapSetInt(dstProps, "_ColorRange", 0, maReplace);
        return dst;
    }
    return nullptr;
}

static void VS_CC autogainFree(void* instanceData, VSCore* core, const VSAPI* vsapi) {
    auto d{static_cast<AUTOGAINData*>(instanceData)};
    vsapi->freeNode(d->node);
    delete d;
}

void VS_CC autogainCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi) {
    auto d{std::make_unique<AUTOGAINData>()};
    int err{0};
    int nPlan, nElem, getP, i;

    d->node = vsapi->mapGetNode(in, "clip", 0, nullptr);
    d->vi = vsapi->getVideoInfo(d->node);

    nPlan = d->vi->format.numPlanes;
    nElem = vsapi->mapNumElements(in, "planes");

    if (nElem <= 0) {
        if (d->vi->format.colorFamily == cfRGB) {
            for (i = 0; i < 3; i++)
                d->process_p[i] = true;
        } else {
            d->process_p[0] = true;
        }
    } else {
        for (i = 0; i < nElem; i++) {
            getP = vsapi->mapGetIntSaturated(in, "planes", i, nullptr);

            if (getP < 0 || getP >= nPlan) {
                vsapi->mapSetError(out, "AutoGain: plane index out of range");
                vsapi->freeNode(d->node);
                return;
            }

            if (d->process_p[getP]) {
                vsapi->mapSetError(out, "AutoGain: plane specified twice");
                vsapi->freeNode(d->node);
                return;
            }

            d->process_p[getP] = true;
        }
    }

#ifdef PLUGIN_X86
    switch (d->vi->format.bitsPerSample) {
        case 8:
            d->process = (instrset_detect() >= 8) ? autogainUC_avx2 : autogainUC_c;
            break;
        case 10:
            d->process = (instrset_detect() >= 8) ? autogainUS_avx2<1023> : autogainUS_c<1023>;
            break;
        case 12:
            d->process = (instrset_detect() >= 8) ? autogainUS_avx2<4095> : autogainUS_c<4095>;
            break;
        case 14:
            d->process = (instrset_detect() >= 8) ? autogainUS_avx2<16383> : autogainUS_c<16383>;
            break;
        case 16:
            d->process = (instrset_detect() >= 8) ? autogainUS_avx2<65535> : autogainUS_c<65535>;
            break;
        case 32:
            d->process = (instrset_detect() >= 8) ? autogainF_avx2 : autogainF_c;
            break;
    }
#else
    switch (d->vi->format.bitsPerSample) {
        case 8:
            d->process = autogainUC_c;
            break;
        case 10:
            d->process = autogainUS_c<1023>;
            break;
        case 12:
            d->process = autogainUS_c<4095>;
            break;
        case 14:
            d->process = autogainUS_c<16383>;
            break;
        case 16:
            d->process = autogainUS_c<65535>;
            break;
        case 32:
            d->process = autogainF_c;
            break;
    }
#endif

    VSFilterDependency deps[] = {{d->node, rpGeneral}};
    vsapi->createVideoFilter(out, "AutoGain", d->vi, autogainGetFrame, autogainFree, fmParallel, deps, 1, d.get(), core);
    d.release();
}