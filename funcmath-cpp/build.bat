@echo off
rem Сборка func(math) через g++ (MinGW).
setlocal

set SRC=src\main.cpp src\Helper.cpp src\Functions.cpp src\Function.cpp ^
        src\LevelStore.cpp src\Level.cpp src\Solver.cpp ^
        src\LevelGenerator.cpp src\Player.cpp src\Game.cpp

g++ -std=c++17 -Wall -Wextra -O2 -o funcmath.exe %SRC%
if errorlevel 1 (
    echo.
    echo Сборка не удалась.
    exit /b 1
)

echo.
echo Готово: funcmath.exe
endlocal
