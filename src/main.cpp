// func(math) — консольная игра.
// Порт Java-версии (github.com/Lexonixeo/funcmathjava1) на C++17.
//
// Режимы запуска:
//   funcmath            обычная игра
//   funcmath --author   интерактивное создание нового уровня
//   funcmath --check     проверить, что все уровни решаемы

#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "Functions.h"
#include "Game.h"
#include "Helper.h"
#include "LevelGenerator.h"
#include "LevelStore.h"
#include "Player.h"
#include "Solver.h"

namespace {

int runCheck(const std::string& dataDir, int onlyLevel, long long nodeLimit,
             long long timeLimitMs) {
    std::vector<int> levels = levelstore::listLevelNumbers(dataDir);
    if (onlyLevel > 0) {
        if (std::find(levels.begin(), levels.end(), onlyLevel) != levels.end())
            levels = {onlyLevel};
        else
            levels.clear();
    }
    std::cout << "Проверка " << levels.size() << " уровней..." << std::endl;
    int solvable = 0, unsolvable = 0, randomLevels = 0, broken = 0;
    for (int n : levels) {
        LevelData data;
        std::string error;
        if (!levelstore::load(levelstore::levelPath(dataDir, n), data, error)) {
            std::cout << "  уровень " << n << ": ОШИБКА (" << error << ")" << std::endl;
            broken++;
            continue;
        }
        // Уровни со случайными функциями тоже проверяются: решатель перебирает
        // все возможные исходы (см. fmath::outcomes).
        const bool random = solver::hasUnstable(data.functions);
        if (random) randomLevels++;
        const char* mark = random ? " (случайные функции)" : "";

        std::vector<solver::Move> moves;
        auto t0 = std::chrono::steady_clock::now();
        bool found = solver::solve(data.numbers, data.functions, data.answer, moves,
                                   nodeLimit, timeLimitMs);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now() - t0)
                      .count();
        if (found) {
            std::cout << "  уровень " << n << mark << ": решаем за " << moves.size()
                      << " ход(ов) (" << ms << " мс)" << std::endl;
            solvable++;
        } else {
            std::cout << "  уровень " << n << mark << ": решение не найдено за " << ms
                      << " мс (лимит или уровень нерешаем!)" << std::endl;
            unsolvable++;
        }
    }
    std::cout << "Итого: решаемо " << solvable << ", не найдено " << unsolvable
              << ", со случайностью " << randomLevels << ", с ошибками " << broken << "." << std::endl;
    return (unsolvable == 0 && broken == 0) ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);

    bool author = false, check = false;
    int onlyLevel = -1;
    long long nodeLimit = 200000, timeLimitMs = 1000;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        long long v;
        if (a == "--author") {
            author = true;
        } else if (a == "--check") {
            check = true;
        } else if (a == "--nodes" && i + 1 < args.size() && helper::parseInt(args[++i], v)) {
            nodeLimit = v;
        } else if (a == "--time" && i + 1 < args.size() && helper::parseInt(args[++i], v)) {
            timeLimitMs = v;
        } else if (helper::parseInt(a, v)) {
            onlyLevel = static_cast<int>(v);
        }
    }

    helper::initConsole();

    std::string dataDir = helper::resolveDataDir();
    fmath::loadDescriptions(dataDir + "/functions.txt");

    if (check) return runCheck(dataDir, onlyLevel, nodeLimit, timeLimitMs);
    if (author) {
        levelgen::authorLevel(dataDir);
        return 0;
    }

    helper::clearScreen();
    Player player = playerstore::login(dataDir);
    Game game(dataDir, std::move(player));
    game.run();
    return 0;
}
