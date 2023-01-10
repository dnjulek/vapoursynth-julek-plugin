#include "shared.h"

struct BUTTERAUGLIData final {
    VSNode* node;
    VSNode* node2;
    const VSVideoInfo* vi;
    jxl::ButteraugliParams ba_params;
    bool distmap;
    bool linput;

    void (*hmap)(VSFrame* dst, const jxl::ImageF& distmap, int width, int height, const ptrdiff_t stride, const VSAPI* vsapi) noexcept;
    void (*fill)(jxl::CodecInOut& ref, jxl::CodecInOut& dist, const VSFrame* src1, const VSFrame* src2, int width, int height, const ptrdiff_t stride, const VSAPI* vsapi) noexcept;
};

template <typename pixel_t, typename jxl_t, bool linput>
extern void fill_image(jxl::CodecInOut& ref, jxl::CodecInOut& dist, const VSFrame* src1, const VSFrame* src2, int width, int height, const ptrdiff_t stride, const VSAPI* vsapi) noexcept;
template <bool linput>
extern void fill_imageF(jxl::CodecInOut& ref, jxl::CodecInOut& dist, const VSFrame* src1, const VSFrame* src2, int width, int height, const ptrdiff_t stride, const VSAPI* vsapi) noexcept;

template <typename pixel_t, typename jxl_t, int peak>
static void heatmap(VSFrame* dst, const jxl::ImageF& distmap, int width, int height, const ptrdiff_t stride, const VSAPI* vsapi) noexcept {
    jxl::Image3F buff = jxl::CreateHeatMapImage(distmap, jxl::ButteraugliFuzzyInverse(1.5), jxl::ButteraugliFuzzyInverse(0.5));
    jxl_t tmp(width, height);
    jxl::Image3Convert(buff, peak, &tmp);

    for (int i = 0; i < 3; i++) {
        auto dstp{reinterpret_cast<pixel_t*>(vsapi->getWritePtr(dst, i))};
        for (int y = 0; y < height; y++) {
            memcpy(dstp, tmp.ConstPlaneRow(i, y), width * sizeof(pixel_t));
            dstp += stride;
        }
    }
}

static void heatmapF(VSFrame* dst, const jxl::ImageF& distmap, int width, int height, const ptrdiff_t stride, const VSAPI* vsapi) noexcept {
    jxl::Image3F buff = jxl::CreateHeatMapImage(distmap, jxl::ButteraugliFuzzyInverse(1.5), jxl::ButteraugliFuzzyInverse(0.5));

    for (int i = 0; i < 3; i++) {
        float* dstp{reinterpret_cast<float*>(vsapi->getWritePtr(dst, i))};
        for (int y = 0; y < height; y++) {
            memcpy(dstp, buff.ConstPlaneRow(i, y), width * sizeof(float));
            dstp += stride;
        }
    }
}

static const VSFrame* VS_CC butteraugliGetFrame(int n, int activationReason, void* instanceData, void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi) {
    auto d{static_cast<BUTTERAUGLIData*>(instanceData)};

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
        vsapi->requestFrameFilter(n, d->node2, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        const VSFrame* src = vsapi->getFrameFilter(n, d->node, frameCtx);
        const VSFrame* src2 = vsapi->getFrameFilter(n, d->node2, frameCtx);

        int width = vsapi->getFrameWidth(src2, 0);
        int height = vsapi->getFrameHeight(src2, 0);
        const ptrdiff_t stride = vsapi->getStride(src2, 0) / d->vi->format.bytesPerSample;

        jxl::CodecInOut ref;
        jxl::CodecInOut dist;
        jxl::ImageF diff_map;

        ref.SetSize(width, height);
        dist.SetSize(width, height);

        if (d->linput) {
            ref.metadata.m.color_encoding = jxl::ColorEncoding::LinearSRGB(false);
            dist.metadata.m.color_encoding = jxl::ColorEncoding::LinearSRGB(false);
        } else {
            ref.metadata.m.color_encoding = jxl::ColorEncoding::SRGB(false);
            dist.metadata.m.color_encoding = jxl::ColorEncoding::SRGB(false);
        }

        d->fill(ref, dist, src, src2, width, height, stride, vsapi);

        if (d->distmap) {
            VSFrame* dst = vsapi->newVideoFrame(vsapi->getVideoFrameFormat(src2), width, height, src2, core);
            double diff_value = jxl::ButteraugliDistance(ref.Main(), dist.Main(), d->ba_params, jxl::GetJxlCms(), &diff_map, nullptr);
            d->hmap(dst, diff_map, width, height, stride, vsapi);
            VSMap* dstProps = vsapi->getFramePropertiesRW(dst);
            vsapi->mapSetFloat(dstProps, "_FrameButteraugli", diff_value, maReplace);

            vsapi->freeFrame(src);
            vsapi->freeFrame(src2);
            return dst;
        } else {
            VSFrame* dst = vsapi->copyFrame(src2, core);
            VSMap* dstProps = vsapi->getFramePropertiesRW(dst);
            vsapi->mapSetFloat(dstProps, "_FrameButteraugli", jxl::ButteraugliDistance(ref.Main(), dist.Main(), d->ba_params, jxl::GetJxlCms(), &diff_map, nullptr), maReplace);

            vsapi->freeFrame(src);
            vsapi->freeFrame(src2);
            return dst;
        }
    }
    return nullptr;
}

