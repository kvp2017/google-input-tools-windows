cmake_minimum_required(VERSION 3.21)
cmake_policy(SET CMP0091 NEW)
project(GoogleInputToolsWindows VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_library(input_engine STATIC src/tamil_transliterator.cpp)
target_include_directories(input_engine PUBLIC include)
target_compile_features(input_engine PUBLIC cxx_std_20)
set_target_properties(input_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    target_compile_options(input_engine PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(input_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(tamil-ime-demo src/main.cpp)
target_link_libraries(tamil-ime-demo PRIVATE input_engine)

enable_testing()
add_executable(tamil-engine-tests tests/tamil_engine_tests.cpp)
target_link_libraries(tamil-engine-tests PRIVATE input_engine)
add_test(NAME tamil-engine-smoke COMMAND tamil-engine-tests)

if(WIN32)
    add_library(tamil_tsf SHARED
        src/windows-tsf/tsf_provider.cpp
        src/windows-tsf/TamilTextService.def
    )
    target_link_libraries(tamil_tsf PRIVATE input_engine ole32 uuid advapi32)
    target_compile_definitions(tamil_tsf PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(tamil_tsf PRIVATE /W4 /permissive- /utf-8)
    endif()
    set_target_properties(tamil_tsf PROPERTIES OUTPUT_NAME "TamilTextService")

    add_executable(TamilImeRegistration src/windows-tsf/register_tamil_ime.cpp)
    target_link_libraries(TamilImeRegistration PRIVATE ole32 uuid advapi32)
    target_compile_definitions(TamilImeRegistration PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)

    add_executable(TamilImeSettings WIN32 src/settings/tamil_ime_settings.cpp)
    target_link_libraries(TamilImeSettings PRIVATE shell32 advapi32)
    target_compile_definitions(TamilImeSettings PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
endif()
