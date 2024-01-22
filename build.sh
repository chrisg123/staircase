#!/bin/bash

set -e
ver=0.1.0
verbose=0
dist=1

for arg in "$@"; do
    if [ "${arg}" == "--verbose" ] || [ "${arg}" == "-v" ]; then
        verbose=1
    fi
    if [ "${arg}" == "--dist" ] || [ "${arg}" == "-d" ]; then
        dist=1
    fi
done

if [ "${verbose}" -eq 1 ]; then
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
mkdir -p "${build_dir}/staircase"

num_cores=$(nproc)

git submodule update --init --recursive

if [ ! -f ".skip_initial_dependency_build" ]; then

    pushd build/freetype
    cmake ../../external/freetype -DCMAKE_TOOLCHAIN_FILE="${toolchain_file}" \
        -DCMAKE_CXX_FLAGS="-s USE_PTHREADS=1" \
        -DCMAKE_C_FLAGS="-s USE_PTHREADS=1"
    make -j"${num_cores}" all
    make install
    popd

    pushd build/occt
    cmake ../../external/occt \
        -DCMAKE_TOOLCHAIN_FILE="${toolchain_file}" \
        -DCMAKE_CXX_FLAGS="-s USE_PTHREADS=1" \
        -DCMAKE_C_FLAGS="-s USE_PTHREADS=1" \
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
    make -j"${num_cores}" all

    # Create symlink to fix bug in occt's `make install`.
    target="${script_dir}/build/occt/lin32/clang/bin/ExpToCasExe.js-7.8.0.wasm"
    link="${script_dir}/build/occt/lin32/clang/bin/ExpToCasExe.wasm"

    if [ -L "${link}" ]; then
        if [ ! -e "${link}" ]; then
            echo "Broken symlink detected. Removing it."
            rm "${link}"
        elif [ "$(readlink "${link}")" = "${target}" ]; then
            echo "Symlink already points to the correct target. Nothing to do."
        else
            echo "Symlink points to a different target. Updating it."
            rm "${link}"
        fi
    fi

    ln -s "${target}" "${link}"

    make install
    popd

    # Create a flag file to skip initial dependency build in subsequent runs of
    # build.sh. Remove this file to prompt a rebuild.
    touch .skip_initial_dependency_build
fi

# Check for sample STEP files and download if not present
samples_dir="${script_dir}/samples"
target_definition="NIST_MBE_PMI_FTC_Definitions"
samples_zip="${samples_dir}/${target_definition}.zip"
target_step_file="${samples_dir}/${target_definition}/nist_ftc_09_asme1_rd.stp"
nist_url_prefix="https://www.nist.gov/system/files/documents/noindex/2022/07/19"

if [ ! -d "${samples_dir}" ]; then
    mkdir -p "${samples_dir}"
fi

if [ ! -f "${target_step_file}" ]; then
    if [ ! -f "${samples_zip}" ]; then
        echo "Downloading sample STEP files..."
        if curl -o "${samples_zip}" "${nist_url_prefix}/${target_definition}.zip"; then
            if unzip "${samples_zip}" -d "${samples_dir}"; then
                rm -f "${samples_zip}"
            fi
        fi
    fi
fi

resource_header="${script_dir}/src/EmbeddedStepFile.hpp"

step_content="nullptr"

if [ ! -f "${target_step_file}" ]; then
    echo "Sample STEP files missing!"
else
    step_content=$(sed 's/"/\\"/g' "${target_step_file}")
fi

# Generate the header file
echo "// Auto-generated file, do not edit" >"${resource_header}"
{
    echo "#ifndef EMBEDDED_STEP_FILE_H"
    echo "#define EMBEDDED_STEP_FILE_H"
    echo "#include <string>"
} >>"${resource_header}"

if [ "${step_content}" = "nullptr" ]; then
    echo "const std::string embeddedStepFile = nullptr;" >>"${resource_header}"
else
    {
        printf "const std::string embeddedStepFile = R\"("
        cat "${target_step_file}"
        echo ")\";"
    } >>"${resource_header}"
fi

echo "#endif // EMBEDDED_STEP_FILE_H" >>"${resource_header}"

pushd build/staircase
cmake ../.. -DCMAKE_TOOLCHAIN_FILE="${toolchain_file}"
make -j"${num_cores}" all
popd

echo "Generating dummy favicon.ico"
# Generate favicon.ico to avoid 404 File not found errors in browser
favicon="${build_dir}/staircase/favicon.ico"
if [ ! -f "$favicon" ]; then
    printf '\x00\x00\x01\x00\x01\x00' >"${favicon}"
    printf '\x10\x10\x01\x00\x20\x00\x00\x00\x00\x04\x00\x00\x16\x00\x00\x00' >>"${favicon}"
    for _ in $(seq 1 256); do
        printf '\xFF\xFF\xFF\xFF' >>"${favicon}"
    done
fi

if [ ! -d "${script_dir}/src" ]; then
    ln -s "${build_dir}/staircase/src" "${script_dir}/src"
fi

html_file="${script_dir}/web/index.html"

cp "${html_file}" "${build_dir}/staircase/index.html"

if [ "$dist" -eq 1 ]; then
    echo "Creating distribution package..."
    dist_dir="${build_dir}/dist"
    rm -rf "$dist_dir"
    mkdir -p "${dist_dir}"

    # Create a temporary directory for packaging
    stage_dir="${build_dir}/temp_package"
    rm -rf "$stage_dir"
    mkdir -p "${stage_dir}/staircase"

    cp "${build_dir}/staircase/staircase.js" "${stage_dir}/staircase/"
    cp "${build_dir}/staircase/staircase.wasm" "${stage_dir}/staircase/"
    cp "${build_dir}/staircase/staircase.worker.js" "${stage_dir}/staircase/"

    tar -cvf "${dist_dir}/staircase-${ver}.tar" -C "${stage_dir}" "staircase"
    pushd "${stage_dir}"
    zip -r "${dist_dir}/staircase-${ver}.zip" "staircase"
    popd

    rm -rf "$stage_dir"

    echo "Distribution package created in ${dist_dir}"
fi
