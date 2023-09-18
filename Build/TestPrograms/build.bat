
@echo off

REM ryu newtest.c
ryu interp_test.c

output.exe

echo Value returned by main: %errorlevel%

popd