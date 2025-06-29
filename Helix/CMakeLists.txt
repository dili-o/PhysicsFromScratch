cmake_minimum_required(VERSION 3.16)
project(Helix)

# Visual Studio 
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

file(GLOB_RECURSE SOURCE_LIST "Src/*.cpp" "Src/*.hpp")

file(GLOB IMGUI_SOURCE_LIST 
    "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/imgui/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/imgui/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/imgui/backends/imgui_impl_sdl3.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/imgui/backends/imgui_impl_sdl3.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/imgui/backends/imgui_impl_vulkan.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/imgui/backends/imgui_impl_vulkan.cpp"
)

option(HELIX_WITH_TRACY "Enable Tracy profiler integration" ON)

set(EXTRA_SOURCE_LIST "")

if(HELIX_WITH_TRACY)
    list(APPEND EXTRA_SOURCE_LIST "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/tracy/public/TracyClient.cpp")
endif()

option(HELIX_SHARED "Build Helix as shared library" OFF)

if(HELIX_SHARED)
    set(HELIX_LIB_TYPE SHARED)
else()
    set(HELIX_LIB_TYPE STATIC)
endif()

# Build as a Static Lib
add_library(${PROJECT_NAME} ${HELIX_LIB_TYPE}
    ${SOURCE_LIST}
    ${IMGUI_SOURCE_LIST}
    "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/stb_image.h" 
    ${EXTRA_SOURCE_LIST}
)

if(HELIX_SHARED)
  target_compile_definitions(${PROJECT_NAME} PRIVATE HELIX_EXPORT)
  target_compile_definitions(${PROJECT_NAME} PRIVATE HELIX_SHARED)
endif()

if(WIN32)
  target_compile_definitions(${PROJECT_NAME} PUBLIC VK_USE_PLATFORM_WIN32_KHR)
  set(VOLK_STATIC_DEFINES "VK_USE_PLATFORM_WIN32_KHR")
endif()

target_include_directories(${PROJECT_NAME} PUBLIC "Src/")
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Find Vulkan SDK
find_package(Vulkan REQUIRED)

if(Vulkan_FOUND)
  target_link_libraries(${PROJECT_NAME} PUBLIC Vulkan::Vulkan)
  set(VULKAN_SDK_PATH $ENV{VULKAN_SDK})
else()
    message(FATAL_ERROR "Vulkan SDK not found. Make sure VULKAN_SDK is installed and accessible.")
endif()

target_compile_definitions(${PROJECT_NAME} PUBLIC VULKAN_SDK_PATH="${VULKAN_SDK_PATH}")

# --------------- Vendors --------------- #
# SPDLOG
add_subdirectory(Vendor/spdlog)
target_link_libraries(${PROJECT_NAME} PUBLIC spdlog::spdlog)
# SDL3
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
add_subdirectory(Vendor/SDL EXCLUDE_FROM_ALL)
target_link_libraries(${PROJECT_NAME} PUBLIC SDL3-static)
# GLM
set(GLM_BUILD_TESTS OFF CACHE BOOL "Disable GLM tests" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Use static GLM library" FORCE)
add_subdirectory(Vendor/glm)
target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm)
target_include_directories(${PROJECT_NAME} PUBLIC Vendor/glm)
# VOLK
add_subdirectory(Vendor/volk)
target_link_libraries(${PROJECT_NAME} PUBLIC volk_headers)
# SPIRV-REFLECT
set(SPIRV_REFLECT_EXECUTABLE OFF CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_STATIC_LIB ON CACHE BOOL "" FORCE)
add_subdirectory(Vendor/SPIRV-Reflect)
target_link_libraries(${PROJECT_NAME} PRIVATE spirv-reflect-static)
target_include_directories(${PROJECT_NAME} PUBLIC Vendor/SPIRV-Reflect)
# VMA
add_definitions(-DVMA_STATIC_VULKAN_FUNCTIONS=0 -DVMA_DYNAMIC_VULKAN_FUNCTIONS=0)
add_subdirectory(Vendor/VMA)
target_link_libraries(${PROJECT_NAME} PUBLIC GPUOpen::VulkanMemoryAllocator)
# Dear Imgui
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/imgui")
#Tracy
if(HELIX_WITH_TRACY)
  option(TRACY_ENABLE "" ON)
  option ( TRACY_ON_DEMAND "" ON )
  target_compile_definitions(${PROJECT_NAME} PUBLIC TRACY_VK_USE_SYMBOL_TABLE)

  set(TRACY_CLIENT_ADDRESS "127.0.0.1" CACHE STRING "Tracy client address")
  set(TRACY_PROFILER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/tracy/profiler/build/Release/tracy-profiler.exe" CACHE STRING "Tracy profiler executable path")

  add_subdirectory(Vendor/tracy)

  target_compile_definitions(${PROJECT_NAME} PUBLIC
      TRACY_ENABLE
      TRACY_ON_DEMAND
      "TRACY_CLIENT_ADDRESS=\"${TRACY_CLIENT_ADDRESS}\""
      "TRACY_PROFILER_DIR=\"${TRACY_PROFILER_DIR}\""
      "HELIX_WITH_TRACY=1"
  )

  target_link_libraries(${PROJECT_NAME} PUBLIC Tracy::TracyClient)
endif()
# OpenMP
find_package(OpenMP REQUIRED)
if(OpenMP_CXX_FOUND)
    target_link_libraries(${PROJECT_NAME} PUBLIC OpenMP::OpenMP_CXX)
endif()
# Include the vendor folder
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/")

# Set output directory to Bin/Engine
set_target_properties(${PROJECT_NAME} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../Bin/$<CONFIG>/${PROJECT_NAME}"
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../Bin/$<CONFIG>/${PROJECT_NAME}"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../Bin/$<CONFIG>/${PROJECT_NAME}"
)

