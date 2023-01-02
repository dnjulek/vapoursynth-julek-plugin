#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>
#include <VapourSynth4.h>
#include <VSHelper4.h>

#include "config.h"

#include "lib/extras/codec.h"
#include "lib/jxl/color_management.h"
#include "lib/jxl/enc_color_management.h"
#include "lib/jxl/enc_butteraugli_comparator.h"
#include "tools/ssimulacra.h"
#include "tools/ssimulacra2.h"

#ifdef PLUGIN_X86
#include "vectorclass.h"
#include "vectormath_exp.h"
#endif

extern void VS_CC agmCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi);
extern void VS_CC butteraugliCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi);
extern void VS_CC rfsCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi);
extern void VS_CC ssimulacraCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi);

#ifdef _MSC_VER
#define FORCE_INLINE inline __forceinline
#else
#define FORCE_INLINE inline __attribute__((always_inline))
#endif

struct AGMData final {
	VSNode* node;
	const VSVideoInfo* vi;
	float luma_scaling;
	float float_range[256];
	int shift, peak;
	void (*process)(const VSFrame* src, VSFrame* dst, float& avg, const AGMData* const VS_RESTRICT d, const VSAPI* vsapi) noexcept;
};