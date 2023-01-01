vapoursynth-julek-plugin is a collection of some already known filters and some new ones that I am implementing here to have more performance.\
Please visit the [wiki](https://github.com/dnjulek/vapoursynth-julek-plugin/wiki) for more details.

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
    - CMake >= 3.20.0
```
### Linux:
```
git clone --recurse-submodules https://github.com/dnjulek/vapoursynth-julek-plugin

cd vapoursynth-julek-plugin/thirdparty/libjxl

cmake -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -B build -DCMAKE_INSTALL_PREFIX=jxl_install -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DJPEGXL_ENABLE_FUZZERS=OFF -DJPEGXL_ENABLE_TOOLS=OFF -DJPEGXL_ENABLE_MANPAGES=OFF -DJPEGXL_ENABLE_BENCHMARK=OFF -DJPEGXL_ENABLE_EXAMPLES=OFF -DJPEGXL_ENABLE_JNI=OFF -DJPEGXL_ENABLE_OPENEXR=OFF -DJPEGXL_ENABLE_TCMALLOC=OFF -DBUILD_SHARED_LIBS=OFF -DJPEGXL_ENABLE_SJPEG=OFF -G Ninja

cmake --build build

cmake --install build

cd ../..

cmake -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -B build -DCMAKE_BUILD_TYPE=Release -G Ninja

cmake --build build

cmake --install build
```

I recommend compiling with clang or you may have problems with libjxl, if you want to try compiling with gcc you may need to add this to the second cmake command:\
``-DCMAKE_C_FLAGS=fPIC -DCMAKE_CXX_FLAGS=-fPIC``
### Windows:
Open the ``Visual Studio 2022 Developer PowerShell`` and use cd to the folder you downloaded the repository.
```
cd vapoursynth-julek-plugin/thirdparty/libjxl

cmake -B build -DCMAKE_INSTALL_PREFIX=jxl_install -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DJPEGXL_ENABLE_FUZZERS=OFF -DJPEGXL_ENABLE_TOOLS=OFF -DJPEGXL_ENABLE_MANPAGES=OFF -DJPEGXL_ENABLE_BENCHMARK=OFF -DJPEGXL_ENABLE_EXAMPLES=OFF -DJPEGXL_ENABLE_JNI=OFF -DJPEGXL_ENABLE_OPENEXR=OFF -DJPEGXL_ENABLE_TCMALLOC=OFF -DBUILD_SHARED_LIBS=OFF -DJPEGXL_ENABLE_SJPEG=OFF -G Ninja

cmake --build build

cmake --install build

cd ../..

cmake -B build -DCMAKE_BUILD_TYPE=Release -DVS_INCLUDE_DIR="C:/Program Files/VapourSynth/sdk/include/vapoursynth" -G Ninja

cmake --build build
```
