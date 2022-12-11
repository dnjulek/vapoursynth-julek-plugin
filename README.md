- Butteraugli ([wiki](https://github.com/dnjulek/vapoursynth-julek-plugin/wiki/Butteraugli))
```python
core.julek.Butteraugli(vnode reference, vnode distorted [, bool distmap=False, float intensity_target=80.0, bool linput=False])
```
- RFS ([wiki](https://github.com/dnjulek/vapoursynth-julek-plugin/wiki/RFS))
```python
core.julek.RFS(vnode clip_a, vnode clip_b, int[] frames [, bool mismatch=False])
```
- SSIMULACRA ([wiki](https://github.com/dnjulek/vapoursynth-julek-plugin/wiki/SSIMULACRA))
```python
core.julek.SSIMULACRA(vnode reference, vnode distorted [, int feature=0, bool simple=False])
```

### Building:

```
Requirements:
    - Git
    - C++17 compiler
    - CMake >= 3.16
```
```
git clone --recurse-submodules https://github.com/dnjulek/vapoursynth-julek-plugin

cd thirdparty/libjxl

cmake -B build -DCMAKE_INSTALL_PREFIX=jxl_install -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DJPEGXL_ENABLE_FUZZERS=OFF -DJPEGXL_ENABLE_TOOLS=OFF -DJPEGXL_ENABLE_MANPAGES=OFF -DJPEGXL_ENABLE_BENCHMARK=OFF -DJPEGXL_ENABLE_EXAMPLES=OFF -DJPEGXL_ENABLE_JNI=OFF -DJPEGXL_ENABLE_OPENEXR=OFF -DJPEGXL_ENABLE_TCMALLOC=OFF -DBUILD_SHARED_LIBS=OFF -DJPEGXL_ENABLE_SJPEG=OFF -G Ninja

cmake --build build

cmake --install build

cd ../..

cmake -B build -DCMAKE_BUILD_TYPE=Release -G Ninja

cmake --build build
```