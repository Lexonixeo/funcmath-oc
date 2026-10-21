// Game.cpp
// ---------
// Верхнеуровневый цикл: главное меню, выбор уровня, сохранение прогресса.
// Game не занимается разбором выражений — это ответственность Level.
#include "Game.h"

#include <algorithm>
#include <iostream>

#include "Functions.h"
#include "Helper.h"
#include "Level.h"
#include "LevelGenerator.h"

Game::Game(std::string dataDir, Player player)
    : dataDir_(std::move(dataDir)), player_(std::move(player)) {
    reloadLevels();
}

void Game::reloadLevels() { levels_ = levelstore::listLevelNumbers(dataDir_); }

bool Game::isKnownLevel(int n) const {
    return std::find(levels_.begin(), levels_.end(), n) != levels_.end();
}

bool Game::loadLevel(int number, LevelData& out) const {
    std::string error;
    if (!levelstore::load(levelstore::levelPath(dataDir_, number), out, error)) {
        std::cout << "Ошибка загрузки уровня: " << error << std::endl;
        return false;
    }
    // Номер и заголовок задаёт игра, а не файл: один и тот же .lvl можно
    // использовать как случайный/временный уровень.
    out.number = number;
    out.title = "Уровень " + std::to_string(number) + ".";
    return true;
}

void Game::rate(int level) {
    std::cout << "Оцените сложность данного уровня от 1 до 10 (введите 0 если не хотите отвечать): ";
    std::string line;
    if (!std::getline(std::cin, line)) return;
    long long rate;
    if (!helper::parseInt(helper::trim(line), rate)) {
        std::cout << "Спасибо!" << std::endl;
        return;
    }
    if (1 <= rate && rate <= 10)
        helper::appendLine(dataDir_ + "/lvlrate.dat", std::to_string(level) + " " + std::to_string(rate));
    std::cout << "Спасибо!" << std::endl;
}

void Game::level(int n) {
    // Сначала фиксируем lastLevel: если игрок выйдет из уровня через exit,
    // команда next/last всё равно будет знать, где он был.
    if (!isKnownLevel(n)) {
        std::cout << "Такого уровня не существует!" << std::endl;
        return;
    }
    player_.lastLevel = n;
    playerstore::save(dataDir_, player_);

    LevelData data;
    if (!loadLevel(n, data)) return;

    Level l(data, player_.tutorial);
    LevelResult result = l.play();
    player_.tutorial = result.tutorial;
    if (result.completed) player_.completed.insert(n);
    playerstore::save(dataDir_, player_);

    rate(n);
}

void Game::play() {
    // «Продолжить» — первый непройденный уровень в отсортированном списке.
    for (int lvl : levels_) {
        if (player_.completed.find(lvl) == player_.completed.end()) {
            level(lvl);
            return;
        }
    }
    std::cout << "Вы прошли все уровни!" << std::endl;
}

void Game::last() {
    if (player_.lastLevel == -1) {
        std::cout << "Вы ещё не заходили в какой-либо уровень!" << std::endl;
        return;
    }
    level(player_.lastLevel - 1);
}

void Game::next() {
    if (player_.lastLevel == -1) {
        std::cout << "Вы ещё не заходили в какой-либо уровень!" << std::endl;
        return;
    }
    level(player_.lastLevel + 1);
}

void Game::randomLevel(int difficulty) {
    // Случайный уровень не сохраняется в data/levels и не входит в статистику
    // пройденных; сохраняется только lastLevel/tutorial.
    if (difficulty < 1) {
        std::cout << "Неверно указана сложность!" << std::endl;
        return;
    }
    LevelData data = levelgen::generateRandom(difficulty);
    Level l(data, player_.tutorial);
    LevelResult result = l.play();
    player_.tutorial = result.tutorial;
    playerstore::save(dataDir_, player_);
}

