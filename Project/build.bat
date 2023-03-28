@echo off

IF NOT EXIST ..\Build mkdir ..\Build
pushd ..\Build

set debug_flags=/DDebug /MDd /Zi
set profile_flags=/DProfile

set common=/FC /FeConstellate.exe /std:c++20 /permissive /wd4530 /nologo ..\Project\Source\unity_build.cpp /link ..\Project\Libs\LLVM-C.lib

set opt=/Od

REM Development build, debug is enabled, profiling and optimization disabled
cl /Od %debug_flags% %common%

REM Optimized build with debug information
REM cl /O2 %debug_flags% %common%

REM Profiling of optimized, final build, no debug info
REM cl /O2 %profile_flags% %common%

REM Final build
REM cl /O2 %common%

popd