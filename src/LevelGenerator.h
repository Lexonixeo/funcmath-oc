// SPDX-License-Identifier: MIT
#pragma once
// Генерация случайных уровней и интерактивное создание уровней.
// Порт LevelGenerator (в Java генерация была заглушкой // to do...).

#include <string>

#include "LevelStore.h"

namespace levelgen {

// Случайный уровень заданной сложности (>= 1), гарантированно решаемый:
// ответ получается реальным применением выбранных функций к числам.
LevelData generateRandom(int difficulty);

// Интерактивное создание уровня: спрашивает данные и сохраняет файл.
void authorLevel(const std::string& dataDir);

// Интерактивный редактор пользовательской функции (аналог FunctionMakerPanel).
// Сохраняет блок в data/functions/<имя>.fn.
void functionEditor(const std::string& dataDir);

}  // namespace levelgen
