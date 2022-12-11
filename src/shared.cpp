#include "shared.h"


VS_EXTERNAL_API(void) VapourSynthPluginInit2(VSPlugin* plugin, const VSPLUGINAPI* vspapi) {
	vspapi->configPlugin("com.julek.plugin", "julek", "Julek filters", 1, VAPOURSYNTH_API_VERSION, 0, plugin);
	vspapi->registerFunction("Butteraugli", "reference:vnode;distorted:vnode;distmap:int:opt;intensity_target:float:opt;linput:int:opt;", "clip:vnode;", butteraugliCreate, nullptr, plugin);
	vspapi->registerFunction("RFS", "clip_a:vnode;clip_b:vnode;frames:int[];mismatch:int:opt;", "clip:vnode;", rfsCreate, nullptr, plugin);
	vspapi->registerFunction("SSIMULACRA", "reference:vnode;distorted:vnode;feature:int:opt;simple:int:opt;", "clip:vnode;", ssimulacraCreate, nullptr, plugin);
}