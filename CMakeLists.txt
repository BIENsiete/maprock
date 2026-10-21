cmake_minimum_required(VERSION 3.16)
project(maprock LANGUAGES C CXX)

# stb has no CMakeLists.txt, so we populate it without add_subdirectory.
cmake_policy(SET CMP0169 OLD)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(MAPROCK_BUILD_GUI "Build the Dear ImGui + GLFW map-view GUI" ON)
option(MAPROCK_BUILD_TESTS "Build the synthetic-world generator and smoke test" ON)

include(FetchContent)

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
set(FETCHCONTENT_QUIET OFF)

# Mojang's LevelDB fork (zlib compression) as a CMake project. Provides the
# `leveldb-mcpe` and `zlibstatic` targets.
FetchContent_Declare(
  leveldb
  GIT_REPOSITORY https://github.com/teampathfinders/leveldb.git
  GIT_TAG master
)
FetchContent_MakeAvailable(leveldb)

# leveldb-mcpe compiles its SSE4.2 CRC path unconditionally but only passes
# -msse4.2 for the GNU compiler id. AppleClang/Clang needs the same flag (on
# x86_64) so <nmmintrin.h>/<cpuid.h> are pulled in.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(leveldb-mcpe PRIVATE -msse4.2)
endif()

# JSON for Modern C++
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# stb (single-file image loading/writing)
FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG master
  GIT_SHALLOW TRUE
)
FetchContent_GetProperties(stb)
if(NOT stb_POPULATED)
  FetchContent_Populate(stb)
endif()
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})

if(MAPROCK_BUILD_GUI)
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
  # Prefer X11 on Linux (wayland-scanner is not present on bare runners).
  set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
  FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
  )
  FetchContent_MakeAvailable(glfw)

  FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.9
  )
  FetchContent_MakeAvailable(imgui)

  add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
  )
  target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
  target_link_libraries(imgui PUBLIC glfw)
endif()

# ---------------------------------------------------------------------------
# Core library
# ---------------------------------------------------------------------------
add_library(maprock_core STATIC
  src/nbt.cpp
  src/world.cpp
  src/block_registry.cpp
  src/texture_atlas.cpp
  src/mesher.cpp
  src/map_image.cpp
  src/obj_exporter.cpp
  src/gltf_exporter.cpp
  src/export_pipeline.cpp
  src/zip.cpp
)
target_include_directories(maprock_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(maprock_core PUBLIC
  leveldb-mcpe
  zlibstatic
  nlohmann_json::nlohmann_json
  stb
)
if(MSVC)
  target_compile_options(maprock_core PRIVATE /W3)
else()
  target_compile_options(maprock_core PRIVATE -Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
# Executables
# ---------------------------------------------------------------------------
add_executable(maprock src/main.cpp)
target_link_libraries(maprock PRIVATE maprock_core)
if(MAPROCK_BUILD_GUI)
  target_sources(maprock PRIVATE src/gui/gui.cpp)
  target_link_libraries(maprock PRIVATE imgui)
  target_compile_definitions(maprock PRIVATE MAPROCK_GUI=1)
  if(WIN32)
    target_link_libraries(maprock PRIVATE opengl32 ole32 uuid)
  elseif(APPLE)
    target_link_libraries(maprock PRIVATE "-framework OpenGL")
  else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(maprock PRIVATE OpenGL::GL)
  endif()

  # GUI-only executable: no console window on Windows, so double-clicking it
  # opens the map window directly.
  add_executable(maprock-gui src/gui/gui_main.cpp src/gui/gui.cpp)
  target_link_libraries(maprock-gui PRIVATE maprock_core imgui)
  target_compile_definitions(maprock-gui PRIVATE MAPROCK_GUI=1)
  if(WIN32)
    target_link_libraries(maprock-gui PRIVATE opengl32 ole32 uuid)
    set_target_properties(maprock-gui PROPERTIES WIN32_EXECUTABLE TRUE)
  elseif(APPLE)
    target_link_libraries(maprock-gui PRIVATE "-framework OpenGL")
  else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(maprock-gui PRIVATE OpenGL::GL)
  endif()
endif()

if(MAPROCK_BUILD_TESTS)
  add_executable(make_test_world tools/make_test_world.cpp)
  target_link_libraries(make_test_world PRIVATE maprock_core)

  enable_testing()
  add_test(
    NAME smoke_export
    COMMAND ${CMAKE_COMMAND}
      -DMAPROCK_EXE=$<TARGET_FILE:maprock>
      -DMAKEWORLD_EXE=$<TARGET_FILE:make_test_world>
      -DTEST_DIR=${CMAKE_BINARY_DIR}/smoke
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/smoke.cmake
  )
endif()