static void VS_CC butteraugliFree(void* instanceData, VSCore* core, const VSAPI* vsapi) {
    auto d{reinterpret_cast<BUTTERAUGLIData*>(instanceData)};

    vsapi->freeNode(d->node);
    vsapi->freeNode(d->node2);
    delete d;
}

void VS_CC butteraugliCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi) {
    auto d{std::make_unique<BUTTERAUGLIData>()};
    int err{0};

    d->node = vsapi->mapGetNode(in, "reference", 0, nullptr);
    d->node2 = vsapi->mapGetNode(in, "distorted", 0, nullptr);
    d->vi = vsapi->getVideoInfo(d->node);

    d->distmap = !!vsapi->mapGetInt(in, "distmap", 0, &err);
    if (err)
        d->distmap = false;

    d->linput = !!vsapi->mapGetInt(in, "linput", 0, &err);
    if (err)
        d->linput = false;

    float intensity_target;
    intensity_target = vsapi->mapGetFloatSaturated(in, "intensity_target", 0, &err);
    if (err)
        intensity_target = 80.0f;

    d->ba_params.hf_asymmetry = 0.8f;
    d->ba_params.xmul = 1.0f;
    d->ba_params.intensity_target = intensity_target;

    if (intensity_target <= 0.0f) {
        vsapi->mapSetError(out, "Butteraugli: intensity_target must be greater than 0.0.");
        vsapi->freeNode(d->node);
        vsapi->freeNode(d->node2);
        return;
    }

    if (d->vi->format.colorFamily != cfRGB) {
        vsapi->mapSetError(out, "Butteraugli: the clip must be in RGB format.");
        vsapi->freeNode(d->node);
        vsapi->freeNode(d->node2);
        return;
    }

    int bits = d->vi->format.bitsPerSample;
    if (bits != 8 && bits != 16 && bits != 32) {
        vsapi->mapSetError(out, "Butteraugli: the clip bit depth must be 8, 16, or 32.");
        vsapi->freeNode(d->node);
        vsapi->freeNode(d->node2);
        return;
    }

    if (!vsh::isSameVideoInfo(vsapi->getVideoInfo(d->node2), d->vi)) {
        vsapi->mapSetError(out, "Butteraugli: both clips must have the same format and dimensions.");
        vsapi->freeNode(d->node);
        vsapi->freeNode(d->node2);
        return;
    }

    switch (d->vi->format.bytesPerSample) {
        case 1:
            d->fill = (d->linput) ? fill_image<uint8_t, jxl::Image3B, true> : fill_image<uint8_t, jxl::Image3B, false>;
            d->hmap = heatmap<uint8_t, jxl::Image3B, 255>;
            break;
        case 2:
            d->fill = (d->linput) ? fill_image<uint16_t, jxl::Image3U, true> : fill_image<uint16_t, jxl::Image3U, false>;
            d->hmap = heatmap<uint16_t, jxl::Image3U, 65535>;
            break;
        case 4:
            d->fill = (d->linput) ? fill_imageF<true> : fill_imageF<false>;
            d->hmap = heatmapF;
            break;
    }

    VSFilterDependency deps[]{{d->node, rpGeneral}, {d->node2, rpGeneral}};
    vsapi->createVideoFilter(out, "Butteraugli", d->vi, butteraugliGetFrame, butteraugliFree, fmParallel, deps, 2, d.get(), core);
    d.release();
}