cmake_minimum_required(VERSION 3.13)
project(sdlarch)

# Set C++ Standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

# Set Compiler Flags
if(MSVC)
else()
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
  # This was causing the aarch64 build of clang-13 to segmentation fault
  #set_source_files_properties(sdlarch.cpp PROPERTIES COMPILE_OPTIONS "-fsanitize=safe-stack")
endif()


# Source Files
set(SOURCE_FILES 
    sdlarch.cpp
    fec.cpp
    glad.c
    imgui/imgui.cpp
    imgui/backends/imgui_impl_sdl2.cpp
    imgui/backends/imgui_impl_opengl3.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    implot/implot.cpp
    implot/implot_items.cpp
)

# Add SDL and libuv subdirectories
set(SDL_SHARED_ENABLED_BY_DEFAULT OFF CACHE BOOL "") # I feel like this shouldn't have to be so janky
add_subdirectory(SDL)
add_subdirectory(libuv)
add_subdirectory(libjuice)
add_subdirectory(zstd/build/cmake)

# Target
add_executable(${PROJECT_NAME} ${SOURCE_FILES})

# This should come after add_executable
target_include_directories(${PROJECT_NAME} PRIVATE
    libjuice/include
    zstd/lib
    imgui/
    imgui/backends/
    implot/
    SDL/include
)

# Static link everything so we don't have to deal with dll hell
target_link_libraries(${PROJECT_NAME} SDL2-static SDL2main uv_a juice-static libzstd_static)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} ws2_32)
endif()
