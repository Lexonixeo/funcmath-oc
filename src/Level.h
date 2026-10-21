#pragma once
// Прохождение одного уровня — порт класса Level.

#include <string>
#include <vector>

#include "LevelStore.h"

struct LevelResult {
    bool tutorial;
    bool completed;
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
    std::vector<PoolItem> pool_;
    std::vector<PoolItem> original_;
    std::vector<std::vector<PoolItem>> stack_;
    std::vector<std::string> history_;
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
    bool evalExpression(const std::vector<std::string>& words, std::size_t& pos,
                        bool consumeFromSet, std::vector<PoolItem>& pool,
                        ValueType literalType, std::vector<Num>& result,
                        std::string& error) const;
    void applyExpression(const std::vector<std::string>& words);
    void calc(const std::vector<std::string>& words);

    int turn();  // -1 — выход из уровня
};
