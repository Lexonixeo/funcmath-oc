// SPDX-License-Identifier: MIT
#pragma once
// Поиск решения уровня (для подсказки и для проверки уровней).
// Работает с функциями произвольной арности и несколькими результатами.
// Случайные функции перебираются полностью (см. fmath::outcomes): решение
// считается найденным, если оно достижимо при каком-то исходе случайности.

#include <string>
#include <vector>

#include "Function.h"

namespace solver {

struct Move {
    std::string fn;
    std::vector<Num> args;
    std::vector<Num> results;
};

// Ищет последовательность ходов, приводящую к target.
// Поиск ограничен числом узлов (nodeLimit) и временем (timeLimitMs),
// чтобы всегда завершаться. false = решение не найдено или лимит исчерпан:
// это не доказательство нерешаемости.
bool solve(const std::vector<Num>& numbers, const std::vector<Fn>& functions,
           Num target, std::vector<Move>& moves,
           long long nodeLimit = 1000000, long long timeLimitMs = 1000);

// true, если в уровне есть нестабильные функции (rand и т.п.).
bool hasUnstable(const std::vector<Fn>& functions);

}  // namespace solver
