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
copy ..\Config\gradle.properties gradle.properties
copy ..\AndroidManifest.xml AndroidManifest.xml
copy ..\build.gradle build.gradle
cd ..
pause

cd Config
rem fix build error
set ANDROID_NDK=""
set ANDROID_NDK_HOME=""
call gradlew.bat -p ../build_android build
cd ..
pause