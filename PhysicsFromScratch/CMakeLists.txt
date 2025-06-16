cmake_minimum_required(VERSION 3.16)
project(PhysicsFromScratch)

# Visual Studio 
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Define your project sources and headers
file(GLOB_RECURSE SOURCE_LIST "Src/*.cpp" "Src/*.hpp")

# Build as an EXE
add_executable(${PROJECT_NAME} 
    ${SOURCE_LIST}
)


set(OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Bin/$<CONFIG>/${PROJECT_NAME}")

set_target_properties(${PROJECT_NAME} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}"
)


target_link_libraries(${PROJECT_NAME} PRIVATE Helix)

target_include_directories(${PROJECT_NAME} PUBLIC "Src/")
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
