cmake_minimum_required(VERSION 3.16)
project(NeonZone LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(NeonZone WIN32
    src/main.cpp
    src/core/Renderer.cpp
    src/core/Input.cpp
    src/core/Audio.cpp
    src/game/Entities.cpp
    src/game/Game.cpp
)

target_include_directories(NeonZone PRIVATE src)
target_link_libraries(NeonZone PRIVATE winmm)

if(MINGW)
    target_link_options(NeonZone PRIVATE -municode -static-libgcc -static-libstdc++)
endif()

if(MSVC)
    target_compile_options(NeonZone PRIVATE /utf-8 /EHsc)
endif()
