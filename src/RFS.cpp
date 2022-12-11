#include "shared.h"

struct RFSData final {
	VSNode* node1;
	VSNode* node2;
	std::vector<bool> replace;
};

struct MismatchInfo {
	bool match;
	bool differentDimensions;
	bool differentFormat;
	bool differentFrameRate;
};

MismatchInfo findCommonVi(VSVideoInfo* outvi, VSNode* node2, const VSAPI* vsapi) {
	MismatchInfo result = {};
	const VSVideoInfo* vi{ vsapi->getVideoInfo(node2) };

	if (outvi->width != vi->width || outvi->height != vi->height) {
		outvi->width = 0;
		outvi->height = 0;
		result.differentDimensions = true;
	}

	if (!vsh::isSameVideoFormat(&outvi->format, &vi->format)) {
		outvi->format = {};
		result.differentFormat = true;
	}

	if (outvi->fpsNum != vi->fpsNum || outvi->fpsDen != vi->fpsDen) {
		outvi->fpsDen = 0;
		outvi->fpsNum = 0;
		result.differentFrameRate = true;
	}

	result.match = !result.differentDimensions && !result.differentFormat && !result.differentFrameRate;
	return result;
}

static const VSFrame* VS_CC rfsGetFrame(int n, int activationReason, void* instanceData, void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi) {
	auto d{ reinterpret_cast<RFSData*>(instanceData) };

	if (activationReason == arInitial) {
		vsapi->requestFrameFilter(n, (d->replace[n] ? d->node2 : d->node1), frameCtx);
	}
	else if (activationReason == arAllFramesReady) {
		return vsapi->getFrameFilter(n, (d->replace[n] ? d->node2 : d->node1), frameCtx);
	}
	return nullptr;
}

static void VS_CC rfsFree(void* instanceData, VSCore* core, const VSAPI* vsapi) {
	auto d{ reinterpret_cast<RFSData*>(instanceData) };

	vsapi->freeNode(d->node1);
	vsapi->freeNode(d->node2);
	delete d;
}

void VS_CC rfsCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi) {
	auto d{ std::make_unique<RFSData>() };
	int err{ 0 };

	d->node1 = vsapi->mapGetNode(in, "clip_a", 0, nullptr);
	d->node2 = vsapi->mapGetNode(in, "clip_b", 0, nullptr);

	VSVideoInfo vi = *vsapi->getVideoInfo(d->node1);
	MismatchInfo mminfo = findCommonVi(&vi, d->node2, vsapi);

	d->replace.resize(vi.numFrames);
	for (int i{ 0 }; i < vsapi->mapNumElements(in, "frames"); i++) {
		d->replace[vsapi->mapGetInt(in, "frames", i, 0)] = true;
	}

	bool mismatch;
	mismatch = !!vsapi->mapGetInt(in, "mismatch", 0, &err);
	if (err)
		mismatch = false;

	if (!mismatch && !mminfo.match) {
		if (mminfo.differentDimensions)
			vsapi->mapSetError(out, "RFS: Clip dimensions don't match, enable mismatch if you want variable format.");
		else if (mminfo.differentFormat)
			vsapi->mapSetError(out, "RFS: Clip formats don't match, enable mismatch if you want variable format.");
		else if (mminfo.differentFrameRate)
			vsapi->mapSetError(out, "RFS: Clip frame rates don't match, enable mismatch if you want variable format.");
		vsapi->freeNode(d->node1);
		vsapi->freeNode(d->node2);
		return;
	}

	VSFilterDependency deps[]{ {d->node1, rpGeneral}, {d->node2, rpGeneral} };
	vsapi->createVideoFilter(out, "RFS", &vi, rfsGetFrame, rfsFree, fmParallel, deps, 2, d.get(), core);
	d.release();
}