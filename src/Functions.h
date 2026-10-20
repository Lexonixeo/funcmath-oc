#pragma once
// Библиотека простейших функций — порт класса SimplestFunctions.
//
// Каждая функция описывается сигнатурой (типы аргументов), арностью и
// реализацией. Арность может быть 1..N, а результат — вектор значений:
// например, primes раскладывает число на несколько простых множителей.
// Вложенные префиксные выражения разбираются в Level.cpp; здесь только
// применение одной функции к готовым аргументам.

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "Value.h"

using Num = Value;

namespace fmath {

using Op = std::function<Num(Num, Num)>;

// Сигнатура функции: типы аргументов. Integer принимает и natural
// (расширяющее преобразование), natural — только natural.
struct Signature {
    std::vector<ValueType> args;
};

struct FunctionSpec {
    std::vector<Signature> signatures;
    bool stable = true;
    // Применение к аргументам. Может бросить исключение (strict natural,
    // деление на ноль и т.п.).
    std::function<std::vector<Num>(const std::vector<Num>&)> apply;
    // Для случайных функций — все возможные наборы результатов.
    // Для стабильных пусто, и outcomes() использует apply().
    std::function<std::vector<std::vector<Num>>(const std::vector<Num>&)> allOutcomes;

    int arity() const;
};

// Реестр: имя поведения -> описание функции.
const std::map<std::string, FunctionSpec>& registry();
const FunctionSpec* findSpec(const std::string& name);
bool hasFunction(const std::string& name);
int functionArity(const std::string& name);

// Старый реестр бинарных операций. Оставлен для генератора и обратной
// совместимости; новые функции лучше добавлять через registry().
const std::map<std::string, Op>& ops();

// Загружает описания функций из файла вида "имя описание".
void loadDescriptions(const std::string& path);

// Описание функции (или само имя, если описания нет).
std::string describe(const std::string& name);

// Стабильна ли функция (детерминирована). rand/sumr/subr/mulr/divr — нет.
bool isStable(const std::string& name);

// Применяет функцию, проверяя число и типы аргументов.
std::vector<Num> apply(const std::string& name, const std::vector<Num>& args);

// Возможные результаты функции: один набор для стабильной, несколько для
// случайной. Для стабильной функции возвращается ровно один вариант.
std::vector<std::vector<Num>> outcomes(const std::string& name,
                                       const std::vector<Num>& args);

// Случайное целое в [min(a,b), max(a,b)] включительно.
long long randBetween(long long a, long long b);

// Список имён функций, подходящих для генерации уровней.
const std::vector<std::string>& generationPool();

}  // namespace fmath
