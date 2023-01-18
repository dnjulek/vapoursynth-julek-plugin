#ifdef PLUGIN_X86
#include "../shared.h"

extern void minmaxUC_c(const uint8_t* VS_RESTRICT srcp, uint8_t& dst_min, uint8_t& dst_max, ptrdiff_t stride, const int w, const int h) noexcept;
extern void minmaxUS_c(const uint8_t* VS_RESTRICT srcp, uint16_t& dst_min, uint16_t& dst_max, ptrdiff_t stride, const int w, const int h) noexcept;
extern void minmaxF_c(const uint8_t* VS_RESTRICT srcp, float& dst_min, float& dst_max, ptrdiff_t stride, const int w, const int h) noexcept;

void autogainUC_avx2(const uint8_t* VS_RESTRICT srcp, uint8_t* VS_RESTRICT dstp, ptrdiff_t stride, const int w, const int h) noexcept {
    uint8_t pmin, pmax;
    minmaxUC_c(srcp, pmin, pmax, stride, w, h);
    for (int y{0}; y < h; y++) {
        for (int x{0}; x < w; x++) {
            uint8_t v = ((const uint8_t*)srcp)[x];
            ((uint8_t*)dstp)[x] = (uint8_t)(((v - pmin) / (float)(pmax - pmin)) * 255 + 0.5f);
        }
        srcp += stride;
        dstp += stride;
    }
}

template <const uint16_t peak>
void autogainUS_avx2(const uint8_t* VS_RESTRICT srcp, uint8_t* VS_RESTRICT dstp, ptrdiff_t stride, const int w, const int h) noexcept {
    uint16_t pmin, pmax;
    minmaxUS_c(srcp, pmin, pmax, stride, w, h);
    const float range = (float)(pmax - pmin);
    for (int y{0}; y < h; y++) {
        for (int x{0}; x < w; x += 16) {
            Vec16us v = (Vec16us().load(((const uint16_t*)srcp) + x)) - pmin;
            Vec8f v_l = to_float(extend_low(v)) / range * peak + 0.5f;
            Vec8f v_h = to_float(extend_high(v)) / range * peak + 0.5f;
            v = compress_saturated_s2u(truncatei(v_l), truncatei(v_h));
            v.store(((uint16_t*)dstp) + x);
        }
        srcp += stride;
        dstp += stride;
    }
}

void autogainF_avx2(const uint8_t* VS_RESTRICT srcp, uint8_t* VS_RESTRICT dstp, ptrdiff_t stride, const int w, const int h) noexcept {
    float pmin, pmax;
    minmaxF_c(srcp, pmin, pmax, stride, w, h);
    const float range = (float)(pmax - pmin);
    for (int y{0}; y < h; y++) {
        for (int x{0}; x < w; x += 8) {
            Vec8f v = (Vec8f().load(((const float*)srcp) + x)) - pmin;
            (v / range).store(((float*)dstp) + x);
        }
        srcp += stride;
        dstp += stride;
    }
}

template void autogainUS_avx2<1023>(const uint8_t* VS_RESTRICT srcp, uint8_t* VS_RESTRICT dstp, ptrdiff_t stride, const int w, const int h) noexcept;
template void autogainUS_avx2<4095>(const uint8_t* VS_RESTRICT srcp, uint8_t* VS_RESTRICT dstp, ptrdiff_t stride, const int w, const int h) noexcept;
template void autogainUS_avx2<16383>(const uint8_t* VS_RESTRICT srcp, uint8_t* VS_RESTRICT dstp, ptrdiff_t stride, const int w, const int h) noexcept;
template void autogainUS_avx2<65535>(const uint8_t* VS_RESTRICT srcp, uint8_t* VS_RESTRICT dstp, ptrdiff_t stride, const int w, const int h) noexcept;
#endif