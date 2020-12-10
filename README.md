# MemoryPool
Memory management pool<br/>
git clone --recursive https://github.com/fedoriusv/MemoryPool.git<br/>

## Build Windows:
### Requariment:
- CMake 3.0 or later<br/>
- Visual Studio 2019<br/>

### Command:
cmake -G"Visual Studio 16 2019" -A x64 -DTARGET_WINDOWS=ON -B build_windows<br/>
cmake --build build_windows --clean-first --config Release<br/>

## Build Android on Windows:
### Requariment: 
- CMake 3.0 or later + Ninja<br/>
- Android NDK r20 or later<br/>
- Android SDK 26 or later<br/>
- Gradle 3.3(included)

### Test:
Compare memory pool with:<br/>
--STD malloc<br/>
--Mimalloc (https://github.com/microsoft/mimalloc)<br/>
 
### Command:
**ANDROID_NDK** - path to NDK<br/>
**ANDROID_HOME** - path to SDK<br/>
Info: https://developer.android.com/ndk/guides/cmake<br/>

Call: *android_build.bat*<br/>
Log: adb logcat -c && adb logcat | grep MemoryPool<br/>

