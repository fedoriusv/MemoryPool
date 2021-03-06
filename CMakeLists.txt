cmake_minimum_required(VERSION 3.4.1)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set (CURRENT_PROJECT "MemoryPoolTest")
project(${CURRENT_PROJECT})

if (TARGET_WINDOWS)
    include(Config/Windows.cmake)
elseif (TARGET_ANDROID)
    include(Config/Android.cmake)
else()
    message(FATAL_ERROR "Unknown platform. Only Platform Windows | Android supported")
endif()

if (TARGET_ANDROID)
    file(GLOB ANDROID_NATIVE_FILES ${ANDROID_NATIVE_PATH}/android_native_app_glue.h ${ANDROID_NATIVE_PATH}/android_native_app_glue.c)
endif()
file(GLOB SOURCE_FILES MemoryPool.h MemoryPool.cpp)
file(GLOB TEST_FILES Test.cpp)

source_group("" FILES ${SOURCE_FILES} ${TEST_FILES})

#mimalloc
set(MIMALLOC_LIB Libraries/mimalloc)
add_subdirectory(${MIMALLOC_LIB})
set_target_properties(mimalloc PROPERTIES FOLDER ${MIMALLOC_LIB})
set_target_properties(mimalloc-static PROPERTIES FOLDER ${MIMALLOC_LIB})
set_target_properties(mimalloc-obj PROPERTIES FOLDER ${MIMALLOC_LIB})
set_target_properties(mimalloc-test-api PROPERTIES FOLDER ${MIMALLOC_LIB})
set_target_properties(mimalloc-test-stress PROPERTIES FOLDER ${MIMALLOC_LIB})


if (TARGET_WINDOWS)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:CONSOLE")
    add_executable(${CURRENT_PROJECT} WIN32 ${SOURCE_FILES}	${TEST_FILES})
elseif (TARGET_ANDROID)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")
    add_library(${CURRENT_PROJECT} SHARED ${SOURCE_FILES} ${TEST_FILES} ${ANDROID_NATIVE_FILES})
    target_link_libraries(${CURRENT_PROJECT} log android)
endif()

#mimalloc
target_link_libraries(${CURRENT_PROJECT} mimalloc-static)
add_dependencies(${CURRENT_PROJECT} mimalloc-static)