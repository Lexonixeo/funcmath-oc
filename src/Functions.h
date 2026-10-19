#pragma once
// Библиотека простейших функций — порт класса SimplestFunctions.
// Значения — long long (в Java были int; для игровых уровней поведение совпадает).

#include <functional>
#include <map>
#include <string>
#include <vector>

using Num = long long;

namespace fmath {

using Op = std::function<Num(Num, Num)>;

// Реестр: имя функции -> реализация.
const std::map<std::string, Op>& ops();

// Загружает описания функций из файла вида "имя описание".
void loadDescriptions(const std::string& path);

// Описание функции (или само имя, если описания нет).
std::string describe(const std::string& name);

// Стабильна ли функция (детерминирована). rand/sumr/subr/mulr/divr — нет.
bool isStable(const std::string& name);

// Возможные результаты функции над парой (a, b) — для перебора случайности.
// Для стабильной функции возвращается один элемент; для rand/sumr/subr/mulr/divr —
// весь диапазон возможных значений (полный перебор; см. замечание в Functions.cpp).
// Стратегию перебора можно заменить в одном месте, не трогая решатель.
std::vector<Num> outcomes(const std::string& name, Num a, Num b);

// Случайное целое в [min(a,b), max(a,b)] включительно.
Num randBetween(Num a, Num b);

// Список имён функций, подходящих для генерации уровней.
const std::vector<std::string>& generationPool();

}  // namespace fmath
