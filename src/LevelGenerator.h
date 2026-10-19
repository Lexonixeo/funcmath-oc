#pragma once
// Генерация случайных уровней и интерактивное создание уровней.
// Порт LevelGenerator (в Java генерация была заглушкой // to do...).

#include <string>

#include "LevelStore.h"

namespace levelgen {

// Случайный уровень заданной сложности (>= 1), гарантированно решаемый.
LevelData generateRandom(int difficulty);

// Интерактивное создание уровня: спрашивает данные и сохраняет файл.
void authorLevel(const std::string& dataDir);

}  // namespace levelgen
