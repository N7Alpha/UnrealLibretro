cmake_minimum_required(VERSION 3.13)
project(sdlarch)

# Set C++ Standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

## Add an option for enabling ASan.
#option(ASAN "Enable AddressSanitizer (ASan)" ON)

if(ASAN)
  if(CMAKE_C_COMPILER_ID MATCHES "AppleClang|GNU|Clang")
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-omit-frame-pointer -fsanitize=address")
    set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fno-omit-frame-pointer -fsanitize=address")
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fno-omit-frame-pointer -fsanitize=address")
  elseif(MSVC)
    add_compile_options(/fsanitize=address)
  else()
    message(SEND_ERROR "AddressSanitizer support requires clang, gcc, or msvc. Try again with -DCMAKE_C_COMPILER.")
  endif()
endif()

# Set Compiler Flags
if(MSVC)
#  if(ASAN)
#    set(SDL_ASAN ON)
#    add_compile_options(/fsanitize=address /MD)
#    add_link_options(/fsanitize=address)
#  endif()
else()
  # Only add the -fsanitize=address flags if ASan is enabled.
#  if(ASAN)
#    set(SDL_ASAN ON)
#    add_compile_options(-fsanitize=address -fpermissive -Wall -O1 -g3)
#    link_libraries(-fsanitize=address)
#  else()
#    add_compile_options(-fpermissive -Wall -O1 -g3)
#  endif()

  if(APPLE)
      link_libraries(-static-libstdc++)
  else()
      link_libraries(-static-libgcc)
  endif()
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