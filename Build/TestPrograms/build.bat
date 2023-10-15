
@echo off
setlocal enabledelayedexpansion

if exist output.exe (
rm output.exe
)

REM ryu newtest.ryu
ryu interp_test.ryu -emit_bc -emit_ir -O 0 -o output.exe

if exist output.exe (
output.exe
echo Value returned by main: !errorlevel!
) else (
echo No .exe produced
)

popd