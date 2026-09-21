cmake_minimum_required(VERSION 3.16)
project(DevBoxLinux LANGUAGES C CXX)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
find_path(SDL2_IMAGE_INCLUDE_DIR SDL_image.h PATH_SUFFIXES SDL2)
find_library(SDL2_IMAGE_LIBRARY SDL2_image)
if(NOT SDL2_IMAGE_INCLUDE_DIR OR NOT SDL2_IMAGE_LIBRARY)
  message(FATAL_ERROR "SDL2_image development files are required for the PNG background")
endif()

configure_file(
  src/backends/linux/DevBoxPlayerBackground.png
  DevBoxPlayerBackground.png COPYONLY)
configure_file(
  src/backends/linux/InputConfiguration.png
  InputConfiguration.png COPYONLY)

find_path(U8G2_CLIB_DIR u8g2.h
  HINTS "${CMAKE_CURRENT_SOURCE_DIR}/../U8g2/src/clib"
  DOC "Path to the U8g2 C source directory (src/clib)")
if(NOT U8G2_CLIB_DIR)
  message(FATAL_ERROR "U8g2 C sources not found; set U8G2_CLIB_DIR")
endif()
file(GLOB U8G2_SOURCES CONFIGURE_DEPENDS "${U8G2_CLIB_DIR}/*.c")
add_library(devbox_u8g2 STATIC ${U8G2_SOURCES})
target_include_directories(devbox_u8g2 PUBLIC "${U8G2_CLIB_DIR}")

add_executable(devbox_linux_gradient
  examples/linux_display/main.cpp
  src/backends/linux/display.cpp
  src/backends/linux/input.cpp
  src/backends/linux/input_config.cpp
  src/backends/linux/audio.cpp
  src/backends/linux/queue.cpp
  src/backends/linux/task.cpp
  src/backends/linux/system.cpp
  src/backends/linux/debug.cpp
)
target_compile_features(devbox_linux_gradient PRIVATE cxx_std_17)
target_compile_definitions(devbox_linux_gradient PRIVATE DEVBOX_TARGET=DEVBOX_TARGET_LINUX)
target_compile_definitions(devbox_linux_gradient PRIVATE DEVBOX_GAME_ID="devbox_linux_gradient")
target_include_directories(devbox_linux_gradient PRIVATE src)
target_include_directories(devbox_linux_gradient PRIVATE "${SDL2_IMAGE_INCLUDE_DIR}")
target_link_libraries(devbox_linux_gradient PRIVATE PkgConfig::SDL2 "${SDL2_IMAGE_LIBRARY}" devbox_u8g2 Threads::Threads)

add_executable(devbox_linux_audio_demo
  examples/linux_audio/main.cpp
  src/backends/linux/display.cpp
  src/backends/linux/input.cpp
  src/backends/linux/input_config.cpp
  src/backends/linux/audio.cpp
  src/backends/linux/queue.cpp
  src/backends/linux/task.cpp
  src/backends/linux/system.cpp
  src/backends/linux/debug.cpp
)
target_compile_features(devbox_linux_audio_demo PRIVATE cxx_std_17)
target_compile_definitions(devbox_linux_audio_demo PRIVATE DEVBOX_TARGET=DEVBOX_TARGET_LINUX)
target_compile_definitions(devbox_linux_audio_demo PRIVATE DEVBOX_GAME_ID="devbox_linux_audio_demo")
target_include_directories(devbox_linux_audio_demo PRIVATE src "${SDL2_IMAGE_INCLUDE_DIR}")
target_link_libraries(devbox_linux_audio_demo PRIVATE PkgConfig::SDL2 "${SDL2_IMAGE_LIBRARY}" devbox_u8g2 Threads::Threads)

add_executable(devbox_linux_sd_demo
  examples/linux_sd/main.cpp
  src/backends/linux/sd.cpp
)
target_compile_features(devbox_linux_sd_demo PRIVATE cxx_std_17)
target_compile_definitions(devbox_linux_sd_demo PRIVATE DEVBOX_TARGET=DEVBOX_TARGET_LINUX)
target_include_directories(devbox_linux_sd_demo PRIVATE src)
