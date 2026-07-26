# Enable Qt code generation for .ui, Q_OBJECT, and .qrc files.
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets LinguistTools)

# Version metadata shared by app runtime and Windows resource script.
set(WINSIZE_APP_VERSION "${PROJECT_VERSION}")
set(WINSIZE_FILE_VERSION
    "${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},0"
)
set(WINSIZE_FILE_VERSION_STR "${WINSIZE_APP_VERSION}.0")

# Keep VERSIONINFO OriginalFilename aligned with final executable name.
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(WINSIZE_ORIGINAL_FILENAME "winsized.exe")
else()
    set(WINSIZE_ORIGINAL_FILENAME "winsize.exe")
endif()

# Convert source directory to forward slashes for windres compatibility.
string(REPLACE "\\" "/" WINSIZE_RC_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# Translation source files compiled into .qm outputs.
set(WINSIZE_TS_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/i18n/winsize_zh_CN.ts
)

# Application sources, including generated winsize.rc.
set(WINSIZE_APP_SOURCES
    main.cpp
    MainWindow.h
    MainWindow.cpp
    MainWindow.ui
    resources/resources.qrc
    ${CMAKE_CURRENT_BINARY_DIR}/winsize.rc
)

# Generate the versioned resource file consumed by the executable target.
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/winsize.rc.in
    ${CMAKE_CURRENT_BINARY_DIR}/winsize.rc
    @ONLY
)

add_executable(winsize_app ${WINSIZE_APP_SOURCES})

# Compile .ts files into .qm files during build.
qt5_add_translation(WINSIZE_QM_FILES ${WINSIZE_TS_FILES})

# Build translations as a first-class target and order app after it.
add_custom_target(winsize_translations ALL DEPENDS ${WINSIZE_QM_FILES})
add_dependencies(winsize_app winsize_translations)

# Copy compiled translations beside the executable.
set(WINSIZE_TRANSLATIONS_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/translations")
foreach(_qm ${WINSIZE_QM_FILES})
    add_custom_command(TARGET winsize_translations POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${WINSIZE_TRANSLATIONS_OUTPUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_qm}" "${WINSIZE_TRANSLATIONS_OUTPUT_DIR}"
        COMMENT "Copying ${_qm} -> translations/"
    )
endforeach()

target_link_libraries(winsize_app PRIVATE
    Qt5::Widgets
    winman
)

target_compile_definitions(winsize_app PRIVATE
    WINSIZE_APP_VERSION="${WINSIZE_APP_VERSION}"
)

set_target_properties(winsize_app PROPERTIES
    OUTPUT_NAME winsize
    DEBUG_POSTFIX d
    # Build as a GUI subsystem app on Windows (no console window).
    WIN32_EXECUTABLE TRUE
)
