cmake_minimum_required(VERSION 3.10)
project(MyProject)

# Set the base directory
set(BaseDir ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Compiler settings
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -g -march=native")
set(CMAKE_CXX_FLAGS_ENGINE "${CMAKE_CXX_FLAGS} -Os")
set(CMAKE_INCLUDE_PATH "${BaseDir}/include")

# Include directories
include_directories(${BaseDir}/include)
include_directories(${BaseDir}/imgui-sfml/include)

# Source files
file(GLOB EngineSources "${BaseDir}/src/engine/*.cpp")
file(GLOB ComponentSources "${BaseDir}/src/component/*.cpp")
set(ClientSource "${BaseDir}/src/client.cpp")
set(ServerSource "${BaseDir}/src/server.cpp")

# Create output directories
set(EngineOutputDir "${BaseDir}/bin/engine")
set(ComponentOutputDir "${BaseDir}/bin/component")
set(ClientOutputDir "${BaseDir}/bin")
set(ServerOutputDir "${BaseDir}/bin")

# Ensure output directories exist
file(MAKE_DIRECTORY ${EngineOutputDir})
file(MAKE_DIRECTORY ${ComponentOutputDir})
file(MAKE_DIRECTORY ${ClientOutputDir})
file(MAKE_DIRECTORY ${ServerOutputDir})

# Compile engine
foreach(source ${EngineSources})
    get_filename_component(source_name ${source} NAME_WE)
    add_library(engine_obj OBJECT ${source})
    set_target_properties(engine_obj PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${EngineOutputDir}
        LIBRARY_OUTPUT_DIRECTORY ${EngineOutputDir}
        RUNTIME_OUTPUT_DIRECTORY ${EngineOutputDir}
    )
endforeach()

# Compile components
foreach(source ${ComponentSources})
    get_filename_component(source_name ${source} NAME_WE)
    add_library(component_obj OBJECT ${source})
    set_target_properties(component_obj PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${ComponentOutputDir}
        LIBRARY_OUTPUT_DIRECTORY ${ComponentOutputDir}
        RUNTIME_OUTPUT_DIRECTORY ${ComponentOutputDir}
    )
endforeach()

# Create the client executable
add_executable(client ${ClientSource} $<TARGET_OBJECTS:engine_obj> $<TARGET_OBJECTS:component_obj>)
target_link_libraries(client
    imgui-sfml
    opengl32
    glu32
    sfml-system
    sfml-window
    sfml-audio
    sfml-main
    sfml-graphics
    sodium
    ws2_32
    mf
    mfplat
    mfreadwrite
    mfuuid
    shlwapi
    ole32
    oleaut32
    rpcrt4
    gdi32
    gdiplus
)

# Create the server executable
add_executable(server ${ServerSource} $<TARGET_OBJECTS:engine_obj> $<TARGET_OBJECTS:component_obj>)
target_link_libraries(server
    imgui-sfml
    opengl32
    glu32
    sfml-system
    sfml-window
    sfml-audio
    sfml-main
    sfml-graphics
    sodium
    ws2_32
    mf
    mfplat
    mfreadwrite
    mfuuid
    shlwapi
    ole32
    oleaut32
    rpcrt4
    gdi32
    gdiplus
)

# Set output directories for executables
set_target_properties(client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${ClientOutputDir}
)

set_target_properties(server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${ServerOutputDir}
)