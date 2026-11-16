# Reference solution notes

This is a separate worked example based on the supplied ECE 309 final starter and the supplied P2/P3 project files. It is meant to help study the design before writing a personal implementation. The source and design log include AI assistance disclosure; read the structures and reimplement them in your own style before using any coursework submission.

Validation performed in the authoring environment:
- 22 final tests, 14 P2 regression tests, and 9 P3 tool-loop tests passed.
- All 11 end-to-end sessions passed.
- C++17 compile used `-Wall -Wextra -Wpedantic -Werror` and AddressSanitizer/UndefinedBehaviorSanitizer.
- LeakSanitizer was disabled because it cannot run under this environment's ptrace setup. CMake is unavailable here, so the sources were compiled directly with g++; the included `CMakeLists.txt` remains the normal build entry point on a Linux system with CMake installed.
