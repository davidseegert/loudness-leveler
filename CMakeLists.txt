cmake_minimum_required(VERSION 3.16)
project(loudness-leveler VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimization flags for Release mode
if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /Oi /Ot /Gy")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -ffast-math -march=native")
endif()

# Find wxWidgets
find_package(wxWidgets REQUIRED COMPONENTS core base adv)
include(${wxWidgets_USE_FILE})

# Sources
set(SOURCES
    main.cpp
    MainWindow.cpp
    MainWindow.h
    ControlGroup.cpp
    ControlGroup.h
    AudioProcessor.cpp
    AudioProcessor.h
    AudioPlayerDevice.cpp
    AudioPlayerDevice.h
    WaveformWidget.cpp
    WaveformWidget.h
    LufsAnalyzer.cpp
)

add_executable(loudness-leveler ${SOURCES})

target_link_libraries(loudness-leveler PRIVATE ${wxWidgets_LIBRARIES})

if(APPLE)
    target_link_libraries(loudness-leveler PRIVATE "-framework CoreAudio" "-framework AudioUnit" "-framework CoreFoundation" "-framework AudioToolbox")
elseif(UNIX AND NOT APPLE)
    target_link_libraries(loudness-leveler PRIVATE dl m pthread)
endif()