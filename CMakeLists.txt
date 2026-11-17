cmake_minimum_required(VERSION 3.16)
project(Lviewer VERSION 1.0.0 LANGUAGES C CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(LVIEWER_TESTS "Build host-side unit tests" OFF)

if(WIN32)
  file(GLOB LV_SRC CONFIGURE_DEPENDS src/*.cpp src/core/*.cpp)
  add_executable(Lviewer WIN32 ${LV_SRC} third_party/miniz/miniz.c res/Lviewer.rc)
  target_include_directories(Lviewer PRIVATE src third_party/miniz res)
  target_compile_definitions(Lviewer PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
      _WIN32_WINNT=0x0A00 WINVER=0x0A00 MINIZ_NO_ZLIB_COMPATIBLE_NAMES
      LV_VERSION_STR=L"${PROJECT_VERSION}")
  target_link_libraries(Lviewer PRIVATE windowscodecs ole32 oleaut32 uuid comctl32 comdlg32
      shell32 shlwapi gdi32 user32 advapi32 winspool propsys imm32)
  if(MSVC)
    target_compile_options(Lviewer PRIVATE /utf-8 /W3 /O2 /MP)
    set_property(TARGET Lviewer PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
    target_link_options(Lviewer PRIVATE /MANIFEST:NO)
  else()
    target_compile_options(Lviewer PRIVATE -municode -O2 -Wall -Wno-unknown-pragmas -Wno-missing-field-initializers -Wno-cast-function-type $<$<COMPILE_LANGUAGE:CXX>:-Wno-invalid-offsetof>)
    target_link_options(Lviewer PRIVATE -municode -static -static-libgcc -static-libstdc++ -s)
  endif()
endif()

if(LVIEWER_TESTS)
  enable_testing()
  add_executable(lv_tests tests/test_main.cpp src/core/Filters.cpp src/core/NaturalSort.cpp src/core/LangFile.cpp)
  target_include_directories(lv_tests PRIVATE src src/core)
  add_test(NAME unit COMMAND lv_tests ${CMAKE_SOURCE_DIR}/res/lang)
endif()
