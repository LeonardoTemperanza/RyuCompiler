
@echo off

REM ryu newtest.c
ryu interp_test.c

pushd ..

output.exe

echo Value returned by main: %errorlevel%

popd