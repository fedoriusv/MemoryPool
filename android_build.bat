@echo off

setlocal
if "%ANDROID_NDK%" == "" (
	echo Error. Variable ANDROID_NDK doesn't defined
	pause
	exit
)

set C_ANDROID_TOOLCHAIN=%ANDROID_NDK%\build\cmake\android.toolchain.cmake
set C_ANDROID_NDK=%ANDROID_NDK%
set C_ANDROID_ABI=arm64-v8a
set C_ANDROID_PLATFORM=android-26
set C_BUILD_CONFIG=Release

rmdir /S /Q build_android

cmake -G"Ninja" -DCMAKE_MAKE_PROGRAM=Ninja -DCMAKE_TOOLCHAIN_FILE=%C_ANDROID_TOOLCHAIN% -DANDROID_NDK=%C_ANDROID_NDK% -DANDROID_ABI=%C_ANDROID_ABI% -DANDROID_PLATFORM=%C_ANDROID_PLATFORM% -DCMAKE_BUILD_TYPE=%C_BUILD_CONFIG% -DTARGET_ANDROID=ON -B build_android
cmake --build build_android --clean-first --config %C_BUILD_CONFIG%
pause

cd build_android
mkdir libs\%C_ANDROID_ABI%
copy libMemoryPoolTest.so libs\\%C_ANDROID_ABI%
copy ..\Config\AndroidManifest.xml AndroidManifest.xml
copy ..\Config\build.xml build.xml
copy ..\Config\project.properties project.properties

pause

ant debug -Dout.final.file=..\MemoryPoolTest.apk
cd ..
pause