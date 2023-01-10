#ifdef PLUGIN_X86
#include "../shared.h"

FORCE_INLINE void get_mask_avx2_8(const uint8_t* VS_RESTRICT srcp, uint8_t* VS_RESTRICT dstp, const ptrdiff_t stride, const float frange[], const int width, const int height, const float scaling) {
    uint8_t lut[256];

    for (int i{0}; i < 256; i += 8) {
        Vec8f srcv = Vec8f().load_a(frange + i);
        Vec8f result = (255.0f * pow(srcv, scaling));
        compress_saturated_s2u(compress_saturated(min(max(truncatei(result + 0.5f), zero_si256()), 255), zero_si256()), zero_si256()).get_low().storel(lut + i);
    }

    for (int y{0}; y < height; y++) {
        for (int x{0}; x < width; x++) {
            dstp[x] = lut[srcp[x]];
        }
        srcp += stride;
        dstp += stride;
    }
}

FORCE_INLINE void get_mask_avx2_16(const uint16_t* VS_RESTRICT srcp, uint16_t* VS_RESTRICT dstp, const ptrdiff_t stride, const float frange[], const int width, const int height, const float scaling, const int peak, const int shift) {
    uint16_t lut[256];

    for (int i{0}; i < 256; i += 8) {
        Vec8f srcv = Vec8f().load_a(frange + i);
        Vec8f result = (static_cast<const float>(peak) * pow(srcv, scaling));
        compress_saturated_s2u(min(max(truncatei(result + 0.5f), zero_si256()), peak), zero_si256()).get_low().store_a(lut + i);
    }

    for (int y{0}; y < height; y++) {
        for (int x{0}; x < width; x++) {
            dstp[x] = (lut[srcp[x] >> shift]);
        }
        srcp += stride;
        dstp += stride;
    }
}

FORCE_INLINE void get_mask_avx2_f(const float* VS_RESTRICT srcp, float* VS_RESTRICT dstp, const ptrdiff_t stride, const int width, const int height, const float scaling) {
    for (int y{0}; y < height; y++) {
        for (int x{0}; x < width; x += 8) {
            Vec8f srcv = Vec8f().load(srcp + x);
            min(max(pow((1.0f - (srcv * mul_add(srcv, mul_add(srcv, mul_add(srcv, mul_add(srcv, 18.188f, -45.47f), 36.624f), -9.466f), 1.124f))), scaling), zero_8f()), 1.0f).store_a(dstp + x);
        }
        srcp += stride;
        dstp += stride;
    }
}

template <typename pixel_t>
void agm_process_avx2(const VSFrame* src, VSFrame* dst, float& avg, const AGMData* const VS_RESTRICT d, const VSAPI* vsapi) noexcept {
    for (int plane{0}; plane < d->vi->format.numPlanes; plane++) {
        const auto width{vsapi->getFrameWidth(src, plane)};
        const auto height{vsapi->getFrameHeight(src, plane)};
        const auto stride{vsapi->getStride(src, plane) / d->vi->format.bytesPerSample};

        auto srcp{reinterpret_cast<const pixel_t*>(vsapi->getReadPtr(src, plane))};
        auto dstp{reinterpret_cast<pixel_t*>(vsapi->getWritePtr(dst, plane))};
        const float scaling{avg * avg * d->luma_scaling};

        if constexpr (std::is_same_v<pixel_t, uint8_t>) {
            get_mask_avx2_8(srcp, dstp, stride, d->float_range, width, height, scaling);
        } else if constexpr (std::is_same_v<pixel_t, uint16_t>) {
            get_mask_avx2_16(srcp, dstp, stride, d->float_range, width, height, scaling, d->peak, d->shift);
        } else {
            get_mask_avx2_f(srcp, dstp, stride, width, height, scaling);
        }
    }
}

template void agm_process_avx2<uint8_t>(const VSFrame* src, VSFrame* dst, float& avg, const AGMData* const VS_RESTRICT d, const VSAPI* vsapi) noexcept;
template void agm_process_avx2<uint16_t>(const VSFrame* src, VSFrame* dst, float& avg, const AGMData* const VS_RESTRICT d, const VSAPI* vsapi) noexcept;
template void agm_process_avx2<float>(const VSFrame* src, VSFrame* dst, float& avg, const AGMData* const VS_RESTRICT d, const VSAPI* vsapi) noexcept;
#endif