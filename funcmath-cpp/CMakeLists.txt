cmake_minimum_required(VERSION 3.16)
project(funcmath LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(funcmath
  src/main.cpp
  src/Helper.cpp
  src/Functions.cpp
  src/Function.cpp
  src/LevelStore.cpp
  src/Level.cpp
  src/Solver.cpp
  src/LevelGenerator.cpp
  src/Player.cpp
  src/Game.cpp
)

if(MSVC)
  target_compile_options(funcmath PRIVATE /W4)
else()
  target_compile_options(funcmath PRIVATE -Wall -Wextra)
endif()

# Каталог с данными копируется рядом с бинарником при сборке.
add_custom_command(TARGET funcmath POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_CURRENT_SOURCE_DIR}/data"
          "$<TARGET_FILE_DIR:funcmath>/data"
)
