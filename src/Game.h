// SPDX-License-Identifier: MIT
#pragma once
// Игровой цикл и главное меню — порт класса Game.
// Game не знает, как устроен уровень: он загружает LevelData, создаёт Level
// и сохраняет прогресс после прохождения.

#include <string>
#include <vector>

#include "LevelStore.h"
#include "Player.h"

class Game {
public:
    Game(std::string dataDir, Player player);
    void run();

private:
    std::string dataDir_;
    Player player_;
    std::vector<int> levels_;

    void reloadLevels();
    bool isKnownLevel(int n) const;
    bool loadLevel(int number, LevelData& out) const;

    void level(int n);
    void play();
    void last();
    void next();
    void randomLevel(int difficulty);
    void stats() const;

    void help() const;
    void menu() const;
};
