cmake_minimum_required(VERSION 3.10)

project(StepViewer)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -pthread -s USE_PTHREADS=1")

set(EMSDK_SYSROOT $ENV{EMSDK}/upstream/emscripten/cache/sysroot)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

include_directories(${EMSDK_SYSROOT}/include
                    ${EMSDK_SYSROOT}/include/opencascade)

link_directories(${EMSDK_SYSROOT}/lib)

add_executable(staircase ${SRC_DIR}/main.cpp)

target_link_libraries(
  staircase
  freetype
  TKRWMesh
  TKXCAF
  TKVCAF
  TKCAF
  TKV3d
  TKHLR
  TKMesh
  TKService
  TKMath
  TKernel
  TKG3d
  TKG2d
  TKShHealing
  TKGeomBase
  TKGeomAlgo
  TKBRep
  TKTopAlgo
  TKLCAF
  TKCDF
  TKXSBase
  TKSTEPBase
  TKSTEP
  TKSTEP209
  TKSTEPAttr
  TKXDESTEP
  TKOpenGles)

set(EMSCRIPTEN_FLAGS
    " -s PTHREAD_POOL_SIZE=4"
    " -s INITIAL_MEMORY=33554432"
    " -s NO_DISABLE_EXCEPTION_CATCHING"
    " -s ASSERTIONS"

    # NOTE: The following three flags seem incompatible with PTHREAD_POOL_SIZE.
    # An issue should be submitted to the main Emscripten project.
    # " --extern-post-js ${CMAKE_CURRENT_SOURCE_DIR}/web/staircase-module.js"
    # " -s EXPORT_NAME='createStaircaseModule'"
    # " -s MODULARIZE=1"
)

string(CONCAT FINAL_EMSCRIPTEN_FLAGS ${EMSCRIPTEN_FLAGS})

set_target_properties(staircase PROPERTIES LINK_FLAGS ${FINAL_EMSCRIPTEN_FLAGS})
