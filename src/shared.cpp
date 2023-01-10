#include "shared.h"

VS_EXTERNAL_API(void)
VapourSynthPluginInit2(VSPlugin* plugin, const VSPLUGINAPI* vspapi) {
    vspapi->configPlugin("com.julek.plugin", "julek", "Julek filters", 3, VAPOURSYNTH_API_VERSION, 0, plugin);
    vspapi->registerFunction("AGM", "clip:vnode;luma_scaling:float:opt;", "clip:vnode;", agmCreate, nullptr, plugin);
    vspapi->registerFunction("Butteraugli", "reference:vnode;distorted:vnode;distmap:int:opt;intensity_target:float:opt;linput:int:opt;", "clip:vnode;", butteraugliCreate, nullptr, plugin);
    vspapi->registerFunction("ColorMap", "clip:vnode;type:int:opt;", "clip:vnode;", colormapCreate, nullptr, plugin);
    vspapi->registerFunction("RFS", "clip_a:vnode;clip_b:vnode;frames:int[];mismatch:int:opt;", "clip:vnode;", rfsCreate, nullptr, plugin);
    vspapi->registerFunction("SSIMULACRA", "reference:vnode;distorted:vnode;feature:int:opt;simple:int:opt;", "clip:vnode;", ssimulacraCreate, nullptr, plugin);
}

template <typename pixel_t, typename jxl_t, bool linput>
void fill_image(jxl::CodecInOut& ref, jxl::CodecInOut& dist, const VSFrame* src1, const VSFrame* src2, int width, int height, const ptrdiff_t stride, const VSAPI* vsapi) noexcept {
    jxl_t tmp1(width, height);
    jxl_t tmp2(width, height);

    for (int i = 0; i < 3; ++i) {
        auto srcp1{reinterpret_cast<const pixel_t*>(vsapi->getReadPtr(src1, i))};
        auto srcp2{reinterpret_cast<const pixel_t*>(vsapi->getReadPtr(src2, i))};

        for (int y = 0; y < height; ++y) {
            memcpy(tmp1.PlaneRow(i, y), srcp1, width * sizeof(pixel_t));
            memcpy(tmp2.PlaneRow(i, y), srcp2, width * sizeof(pixel_t));

            srcp1 += stride;
            srcp2 += stride;
        }
    }

    ref.SetFromImage(std::move(jxl::ConvertToFloat(tmp1)), (linput) ? jxl::ColorEncoding::LinearSRGB(false) : jxl::ColorEncoding::SRGB(false));
    dist.SetFromImage(std::move(jxl::ConvertToFloat(tmp2)), (linput) ? jxl::ColorEncoding::LinearSRGB(false) : jxl::ColorEncoding::SRGB(false));
}

template <bool linput>
void fill_imageF(jxl::CodecInOut& ref, jxl::CodecInOut& dist, const VSFrame* src1, const VSFrame* src2, int width, int height, const ptrdiff_t stride, const VSAPI* vsapi) noexcept {
    jxl::Image3F tmp1(width, height);
    jxl::Image3F tmp2(width, height);

    for (int i = 0; i < 3; ++i) {
        const float* srcp1{reinterpret_cast<const float*>(vsapi->getReadPtr(src1, i))};
        const float* srcp2{reinterpret_cast<const float*>(vsapi->getReadPtr(src2, i))};

        for (int y = 0; y < height; ++y) {
            memcpy(tmp1.PlaneRow(i, y), srcp1, width * sizeof(float));
            memcpy(tmp2.PlaneRow(i, y), srcp2, width * sizeof(float));

            srcp1 += stride;
            srcp2 += stride;
        }
    }

    ref.SetFromImage(std::move(tmp1), (linput) ? jxl::ColorEncoding::LinearSRGB(false) : jxl::ColorEncoding::SRGB(false));
    dist.SetFromImage(std::move(tmp2), (linput) ? jxl::ColorEncoding::LinearSRGB(false) : jxl::ColorEncoding::SRGB(false));
}

template void fill_image<uint8_t, jxl::Image3B, true>(jxl::CodecInOut& ref, jxl::CodecInOut& dist, const VSFrame* src1, const VSFrame* src2, int width, int height, const ptrdiff_t stride, const VSAPI* vsapi) noexcept;
template void fill_image<uint16_t, jxl::Image3U, true>(jxl::CodecInOut& ref, jxl::CodecInOut& dist, const VSFrame* src1, const VSFrame* src2, int width, int height, const ptrdiff_t stride, const VSAPI* vsapi) noexcept;

template void fill_image<uint8_t, jxl::Image3B, false>(jxl::CodecInOut& ref, jxl::CodecInOut& dist, const VSFrame* src1, const VSFrame* src2, int width, int height, const ptrdiff_t stride, const VSAPI* vsapi) noexcept;
template void fill_image<uint16_t, jxl::Image3U, false>(jxl::CodecInOut& ref, jxl::CodecInOut& dist, const VSFrame* src1, const VSFrame* src2, int width, int height, const ptrdiff_t stride, const VSAPI* vsapi) noexcept;

template void fill_imageF<true>(jxl::CodecInOut& ref, jxl::CodecInOut& dist, const VSFrame* src1, const VSFrame* src2, int width, int height, const ptrdiff_t stride, const VSAPI* vsapi) noexcept;
template void fill_imageF<false>(jxl::CodecInOut& ref, jxl::CodecInOut& dist, const VSFrame* src1, const VSFrame* src2, int width, int height, const ptrdiff_t stride, const VSAPI* vsapi) noexcept;