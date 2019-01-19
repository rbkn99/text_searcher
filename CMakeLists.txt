cmake_minimum_required(VERSION 2.8)

project(text_searcher)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wall -pedantic")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}  -g -D_GLIBCXX_DEBUG")
endif()

find_package(Qt5Widgets REQUIRED)

add_executable(text_searcher
        main.cpp
        mainwindow.h
        mainwindow.cpp
        scanner.h
        scanner.cpp
        tests.cpp
        gtest/gtest.h
        gtest/gtest-all.cc
        #gtest/gtest_main.cc
        )
qt5_use_modules(text_searcher Widgets)
target_link_libraries(text_searcher -lpthread)