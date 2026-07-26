# Enable Qt code generation for .qrc files.
set(CMAKE_AUTORCC ON)

find_package(Qt5 REQUIRED COMPONENTS Core)

# Public headers exported by the winman shared library.
set(WINMAN_HEADERS
    include/winman
    include/detail/Common.h
    include/detail/WinOps.h
)

set(WINMAN_SOURCES
    src/WinOps.cpp
    resources/resources.qrc
)

# Win32 operations shared library.
add_library(winman SHARED ${WINMAN_HEADERS} ${WINMAN_SOURCES})

target_include_directories(winman
    PUBLIC
        # Public headers while building inside this repository.
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        # Public headers after installation.
        $<INSTALL_INTERFACE:include>
)

# Export symbols when building the DLL itself.
target_compile_definitions(winman PRIVATE WINMAN_LIBRARY)

set_target_properties(winman PROPERTIES
    OUTPUT_NAME "winman"
    DEBUG_POSTFIX "d"
)

# Qt Core for JSON parsing and resource embedding.
# DWM APIs are required for cloaking checks and window metadata.
target_link_libraries(winman PRIVATE
    Qt5::Core
    dwmapi
)
