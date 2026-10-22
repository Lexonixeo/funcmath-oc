// SPDX-License-Identifier: MIT
#pragma once
// Прохождение одного уровня — порт класса Level.

#include <string>
#include <vector>

#include "LevelStore.h"

struct LevelResult {
    bool tutorial;   // показывать ли обучение в следующий раз
    bool completed;  // пройден ли уровень
};

// Число в наборе вместе с необязательным именем. Имя — это метка для
// читаемости и выбора: решатель по-прежнему работает только со значениями.
struct PoolItem {
    Num value;
    std::string name;
};

class Level {
public:
    Level(LevelData data, bool tutorial);
    LevelResult play();

private:
    LevelData data_;
    std::vector<PoolItem> pool_;      // текущий набор
    std::vector<PoolItem> original_;  // начальный набор для reset
    std::vector<std::vector<PoolItem>> stack_;  // снимки для back
    std::vector<std::string> history_;           // строки ходов
    bool tutorial_;
    bool completed_ = false;

    const Fn* findFunction(const std::string& name) const;

    void start();
    void printHeader() const;
    void printNumbers() const;
    void printFunctions() const;
    void printTutorial();
    void printHelp();

    void nums() const;
    void printHistory() const;
    void restart();
    void goBack();
    void hint();

    // Рекурсивно разбирает префиксное выражение. consumeFromSet=true —
    // аргументы-числа берутся из pool и удаляются из него; false — числа
    // создаются по literalType (режим calc).
    //
    // pos сдвигается на разобранные слова. result — значения, которые вернуло
    // выражение: их может быть несколько (поток), и родительская функция
    // сама решает, сколько из них использовать.
    bool evalExpression(const std::vector<std::string>& words, std::size_t& pos,
                        bool consumeFromSet, std::vector<PoolItem>& pool,
                        ValueType literalType, std::vector<Num>& result,
                        std::string& error) const;
    // Переводит ввод игрока в префиксные слова. Если строка начинается с
    // известной функции и не содержит инфикса, сохраняется старый путь через
    // wordsFromString; иначе используется expression::normalize.
    bool prepareExpression(const std::string& raw,
                           std::vector<std::string>& words,
                           std::string& error) const;
    // Применяет выражение к настоящему набору; при ошибке набор не меняется.
    void applyExpression(const std::string& raw);
    // Вычисляет выражение на копии набора, не меняя pool_/history_.
    void calc(const std::string& raw);

    int turn();  // -1 — выход из уровня
};
