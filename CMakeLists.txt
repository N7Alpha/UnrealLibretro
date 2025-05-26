# This builds netarch as a fully standalone executable and doesn't depend on UBT

cmake_minimum_required(VERSION 3.13)
project(sdlarch)

# Set C++ Standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SDL_STATIC ON)

# Enable include-what-you-use if it exists
# It was hitting an assertin the code I cared about checking
#find_program(IWYU_PATH NAMES include-what-you-use iwyu)
#if(IWYU_PATH)
#  set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE
#    ${IWYU_PATH}
#    -Xiwyu --no_fwd_decls
#  )
#endif()

option(NETPLAYARCH_ASAN "Enable AddressSanitizer (ASan)" OFF)

if(NETPLAYARCH_ASAN)
  if(CMAKE_C_COMPILER_ID MATCHES "AppleClang|GNU|Clang")
    # Set compiler and linker flags for ASan
    add_compile_options(-fno-omit-frame-pointer -fsanitize=address)
    link_libraries(-fsanitize=address)
  elseif(MSVC)
    add_compile_options(/fsanitize=address)
  else()
    message(SEND_ERROR "AddressSanitizer support requires clang, gcc, or msvc. Try again with -DCMAKE_C_COMPILER.")
  endif()
endif()

if(MSVC)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)
  # Disable warnings about deprecated POSIX functions + warnings relevant to C++ but not C
  add_definitions("/wd4244 /wd4267 /wd4200 /wd26495")
else()
  add_compile_options(-Wsizeof-pointer-div)
endif()


# Source Files
set(SOURCE_FILES
    netarch.cpp
    Source/UnrealLibretro/Private/fec.c
    Source/ThirdParty/imgui/imgui.cpp
    Source/ThirdParty/imgui/backends/imgui_impl_sdl3.cpp
    Source/ThirdParty/imgui/backends/imgui_impl_opengl3.cpp
    Source/ThirdParty/imgui/imgui_draw.cpp
    Source/ThirdParty/imgui/imgui_demo.cpp
    Source/ThirdParty/imgui/imgui_tables.cpp
    Source/ThirdParty/imgui/imgui_widgets.cpp
    Source/ThirdParty/implot/implot.cpp
    Source/ThirdParty/implot/implot_items.cpp
    Source/ThirdParty/netImgui/Code/Client/Private/NetImgui_Api.cpp
    Source/ThirdParty/netImgui/Code/Client/Private/NetImgui_Client.cpp
    Source/ThirdParty/netImgui/Code/Client/Private/NetImgui_CmdPackets_DrawFrame.cpp
    Source/ThirdParty/netImgui/Code/Client/Private/NetImgui_NetworkPosix.cpp
    Source/ThirdParty/netImgui/Code/Client/Private/NetImgui_NetworkUE4.cpp
    Source/ThirdParty/netImgui/Code/Client/Private/NetImgui_NetworkWin32.cpp
)

# Add SDL and libuv subdirectories
# The way I set options with set() looks janky. This was the only thing I could figure out that worked after fiddling with cmake for a half hour

# 2. Tell CMake you DO NOT want to build SDL3's shared library
set(SDL_SHARED OFF CACHE BOOL "Disable SDL3 shared library build" FORCE)
set(SDL_TESTS OFF CACHE BOOL "Disable building SDL3 tests" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "Disable building SDL3 examples" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "Disable SDL3_test library artifact" FORCE)
set(SDL_UNIX_CONSOLE_BUILD ON CACHE BOOL "")
add_subdirectory(Source/ThirdParty/SDL)
add_subdirectory(Source/ThirdParty/libuv)
set(NO_TESTS ON CACHE BOOL "") # libjuice defaults to building tests (which we don't have checked in)
add_subdirectory(Source/ThirdParty/libjuice)
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "")
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "")
set(ZSTD_LEGACY_SUPPORT OFF CACHE BOOL "")
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "")
add_subdirectory(Source/ThirdParty/zstd/build/cmake)

# Target
add_executable(${PROJECT_NAME} ${SOURCE_FILES})

# This should come after add_executable
target_include_directories(${PROJECT_NAME} PRIVATE
    Source/UnrealLibretro/Private
    Source/UnrealLibretro/Public/libretro
    Source/ThirdParty/libjuice/include
    Source/ThirdParty/zstd/lib
    Source/ThirdParty/imgui
    Source/ThirdParty/imgui/backends
    Source/ThirdParty/implot
    Source/ThirdParty/SDL/include
    Source/ThirdParty/netImgui/Code/Client
)

# Static link everything so we don't have to deal with dll hell
target_link_libraries(${PROJECT_NAME} uv_a juice-static libzstd_static SDL3::SDL3-static)
