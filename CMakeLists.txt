cmake_minimum_required(VERSION 3.16)
project(VulkanSandbox)

# Set C++ standard globally
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(HELIX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Helix")
add_definitions(-DNOMINMAX)
# Helix
set(HELIX_SHARED OFF CACHE BOOL "" FORCE)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/Helix")

target_compile_definitions(Helix PUBLIC
    $<$<CONFIG:DEBUG>:_DEBUG>
    UNICODE
    _UNICODE
    _CRT_SECURE_NO_WARNINGS
    VK_NO_PROTOTYPES
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/PhysicsFromScratch")

target_compile_definitions(Helix PUBLIC ASSETS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/Assets/")
target_compile_definitions(Helix PUBLIC SHADER_PATH="${CMAKE_CURRENT_SOURCE_DIR}/Assets/Shaders/")

# Set the startup project to Sandbox
if(MSVC)
  set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTY VS_STARTUP_PROJECT "${CMAKE_CURRENT_SOURCE_DIR}/PhysicsFromScratch")
endif()
