# OptoComp
Original opto-style compressor (VST3/AU/Standalone), JUCE 8.

Build:
    cmake -B build
    cmake --build build --config Release

First configure downloads JUCE (needs internet). Plugins are copied to your
system plugin folder after build. Uses JUCE's generic UI (sliders) for now.
