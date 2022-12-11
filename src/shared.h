#pragma once

#include <memory>
#include <vector>
#include <VapourSynth4.h>
#include <VSHelper4.h>

#include "lib/extras/codec.h"
#include "lib/jxl/color_management.h"
#include "lib/jxl/enc_color_management.h"
#include "lib/jxl/enc_butteraugli_comparator.h"
#include "tools/ssimulacra.h"
#include "tools/ssimulacra2.h"


extern void VS_CC butteraugliCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi);
extern void VS_CC rfsCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi);
extern void VS_CC ssimulacraCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi);