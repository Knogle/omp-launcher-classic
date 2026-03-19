cmake_minimum_required(VERSION 3.16)

project(omp_launcher_classic LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Svg)

qt_add_executable(omp_launcher_classic
  main.cpp
  resources.qrc
)

set_target_properties(omp_launcher_classic PROPERTIES OUTPUT_NAME "omp-launcher-classic")

target_link_libraries(omp_launcher_classic PRIVATE Qt6::Widgets)
target_link_libraries(omp_launcher_classic PRIVATE Qt6::Network)
target_link_libraries(omp_launcher_classic PRIVATE Qt6::Svg)

if (WIN32)
  target_sources(omp_launcher_classic PRIVATE app.rc)
  set_target_properties(omp_launcher_classic PROPERTIES WIN32_EXECUTABLE TRUE)
  target_link_libraries(omp_launcher_classic PRIVATE psapi shell32)
endif()
