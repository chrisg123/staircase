cmake_minimum_required(VERSION 3.10)

project(Staircase)

set(CMAKE_CXX_STANDARD 17)

option(DIST_BUILD "Build for distribution" OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -s USE_PTHREADS=1 -Wno-pthreads-mem-growth")

set(EMSDK_SYSROOT $ENV{EMSDK}/upstream/emscripten/cache/sysroot)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

include_directories(${EMSDK_SYSROOT}/include
                    ${EMSDK_SYSROOT}/include/opencascade)

link_directories(${EMSDK_SYSROOT}/lib)

set(SOURCE_FILES
  ${SRC_DIR}/main.cpp
  ${SRC_DIR}/GraphicsUtilities.cpp
  ${SRC_DIR}/OCCTUtilities.cpp
  ${SRC_DIR}/StaircaseViewController.cpp
  ${SRC_DIR}/StaircaseViewer.cpp
)

add_executable(staircase ${SOURCE_FILES})

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
    " --bind"
    " -sPTHREAD_POOL_SIZE=1"
    " -sSTACK_SIZE=1MB"
    " -sINITIAL_MEMORY=67108864"
    " -sALLOW_MEMORY_GROWTH=1"
    " -sNO_DISABLE_EXCEPTION_CATCHING"
    " --extern-post-js ${CMAKE_CURRENT_SOURCE_DIR}/web/staircase-module-post.js"
    " -sMODULARIZE"
    " -sEXPORT_NAME='createStaircaseModule'"
)

if(DIST_BUILD)
  add_definitions(-DDIST_BUILD)

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -gsource-map -O0")
  set(EMSCRIPTEN_FLAGS
    "${EMSCRIPTEN_FLAGS}"
    " -sSTACK_OVERFLOW_CHECK=1"
    " -sASSERTIONS"
  )
endif()


string(CONCAT FINAL_EMSCRIPTEN_FLAGS ${EMSCRIPTEN_FLAGS})

set_target_properties(staircase PROPERTIES LINK_FLAGS ${FINAL_EMSCRIPTEN_FLAGS})
