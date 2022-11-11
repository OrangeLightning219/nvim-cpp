@echo off
if not exist build mkdir build

set compiler_args=^
-Fe:"nvim-cpp.exe" ^
-nologo ^
-GR- ^
-EHa- ^
-O2 -Oi ^
-WX -W4 ^
-DSLOW -DINTERNAL -DUNITY_BUILD ^
-FC ^
-Zi ^
-std:c++17 ^
-diagnostics:caret ^
-wd4201 ^
-wd4100 ^
-wd4505 ^
-wd4189 ^
-wd4456

set linker_args=-incremental:no 
set linker_libs=Ws2_32.lib Shell32.lib

pushd build

cl %compiler_args% -MTd  ../main.cpp /link %linker_args% %linker_libs% && echo Build succesfull || echo Build failed

popd
