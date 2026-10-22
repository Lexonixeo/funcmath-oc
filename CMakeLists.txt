cmake_minimum_required(VERSION 3.16)

set(FM_VERSION "0.2.0" CACHE STRING "funcmath version (without leading v)")
string(REGEX REPLACE "^v" "" FM_VERSION "${FM_VERSION}")
if(NOT FM_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
  message(FATAL_ERROR "FM_VERSION must look like 0.2.0 (got '${FM_VERSION}')")
endif()

project(funcmath VERSION ${FM_VERSION} LANGUAGES CXX)

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
  src/Expression.cpp
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

include(CTest)
if(BUILD_TESTING)
  add_executable(expression_tests
    tests/expression_tests.cpp
    src/Expression.cpp
    src/Functions.cpp
    src/Helper.cpp
  )
  target_include_directories(expression_tests PRIVATE src)
  if(MSVC)
    target_compile_options(expression_tests PRIVATE /W4)
  else()
    target_compile_options(expression_tests PRIVATE -Wall -Wextra)
  endif()
  add_test(NAME expression_tests COMMAND expression_tests)
  set_tests_properties(expression_tests PROPERTIES TIMEOUT 30)

  # Список вынесен в кэш, чтобы временно исключать проблемные уровни при
  # перенумерации, не пряча это в коде CMake. Сейчас он пуст.
  set(FM_KNOWN_UNSOLVABLE_LEVELS "" CACHE STRING
      "уровни, временно пропускаемые в CTest")
  file(GLOB FM_LEVEL_FILES CONFIGURE_DEPENDS
       "${CMAKE_CURRENT_SOURCE_DIR}/data/levels/level*.lvl")
  foreach(FM_LEVEL_FILE IN LISTS FM_LEVEL_FILES)
    get_filename_component(FM_LEVEL_NAME "${FM_LEVEL_FILE}" NAME)
    if(FM_LEVEL_NAME MATCHES "^level([0-9]+)\\.lvl$")
      set(FM_LEVEL "${CMAKE_MATCH_1}")
      if(FM_LEVEL IN_LIST FM_KNOWN_UNSOLVABLE_LEVELS)
        continue()
      endif()
      set(FM_LEVEL_ARGS --check ${FM_LEVEL})
      set(FM_LEVEL_TIMEOUT 60)
      # Уровень 18 требует более глубокого поиска: стандартного лимита
      # решателя не хватает, хотя решение существует. Это кандидат на
      # упрощение при аудите, но тест не должен его скрывать.
      if(FM_LEVEL STREQUAL "18")
        set(FM_LEVEL_ARGS --check 18 --nodes 50000000 --time 60000)
        set(FM_LEVEL_TIMEOUT 120)
      endif()
      add_test(NAME "level_${FM_LEVEL}_solvable"
               COMMAND funcmath ${FM_LEVEL_ARGS})
      set_tests_properties("level_${FM_LEVEL}_solvable" PROPERTIES
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:funcmath>"
        TIMEOUT ${FM_LEVEL_TIMEOUT})
    endif()
  endforeach()
endif()

# --- Упаковка релиза ---------------------------------------------------------

string(TOLOWER "${CMAKE_SYSTEM_NAME}" FM_OS)
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" FM_ARCH)
if(FM_ARCH STREQUAL "amd64" OR FM_ARCH STREQUAL "x86_64")
  set(FM_ARCH "x64")
elseif(FM_ARCH STREQUAL "aarch64" OR FM_ARCH STREQUAL "arm64")
  set(FM_ARCH "arm64")
endif()
set(FM_PLATFORM "${FM_OS}-${FM_ARCH}" CACHE STRING
    "release platform label, for example windows-x64")

set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ".")
include(InstallRequiredSystemLibraries)
install(TARGETS funcmath RUNTIME DESTINATION .)
install(DIRECTORY data DESTINATION .
        PATTERN "players" EXCLUDE
        PATTERN "lvlrate.dat" EXCLUDE
        PATTERN "cache" EXCLUDE
        PATTERN "logs" EXCLUDE)
install(DIRECTORY docs DESTINATION .)
install(FILES README.md LICENSE DESTINATION .)

# Для релиза дополнительно кладём простые .txt-версии документации: их можно
# открыть в Блокноте без просмотрщика Markdown. В репозитории основным
# форматом остаётся .md.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
  "${CMAKE_CURRENT_SOURCE_DIR}/docs/gameplay.md"
  "${CMAKE_CURRENT_SOURCE_DIR}/docs/level-authoring.md"
  "${CMAKE_CURRENT_SOURCE_DIR}/docs/level-design.md"
  "${CMAKE_CURRENT_SOURCE_DIR}/docs/game.md"
  "${CMAKE_CURRENT_SOURCE_DIR}/docs/architecture.md"
  "${CMAKE_CURRENT_SOURCE_DIR}/docs/level-audit.md"
  "${CMAKE_CURRENT_SOURCE_DIR}/docs/level-renumbering.md")

find_program(FM_PYTHON NAMES python3 python)
if(FM_PYTHON)
  set(FM_TXT_DOCS_DIR "${CMAKE_BINARY_DIR}/txt-docs")
  execute_process(
    COMMAND "${FM_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/md_to_txt.py"
            --source "${CMAKE_CURRENT_SOURCE_DIR}"
            --output "${FM_TXT_DOCS_DIR}"
    RESULT_VARIABLE FM_TXT_DOCS_RESULT
    OUTPUT_QUIET)
  if(FM_TXT_DOCS_RESULT EQUAL 0)
    install(DIRECTORY "${FM_TXT_DOCS_DIR}/" DESTINATION .)
  else()
    message(WARNING "Не удалось создать .txt-версии документации")
  endif()
else()
  message(WARNING "Python не найден: .txt-версии документации не будут упакованы")
endif()

set(CPACK_PACKAGE_NAME "funcmath-ds")
set(CPACK_PACKAGE_VENDOR "funcmath")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Console puzzle game func(math), C++17 port")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME
    "funcmath-ds-v${PROJECT_VERSION}-${FM_PLATFORM}")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/package")
if(WIN32)
  set(CPACK_GENERATOR "ZIP")
else()
  set(CPACK_GENERATOR "TGZ")
endif()
include(CPack)
