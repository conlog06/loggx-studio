cmake_minimum_required(VERSION 3.24)
project(LoggXStudio VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
include(CTest)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Multimedia Concurrent)
qt_standard_project_setup()
add_library(loggx_core
  src/core/project.cpp src/core/document.cpp src/core/audio.cpp src/core/exportplan.cpp)
target_include_directories(loggx_core PUBLIC src)
target_link_libraries(loggx_core PUBLIC Qt6::Core Qt6::Gui)
qt_add_executable(LoggXStudio WIN32 MACOSX_BUNDLE
  src/main.cpp src/ui/window.cpp src/ui/timeline.cpp src/ui/pianoroll.cpp
  src/engine/playback.cpp src/engine/renderqueue.cpp src/engine/mediaimport.cpp)
target_link_libraries(LoggXStudio PRIVATE loggx_core Qt6::Widgets Qt6::Multimedia Qt6::Concurrent)
set_target_properties(LoggXStudio PROPERTIES
  MACOSX_BUNDLE_BUNDLE_NAME "LoggX Studio"
  MACOSX_BUNDLE_GUI_IDENTIFIER "studio.loggx.desktop"
  MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
  MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})
if(MSVC)
  target_compile_options(loggx_core PRIVATE /W4 /utf-8)
  target_compile_options(LoggXStudio PRIVATE /W4 /utf-8)
else()
  target_compile_options(loggx_core PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(LoggXStudio PRIVATE -Wall -Wextra -Wpedantic)
endif()
if(BUILD_TESTING)
  find_package(Qt6 REQUIRED COMPONENTS Test)
  qt_add_executable(loggx_tests tests/core_tests.cpp)
  target_link_libraries(loggx_tests PRIVATE loggx_core Qt6::Test)
  add_test(NAME core COMMAND loggx_tests)
  set_tests_properties(core PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
include(GNUInstallDirs)
install(TARGETS LoggXStudio BUNDLE DESTINATION . RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
qt_generate_deploy_app_script(TARGET LoggXStudio OUTPUT_SCRIPT deploy_script NO_UNSUPPORTED_PLATFORM_ERROR)
install(SCRIPT ${deploy_script})
