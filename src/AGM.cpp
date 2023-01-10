#include "shared.h"

template <typename pixel_t>
extern void agm_process_avx2(const VSFrame* src, VSFrame* dst, float& avg, const AGMData* const VS_RESTRICT d, const VSAPI* vsapi) noexcept;

template <typename pixel_t>
void agm_process_c(const VSFrame* src, VSFrame* dst, float& avg, const AGMData* const VS_RESTRICT d, const VSAPI* vsapi) noexcept {
    for (int plane{0}; plane < d->vi->format.numPlanes; plane++) {
        const auto width{vsapi->getFrameWidth(src, plane)};
        const auto height{vsapi->getFrameHeight(src, plane)};
        const auto stride{vsapi->getStride(src, plane) / d->vi->format.bytesPerSample};

        auto srcp{reinterpret_cast<const pixel_t*>(vsapi->getReadPtr(src, plane))};
        auto dstp{reinterpret_cast<pixel_t*>(vsapi->getWritePtr(dst, plane))};
        const float scaling{avg * avg * d->luma_scaling};
        const float peak{static_cast<const float>(d->peak)};

        pixel_t lut[256];

        for (int i{0}; i < 256; i++) {
            lut[i] = static_cast<pixel_t>(std::clamp((std::pow(d->float_range[i], scaling) * peak + 0.5f), 0.0f, peak));
        }

        for (int y{0}; y < height; y++) {
            for (int x{0}; x < width; x++) {
                if constexpr (std::is_integral_v<pixel_t>) {
                    dstp[x] = lut[srcp[x] >> d->shift];
                } else {
                    dstp[x] = std::clamp(std::pow(1.0f - (srcp[x] * ((srcp[x] * ((srcp[x] * ((srcp[x] * ((srcp[x] * 18.188f) - 45.47f)) + 36.624f)) - 9.466f)) + 1.124f)), scaling), 0.0f, 1.0f);
                }
            }
            srcp += stride;
            dstp += stride;
        }
    }
}

static const VSFrame* VS_CC agmGetFrame(int n, int activationReason, void* instanceData, void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi) {
    auto d{static_cast<AGMData*>(instanceData)};

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        const VSFrame* src = vsapi->getFrameFilter(n, d->node, frameCtx);

        const VSVideoFormat* fi = vsapi->getVideoFrameFormat(src);
        int srcw = vsapi->getFrameWidth(src, 0);
        int srch = vsapi->getFrameHeight(src, 0);

        VSFrame* dst = vsapi->newVideoFrame(fi, srcw, srch, src, core);

        float avg = vsapi->mapGetFloat(vsapi->getFramePropertiesRO(src), "PlaneStatsAverage", 0, nullptr);
        d->process(src, dst, avg, d, vsapi);

        vsapi->freeFrame(src);
        VSMap* dstProps = vsapi->getFramePropertiesRW(dst);
        vsapi->mapSetInt(dstProps, "_ColorRange", 0, maReplace);
        return dst;
    }
    return nullptr;
}

static void VS_CC agmFree(void* instanceData, VSCore* core, const VSAPI* vsapi) {
    auto d{static_cast<AGMData*>(instanceData)};
    vsapi->freeNode(d->node);
    delete d;
}

void VS_CC agmCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi) {
    auto d{std::make_unique<AGMData>()};
    int err = 0;

    d->node = vsapi->mapGetNode(in, "clip", 0, nullptr);
    d->vi = vsapi->getVideoInfo(d->node);

    d->luma_scaling = vsapi->mapGetFloatSaturated(in, "luma_scaling", 0, &err);
    if (err)
        d->luma_scaling = 10.0f;

    int bits = d->vi->format.bitsPerSample;
    d->shift = bits - 8;
    d->peak = (1 << bits) - 1;

    for (int i{0}; i < 256; i++) {
        const float x{i / 256.0f};
        d->float_range[i] = (1.0f - (x * ((x * ((x * ((x * ((x * 18.188f) - 45.47f)) + 36.624f)) - 9.466f)) + 1.124f)));
    }

    VSMap* args = vsapi->createMap();
    vsapi->mapConsumeNode(args, "clipa", d->node, maAppend);
    vsapi->mapSetData(args, "prop", "PlaneStats", -1, dtUtf8, maAppend);
    VSPlugin* stdplugin = vsapi->getPluginByID(VSH_STD_PLUGIN_ID, core);
    VSMap* ret = vsapi->invoke(stdplugin, "PlaneStats", args);
    d->node = vsapi->mapGetNode(ret, "clip", 0, nullptr);
    vsapi->freeMap(args);
    vsapi->freeMap(ret);

    if (d->vi->format.bytesPerSample == 1) {
        d->process = agm_process_c<uint8_t>;
#ifdef PLUGIN_X86
        d->process = (instrset_detect() >= 8) ? agm_process_avx2<uint8_t> : agm_process_c<uint8_t>;
#endif
    } else if (d->vi->format.bytesPerSample == 2) {
        d->process = agm_process_c<uint16_t>;
#ifdef PLUGIN_X86
        d->process = (instrset_detect() >= 8) ? agm_process_avx2<uint16_t> : agm_process_c<uint16_t>;
#endif
    } else {
        d->process = agm_process_c<float>;
#ifdef PLUGIN_X86
        d->process = (instrset_detect() >= 8) ? agm_process_avx2<float> : agm_process_c<float>;
#endif
    }

    VSFilterDependency deps[] = {{d->node, rpGeneral}};
    vsapi->createVideoFilter(out, "AGM", d->vi, agmGetFrame, agmFree, fmParallel, deps, 1, d.get(), core);
    d.release();
}