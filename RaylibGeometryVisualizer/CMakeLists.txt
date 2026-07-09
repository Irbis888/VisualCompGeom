cmake_minimum_required(VERSION 3.20)

project(GeometryVisualizer VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# Keep the dependency local to this build and pin it for reproducible builds.
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(RAYLIB_ARCHIVE "${CMAKE_CURRENT_SOURCE_DIR}/raylib-6.0.zip")
if(EXISTS "${RAYLIB_ARCHIVE}")
    message(STATUS "Using local Raylib archive: ${RAYLIB_ARCHIVE}")
    FetchContent_Declare(
        raylib
        URL "${RAYLIB_ARCHIVE}"
        URL_HASH SHA256=A6770924FED7B25DE9F6009EC236CD095A99577A3907026DA2BFA21A27AD271A
        DOWNLOAD_EXTRACT_TIMESTAMP OFF
    )
else()
    FetchContent_Declare(
        raylib
        URL https://codeload.github.com/raysan5/raylib/zip/refs/tags/6.0
        URL_HASH SHA256=A6770924FED7B25DE9F6009EC236CD095A99577A3907026DA2BFA21A27AD271A
        DOWNLOAD_EXTRACT_TIMESTAMP OFF
    )
endif()
FetchContent_MakeAvailable(raylib)

add_library(triangulation_algorithm
    ../Triangulation/Triangulation.cpp
)
target_compile_definitions(triangulation_algorithm PRIVATE TRIANGULATION_NO_MAIN)
target_include_directories(triangulation_algorithm PUBLIC
    ../Triangulation
    ../include
)

add_library(cpu_convex_hull_algorithm
    ../CompGeomAlgos/CPUConvexHull.cpp
)

add_library(fortune_voronoi_algorithm
    ../FortuneVoronoi/FortuneVoronoi.cpp
)
target_compile_definitions(fortune_voronoi_algorithm PRIVATE FORTUNE_VORONOI_NO_MAIN)
target_include_directories(fortune_voronoi_algorithm PUBLIC
    ../FortuneVoronoi
    ../include
)
target_compile_definitions(cpu_convex_hull_algorithm PRIVATE CPU_CONVEX_HULL_NO_MAIN)
target_include_directories(cpu_convex_hull_algorithm PUBLIC
    ../CompGeomAlgos
    ../include
)

add_executable(geometry_visualizer
    src/main.cpp
    src/algorithm_registry.cpp
    src/input_register.cpp
)
target_include_directories(geometry_visualizer PRIVATE src ../include)
target_compile_definitions(geometry_visualizer PRIVATE
    CPU_CONVEX_INPUT_FILE="${CMAKE_CURRENT_SOURCE_DIR}/../CompGeomAlgos/input.txt"
    TRIANGULATION_INPUT_FILE="${CMAKE_CURRENT_SOURCE_DIR}/../Triangulation/input.txt"
    FORTUNE_VORONOI_INPUT_FILE="${CMAKE_CURRENT_SOURCE_DIR}/../FortuneVoronoi/input.txt"
)
target_link_libraries(geometry_visualizer PRIVATE
    raylib
    triangulation_algorithm
    cpu_convex_hull_algorithm
    fortune_voronoi_algorithm
)

if(MSVC)
    target_compile_options(geometry_visualizer PRIVATE /W4 /permissive-)
else()
    target_compile_options(geometry_visualizer PRIVATE -Wall -Wextra -Wpedantic)
endif()
