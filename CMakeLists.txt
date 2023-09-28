cmake_minimum_required(VERSION 3.10)

project(StepViewer)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -s USE_PTHREADS=1")

set(EMSDK_SYSROOT $ENV{EMSDK}/upstream/emscripten/cache/sysroot)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

include_directories(${EMSDK_SYSROOT}/include
                    ${EMSDK_SYSROOT}/include/opencascade)

link_directories(${EMSDK_SYSROOT}/lib)

add_executable(StepViewer ${SRC_DIR}/main.cpp)

target_link_libraries(
  StepViewer
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
  TKOpenGles
)

set_target_properties(StepViewer PROPERTIES LINK_FLAGS "-s PTHREAD_POOL_SIZE=4")
