cmake_minimum_required(VERSION 3.16)
project(capture_tool_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(OpenCV REQUIRED)

find_package(Qt6 COMPONENTS Widgets QUIET)
if (Qt6_FOUND)
    set(Qt_LIBRARIES Qt6::Widgets)
else()
    find_package(Qt5 COMPONENTS Widgets REQUIRED)
    set(Qt_LIBRARIES Qt5::Widgets)
endif()

add_executable(${PROJECT_NAME} capture-tool.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE ${OpenCV_LIBS} ${Qt_LIBRARIES})

if (WIN32)
    # On Windows, enable multithreaded runtime and default subsystem settings
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE FALSE)
endif()
