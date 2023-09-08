#!/bin/bash

set -e

verbose=0

for arg in "$@"; do
    if [ "$arg" == "--verbose" ] || [ "$arg" == "-v" ]; then
        verbose=1
    fi
done

if [ "$verbose" -eq 1 ]; then
    set -x
fi

script_dir="$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")"

export SKIP_EMSDK_MESSAGE=1
. "${script_dir}/env.sh" || {
    echo "Error: Failed to set up the environment using env.sh" >&2
    exit 1
}

build_dir="${script_dir}/build"

cmake_modules="upstream/emscripten/cmake/Modules"
toolchain_file="${EMSDK}/${cmake_modules}/Platform/Emscripten.cmake"

mkdir -p "${build_dir}/freetype"
mkdir -p "${build_dir}/occt"
mkdir -p "${build_dir}/step-viewer"

num_cores=$(nproc)

if [ ! -f ".skip_initial_dependency_build" ]; then
    pushd build/freetype
    cmake ../../external/freetype -DCMAKE_TOOLCHAIN_FILE="${toolchain_file}"
    make -j$num_cores all
    make install
    popd

    pushd build/occt
    cmake ../../external/occt \
        -DCMAKE_TOOLCHAIN_FILE="${toolchain_file}" \
        -DBUILD_LIBRARY_TYPE="Static" \
        -D3RDPARTY_FREETYPE_DIR="${EMSDK}/upstream/emscripten/cache/sysroot/include/freetype2" \
        -D3RDPARTY_FREETYPE_INCLUDE_DIR_ft2build="${EMSDK}/upstream/emscripten/cache/sysroot/include/freetype2" \
        -D3RDPARTY_FREETYPE_INCLUDE_DIR_freetype2="${EMSDK}/upstream/emscripten/cache/sysroot/include/freetype2" \
        -D3RDPARTY_FREETYPE_LIBRARY="${EMSDK}/upstream/emscripten/cache/sysroot/lib/libfreetype.a" \
        -D3RDPARTY_FREETYPE_LIBRARY_DIR="${EMSDK}/upstream/emscripten/cache/sysroot/lib" \
        -DUSE_FREETYPE=ON \
        -DUSE_TK=OFF \
        -DUSE_FFMPEG=OFF \
        -DUSE_FREEIMAGE=OFF \
        -DUSE_OPENVR=OFF \
        -DUSE_RAPIDJSON=OFF \
        -DBUILD_MODULE_Draw=OFF \
        -DBUILD_MODULE_Visualization=ON \
        -DBUILD_MODULE_ModelingData=ON \
        -DBUILD_MODULE_ModelingAlgorithms=ON \
        -DBUILD_MODULE_DataExchange=ON
    make -j$num_cores all

    # Create symlink to fix bug in occt's `make install`.
    target="${script_dir}/build/occt/lin32/clang/bin/ExpToCasExe.js-7.8.0.wasm"
    link="${script_dir}/build/occt/lin32/clang/bin/ExpToCasExe.wasm"

    if [ -L "$link" ]; then
        if [ ! -e "$link" ]; then
            echo "Broken symlink detected. Removing it."
            rm "$link"
        elif [ "$(readlink "$link")" = "$target" ]; then
            echo "Symlink already points to the correct target. Nothing to do."
        else
            echo "Symlink points to a different target. Updating it."
            rm "$link"
        fi
    fi

    ln -s "$target" "$link"

    make install
    popd

    touch .skip_initial_dependency_build
fi

pushd build/step-viewer
cmake ../.. -DCMAKE_TOOLCHAIN_FILE="${toolchain_file}"
make -j$num_cores all
popd
