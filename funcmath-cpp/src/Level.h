#pragma once
// Прохождение одного уровня — порт класса Level.

#include <string>
#include <vector>

#include "LevelStore.h"

struct LevelResult {
    bool tutorial;
    bool completed;
};

class Level {
public:
    Level(LevelData data, bool tutorial);
    LevelResult play();

private:
    LevelData data_;
    std::vector<Num> numbers_;
    std::vector<Num> original_;
    std::vector<std::vector<Num>> stack_;
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
    void restart();
    void goBack();
    void hint();

    bool multisetContains(const std::vector<Num>& args) const;
    void useFunction(const std::vector<std::string>& words);
    void calc(const std::vector<std::string>& words);

    int turn();  // -1 — выход из уровня
};
