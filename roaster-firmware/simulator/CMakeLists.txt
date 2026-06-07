cmake_minimum_required(VERSION 3.20)

project(roaster_lvgl_simulator LANGUAGES C)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

set(LOCAL_LVGL_DIR "$ENV{HOME}/Documents/Arduino/libraries/lvgl")
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "Disable LVGL examples" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "Disable LVGL demos" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "Disable internal ThorVG" FORCE)

if(EXISTS "${LOCAL_LVGL_DIR}/CMakeLists.txt")
  add_subdirectory("${LOCAL_LVGL_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/lvgl")
else()
  FetchContent_Declare(
    lvgl
    GIT_REPOSITORY https://github.com/lvgl/lvgl.git
    GIT_TAG v9.2.2
  )
  FetchContent_MakeAvailable(lvgl)
endif()

set(LVGL_TARGET lvgl)
if(TARGET lvgl::lvgl)
  set(LVGL_TARGET lvgl::lvgl)
endif()

if(TARGET lvgl)
  target_compile_definitions(lvgl PUBLIC LV_CONF_PATH="${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h")
endif()

add_executable(roaster-lvgl-sim main.c)

target_compile_definitions(roaster-lvgl-sim PRIVATE
  SDL_MAIN_HANDLED
  LV_CONF_PATH="${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h"
)

target_link_libraries(roaster-lvgl-sim PRIVATE
  ${LVGL_TARGET}
  PkgConfig::SDL2
)
