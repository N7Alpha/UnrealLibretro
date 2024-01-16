cmake_minimum_required(VERSION 3.13)
project(sdlarch)

# Set C++ Standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)
  # Disable warnings about deprecated POSIX functions + warnings relevant to C++ but not C
  add_definitions("/wd4244 /wd4267 /wd4200 /wd26495")
else()
  add_compile_options(-Wsizeof-pointer-div)
endif()

# Set Compiler Flags
if(MSVC)
else()
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set_source_files_properties(sdlarch.cpp PROPERTIES COMPILE_OPTIONS "-fsanitize=safe-stack")
  endif()

  if(APPLE)
      link_libraries(-static-libstdc++)
  else()
      link_libraries(-static-libgcc)
  endif()
endif()

# Enable hardware safe stack for x86_64 and software for aarch64 clang (this won't work if compiled in a dll)
if(CMAKE_SIZEOF_VOID_P EQUAL 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "amd64|x86_64")
  # Apply flags for MSVC
  if(MSVC)
    set_source_files_properties(sdlarch.cpp PROPERTIES COMPILE_OPTIONS "/CETCOMPAT")
  # Apply flags for GCC/Clang
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(sdlarch.cpp PROPERTIES COMPILE_OPTIONS "-mshstk")
  endif()
elseif()
  set_source_files_properties(sdlarch.cpp PROPERTIES COMPILE_OPTIONS "-fsanitize=safe-stack")
endif()

# Include Directories
include_directories(libjuice/include zstd/lib imgui/ imgui/backends/)

# Source Files
set(SOURCE_FILES 
    sdlarch.cpp
    fec.cpp
    rs.cpp
    glad.c
    imgui/imgui.cpp
    imgui/backends/imgui_impl_sdl2.cpp
    imgui/backends/imgui_impl_opengl3.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp)

# Add SDL and libuv subdirectories
set(SDL_SHARED_ENABLED_BY_DEFAULT OFF)
add_subdirectory(SDL)
add_subdirectory(libuv)
add_subdirectory(libjuice)
add_subdirectory(zstd/build/cmake)

# Target
add_executable(${PROJECT_NAME} ${SOURCE_FILES})

# Static link everything so we don't have to deal with dll hell
target_link_libraries(${PROJECT_NAME} SDL2-static SDL2main uv_a juice-static libzstd_static)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} ws2_32)
elseif(UNIX)
    target_link_libraries(${PROJECT_NAME} socket)
endif()
