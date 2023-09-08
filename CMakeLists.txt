cmake_minimum_required(VERSION 3.10)

project(StepViewer)

set(EMSDK_SYSROOT $ENV{EMSDK}/upstream/emscripten/cache/sysroot)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

include_directories(
  ${EMSDK_SYSROOT}/include
  ${EMSDK_SYSROOT}/include/opencascade
)

link_directories(${EMSDK_SYSROOT}/lib)

add_executable(StepViewer ${SRC_DIR}/main.cpp)

target_link_libraries(StepViewer TKMath)

