include_guard(GLOBAL)

# Adds strict warnings for all project targets.
function(winsize_apply_compiler_options)
    if(MSVC)
        # /W4 is the baseline warning level for MSVC builds.
        add_compile_options(/W4)
    else()
        # Keep non-MSVC builds warning-clean under common compilers.
        add_compile_options(-Wall -Wextra -Wpedantic)
    endif()

    # Define common Windows macros globally across all translation units.
    # WIN32_LEAN_AND_MEAN: exclude rarely-used Windows headers for faster builds.
    # NOMINMAX: prevent Windows.h from defining min/max macros that conflict with STL.
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
endfunction()