void Game::stats() const {
    // Оценки читаются из общего lvlrate.dat, поэтому средняя оценка —
    // по всем игрокам, а не только по текущему.
    int total = static_cast<int>(levels_.size());
    int done = static_cast<int>(player_.completed.size());
    std::cout << "=== Статистика игрока " << player_.name << " ===" << std::endl;
    std::cout << "Пройдено уровней: " << done << " из " << total;
    if (total > 0) std::cout << " (" << (done * 100 / total) << "%)";
    std::cout << std::endl;
    std::cout << "Последний уровень: " << (player_.lastLevel == -1 ? "нет" : std::to_string(player_.lastLevel)) << std::endl;

    std::string ratings = helper::readFile(dataDir_ + "/lvlrate.dat");
    int ratingCount = 0;
    long long ratingSum = 0;
    for (const std::string& raw : helper::splitLines(ratings)) {
        std::vector<std::string> w = helper::wordsFromString(helper::trim(raw));
        if (w.size() < 2) continue;
        long long r;
        if (helper::parseInt(w[1], r)) {
            ratingCount++;
            ratingSum += r;
        }
    }
    if (ratingCount > 0)
        std::cout << "Средняя оценка сложности: " << (static_cast<double>(ratingSum) / ratingCount)
                  << " (оценок: " << ratingCount << ")" << std::endl;

    std::cout << "Пройденные уровни:";
    if (player_.completed.empty()) std::cout << " (нет)";
    for (int lvl : player_.completed) std::cout << " " << lvl;
    std::cout << std::endl;
}

void Game::help() const {
    helper::clearScreen();
    std::cout << "Список команд:" << std::endl;
    std::cout << "menu          - Перейти в главное меню" << std::endl;
    std::cout << "help          - Узнать список команд" << std::endl;
    std::cout << "play          - Начать/продолжить игру" << std::endl;
    std::cout << "level {lvl}   - Перейти к уровню {lvl}" << std::endl;
    std::cout << "random {dfc}  - Играть в случайный уровень сложности {dfc} (от 1)" << std::endl;
    std::cout << "last          - Перейти к предыдущему уровню" << std::endl;
    std::cout << "next          - Перейти к следующему уровню" << std::endl;
    std::cout << "stats         - Узнать свою статистику" << std::endl;
    std::cout << "exit          - Выйти из игры" << std::endl;
}

void Game::menu() const {
    helper::clearScreen();
    int total = static_cast<int>(levels_.size());
    std::cout << "Добро пожаловать, " << player_.name << "!" << std::endl;
    std::cout << "Вы сейчас играете во второй консольный прототип игры func(math)." << std::endl;
    std::cout << "Он написан для того, чтобы реализовать геймплей игры на C++ и узнать, может ли она вам понравиться." << std::endl;
    std::cout << std::endl;
    std::cout << "Вы находитесь в главном меню." << std::endl;
    std::cout << "Игра начинается с 1-ого уровня. Всего в игре " << total << " уровней." << std::endl;
    std::cout << "Чтобы " << (player_.completed.empty() ? "начать игру" : "продолжить игру с первого непройденного уровня") << ", введите команду play." << std::endl;
    std::cout << "Чтобы играть в конкретный уровень, введите команду level {номер уровня}." << std::endl;
    std::cout << "Чтобы узнать список команд, введите команду help." << std::endl;
    std::cout << "Чтобы выйти из игры, введите команду exit." << std::endl;
}

void Game::run() {
    // Прогресс сохраняется перед каждым приглашением: так он переживает
    // аварийное завершение и EOF в перенаправленном stdin.
    helper::clearScreen();
    menu();
    while (true) {
        playerstore::save(dataDir_, player_);
        std::cout << std::endl;
        std::cout << "[Commander] Введите команду: ";
        std::string line;
        if (!std::getline(std::cin, line)) break;
        std::vector<std::string> command = helper::wordsFromString(line);
        const std::string& first = command[0];

        if (first.empty()) {
            continue;
        } else if (first == "menu") {
            menu();
        } else if (first == "help") {
            help();
        } else if (first == "play") {
            play();
        } else if (first == "level") {
            if (command.size() > 2) {
                std::cout << "Избыток аргументов! Введите команду ещё раз." << std::endl;
            } else if (command.size() == 1) {
                std::cout << "Недостаточно аргументов! Введите команду ещё раз." << std::endl;
            } else {
                long long lvl;
                if (!helper::parseInt(command[1], lvl)) {
                    std::cout << "Неверный аргумент: должно быть число!" << std::endl;
                } else {
                    level(static_cast<int>(lvl));
                }
            }
        } else if (first == "random") {
            if (command.size() < 2) {
                std::cout << "Укажите сложность: random {dfc}" << std::endl;
            } else {
                long long d;
                if (!helper::parseInt(command[1], d)) {
                    std::cout << "Неверный аргумент: должно быть число!" << std::endl;
                } else {
                    randomLevel(static_cast<int>(d));
                }
            }
        } else if (first == "last") {
            last();
        } else if (first == "next") {
            next();
        } else if (first == "stats") {
            stats();
        } else if (first == "exit") {
            break;
        } else {
            std::cout << "Неизвестная команда :( Введите help, чтобы узнать список команд." << std::endl;
        }
    }
}
