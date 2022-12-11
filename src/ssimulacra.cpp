#include "shared.h"

struct SSIMULACRAData final {
	VSNode* node;
	VSNode* node2;
	const VSVideoInfo* vi;
	bool simple;
	int feature;

	void (*fill)(jxl::CodecInOut& ref, jxl::CodecInOut& dist, const VSFrame* src1, const VSFrame* src2, int width, int height, const ptrdiff_t stride, const SSIMULACRAData* const VS_RESTRICT d, const VSAPI* vsapi) noexcept;
};

template <typename pixel_t, typename jxl_t>
void fill_image(jxl::CodecInOut& ref, jxl::CodecInOut& dist, const VSFrame* src1, const VSFrame* src2, int width, int height, const ptrdiff_t stride, const SSIMULACRAData* const VS_RESTRICT d, const VSAPI* vsapi) noexcept {
	jxl_t tmp1(width, height);
	jxl_t tmp2(width, height);

	for (int i{ 0 }; i < 3; i++) {
		auto srcp1{ reinterpret_cast<const pixel_t*>(vsapi->getReadPtr(src1, i)) };
		auto srcp2{ reinterpret_cast<const pixel_t*>(vsapi->getReadPtr(src2, i)) };

		for (int y{ 0 }; y < height; y++) {
			memcpy(tmp1.PlaneRow(i, y), srcp1, width * sizeof(pixel_t));
			memcpy(tmp2.PlaneRow(i, y), srcp2, width * sizeof(pixel_t));

			srcp1 += stride;
			srcp2 += stride;
		}
	}

	ref.SetFromImage(std::move(jxl::ConvertToFloat(tmp1)), jxl::ColorEncoding::SRGB(false));
	dist.SetFromImage(std::move(jxl::ConvertToFloat(tmp2)), jxl::ColorEncoding::SRGB(false));
}


void fill_imageF(jxl::CodecInOut& ref, jxl::CodecInOut& dist, const VSFrame* src1, const VSFrame* src2, int width, int height, const ptrdiff_t stride, const SSIMULACRAData* const VS_RESTRICT d, const VSAPI* vsapi) noexcept {
	jxl::Image3F tmp1(width, height);
	jxl::Image3F tmp2(width, height);

	for (int i{ 0 }; i < 3; i++) {
		const float* srcp1{ reinterpret_cast<const float*>(vsapi->getReadPtr(src1, i)) };
		const float* srcp2{ reinterpret_cast<const float*>(vsapi->getReadPtr(src2, i)) };

		for (int y = 0; y < height; y++) {
			memcpy(tmp1.PlaneRow(i, y), srcp1, width * sizeof(float));
			memcpy(tmp2.PlaneRow(i, y), srcp2, width * sizeof(float));

			srcp1 += stride;
			srcp2 += stride;
		}
	}

	ref.SetFromImage(std::move(tmp1), jxl::ColorEncoding::SRGB(false));
	dist.SetFromImage(std::move(tmp2), jxl::ColorEncoding::SRGB(false));
}

static const VSFrame* VS_CC ssimulacraGetFrame(int n, int activationReason, void* instanceData, void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi) {
	auto d{ static_cast<SSIMULACRAData*>(instanceData) };

	if (activationReason == arInitial) {
		vsapi->requestFrameFilter(n, d->node, frameCtx);
		vsapi->requestFrameFilter(n, d->node2, frameCtx);
	}
	else if (activationReason == arAllFramesReady) {
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

		d->fill(ref, dist, src, src2, width, height, stride, d, vsapi);

		VSFrame* dst = vsapi->copyFrame(src2, core);
		VSMap* dstProps = vsapi->getFramePropertiesRW(dst);

		if (!d->feature || d->feature == 2) {
			Msssim msssim{ ComputeSSIMULACRA2(ref.Main(), dist.Main()) };
			vsapi->mapSetFloat(dstProps, "_SSIMULACRA2", msssim.Score(), maReplace);
		}

		if (d->feature) {
			ref.TransformTo(jxl::ColorEncoding::LinearSRGB(false), jxl::GetJxlCms());
			dist.TransformTo(jxl::ColorEncoding::LinearSRGB(false), jxl::GetJxlCms());
			ssimulacra::Ssimulacra ssimulacra_{ ssimulacra::ComputeDiff(*ref.Main().color(), *dist.Main().color(), d->simple) };
			vsapi->mapSetFloat(dstProps, "_SSIMULACRA", ssimulacra_.Score(), maReplace);
		}

		vsapi->freeFrame(src);
		vsapi->freeFrame(src2);
		return dst;
	}
	return nullptr;

}

static void VS_CC ssimulacraFree(void* instanceData, VSCore* core, const VSAPI* vsapi) {
	auto d{ reinterpret_cast<SSIMULACRAData*>(instanceData) };

	vsapi->freeNode(d->node);
	vsapi->freeNode(d->node2);
	delete d;
}

void VS_CC ssimulacraCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi) {
	auto d{ std::make_unique<SSIMULACRAData>() };
	int err{ 0 };

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
		d->fill = fill_image<uint8_t, jxl::Image3B>;
		break;
	case 2:
		d->fill = fill_image<uint16_t, jxl::Image3U>;
		break;
	case 4:
		d->fill = fill_imageF;
		break;
	}

	VSFilterDependency deps[]{ {d->node, rpGeneral}, {d->node2, rpGeneral} };
	vsapi->createVideoFilter(out, "SSIMULACRA", d->vi, ssimulacraGetFrame, ssimulacraFree, fmParallel, deps, 2, d.get(), core);
	d.release();
}