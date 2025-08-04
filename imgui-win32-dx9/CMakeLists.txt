cmake_minimum_required(VERSION 3.10)
project(BuildImguiWinDX9 CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT WIN32)
    message(FATAL_ERROR "require Win32 environment")
endif()

option(ENABLE_OPTIMIZATION "optimization" ON)

set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(OUTPUT_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib")
file(GLOB OBJECT_SOURCE "src/*.cpp")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(LIBRARY_OUTPUT_NAME)

include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)


add_library(imgui-win32-dx9 STATIC ${OBJECT_SOURCE})

target_include_directories(imgui-win32-dx9 PUBLIC ${INCLUDE_DIR})
target_link_libraries(imgui-win32-dx9 PUBLIC gdi32)

if(ENABLE_OPTIMIZATION)
    target_compile_options(imgui-win32-dx9 PRIVATE "-Os" "-march=nehalem" "-s")
endif()

set_target_properties(imgui-win32-dx9 PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_LIB_DIR}"
)

# Do not install the library system-wide
install(TARGETS imgui-win32-dx9 EXPORT MyTarget
    ARCHIVE DESTINATION "${OUTPUT_LIB_DIR}" # This ensures it's only placed in lib/ if install is run, but we are not running install
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
)