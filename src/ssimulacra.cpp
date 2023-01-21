#include "shared.h"

struct SSIMULACRAData final {
    VSNode* node;
    VSNode* node2;
    const VSVideoInfo* vi;
    bool simple;
    int feature;

    void (*fill)(jxl::CodecInOut& ref, jxl::CodecInOut& dist, const VSFrame* src1, const VSFrame* src2, int width, int height, const ptrdiff_t stride, const VSAPI* vsapi) noexcept;
};

template <typename pixel_t, typename jxl_t, bool linput>
extern void fill_image(jxl::CodecInOut& ref, jxl::CodecInOut& dist, const VSFrame* src1, const VSFrame* src2, int width, int height, const ptrdiff_t stride, const VSAPI* vsapi) noexcept;
template <bool linput>
extern void fill_imageF(jxl::CodecInOut& ref, jxl::CodecInOut& dist, const VSFrame* src1, const VSFrame* src2, int width, int height, const ptrdiff_t stride, const VSAPI* vsapi) noexcept;

static const VSFrame* VS_CC ssimulacraGetFrame(int n, int activationReason, void* instanceData, void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi) {
    auto d{static_cast<SSIMULACRAData*>(instanceData)};

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

        d->fill(ref, dist, src, src2, width, height, stride, vsapi);

        VSFrame* dst = vsapi->copyFrame(src2, core);
        VSMap* dstProps = vsapi->getFramePropertiesRW(dst);

        if (!d->feature || d->feature == 2) {
            Msssim msssim{ComputeSSIMULACRA2(ref.Main(), dist.Main())};
            vsapi->mapSetFloat(dstProps, "_SSIMULACRA2", msssim.Score(), maReplace);
        }

        if (d->feature) {
            JXL_CHECK(ref.TransformTo(jxl::ColorEncoding::LinearSRGB(false), jxl::GetJxlCms()));
            JXL_CHECK(dist.TransformTo(jxl::ColorEncoding::LinearSRGB(false), jxl::GetJxlCms()));
            ssimulacra::Ssimulacra ssimulacra_{ssimulacra::ComputeDiff(*ref.Main().color(), *dist.Main().color(), d->simple)};
            vsapi->mapSetFloat(dstProps, "_SSIMULACRA", ssimulacra_.Score(), maReplace);
        }

        vsapi->freeFrame(src);
        vsapi->freeFrame(src2);
        return dst;
    }
    return nullptr;
}

static void VS_CC ssimulacraFree(void* instanceData, VSCore* core, const VSAPI* vsapi) {
    auto d{reinterpret_cast<SSIMULACRAData*>(instanceData)};

    vsapi->freeNode(d->node);
    vsapi->freeNode(d->node2);
    delete d;
}

void VS_CC ssimulacraCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi) {
    auto d{std::make_unique<SSIMULACRAData>()};
    int err{0};

    d->node = vsapi->mapGetNode(in, "reference", 0, nullptr);
    d->node2 = vsapi->mapGetNode(in, "distorted", 0, nullptr);
    d->vi = vsapi->getVideoInfo(d->node);

    d->feature = vsapi->mapGetIntSaturated(in, "feature", 0, &err);
    if (err)
        d->feature = 0;

    d->simple = !!vsapi->mapGetInt(in, "simple", 0, &err);
    if (err)
        d->simple = false;

    if (d->vi->format.colorFamily != cfRGB) {
        vsapi->mapSetError(out, "SSIMULACRA: the clip must be in RGB format.");
        vsapi->freeNode(d->node);
        vsapi->freeNode(d->node2);
        return;
    }

    int bits = d->vi->format.bitsPerSample;
    if (bits != 8 && bits != 16 && bits != 32) {
        vsapi->mapSetError(out, "SSIMULACRA: the clip bit depth must be 8, 16, or 32.");
        vsapi->freeNode(d->node);
        vsapi->freeNode(d->node2);
        return;
    }

    if (!vsh::isSameVideoInfo(vsapi->getVideoInfo(d->node2), d->vi)) {
        vsapi->mapSetError(out, "SSIMULACRA: both clips must have the same format and dimensions.");
        vsapi->freeNode(d->node);
        vsapi->freeNode(d->node2);
        return;
    }

    if (d->feature < 0 || d->feature > 2) {
        vsapi->mapSetError(out, "SSIMULACRA: feature must be 0, 1, or 2.");
        vsapi->freeNode(d->node);
        vsapi->freeNode(d->node2);
        return;
    }

    if (d->vi->height < 8 || d->vi->width < 8) {
        vsapi->mapSetError(out, "SSIMULACRA: minimum image size is 8x8 pixels.");
        vsapi->freeNode(d->node);
        vsapi->freeNode(d->node2);
        return;
    }

    switch (d->vi->format.bytesPerSample) {
        case 1:
            d->fill = fill_image<uint8_t, jxl::Image3B, false>;
            break;
        case 2:
            d->fill = fill_image<uint16_t, jxl::Image3U, false>;
            break;
        case 4:
            d->fill = fill_imageF<false>;
            break;
    }

    VSFilterDependency deps[]{{d->node, rpGeneral}, {d->node2, rpGeneral}};
    vsapi->createVideoFilter(out, "SSIMULACRA", d->vi, ssimulacraGetFrame, ssimulacraFree, fmParallel, deps, 2, d.get(), core);
    d.release();
}