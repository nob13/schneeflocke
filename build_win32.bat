@echo off

if %1!==! (set BUILD_TYPE=Debug) ELSE (set BUILD_TYPE=%1)
echo Build Type is %BUILD_TYPE%


mkdir build
cd build
cmake ../ -G "Visual Studio 9 2008" -DCMAKE_INSTALL_PREFIX=../install -DSFSerialization_ROOT=C:\sfserialization\install -DLIBRARY_FLAGS=STATIC
if ERRORLEVEL 1 goto errorcase
"C:\Program Files\Microsoft Visual Studio 9.0\Common7\IDE\devenv.com" Schneeflocke.sln /build %BUILD_TYPE% /project ALL_BUILD
if ERRORLEVEL 1 goto errorcase
"C:\Program Files\Microsoft Visual Studio 9.0\Common7\IDE\devenv.com" Schneeflocke.sln /build %BUILD_TYPE% /project INSTALL
if ERRORLEVEL 1 goto errorcase
"C:\Program Files\Microsoft Visual Studio 9.0\Common7\IDE\devenv.com" Schneeflocke.sln /build %BUILD_TYPE% /project PACKAGE
if ERRORLEVEL 1 goto errorcase
cd ..
exit /b 0
:errorcase
echo "There was an error, build failed!!!"
exit /b 1

