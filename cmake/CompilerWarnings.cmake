# Interface target carrying the warning flags for KickForge's own sources.
# JUCE module sources are compiled through juce_add_plugin and keep JUCE's flags.

add_library(kickforge_warnings INTERFACE)

if(MSVC)
    target_compile_options(kickforge_warnings INTERFACE /W4)
else()
    target_compile_options(kickforge_warnings INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
    )
endif()
