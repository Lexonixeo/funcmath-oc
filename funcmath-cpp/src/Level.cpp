#include "Level.h"

#include <algorithm>
#include <iostream>

#include "Helper.h"
#include "Solver.h"

namespace {

bool take(std::vector<Num>& v, Num value) {
    auto it = std::find(v.begin(), v.end(), value);
    if (it == v.end()) return false;
    v.erase(it);
    return true;
}

}  // namespace

Level::Level(LevelData data, bool tutorial) : data_(std::move(data)), tutorial_(tutorial) {
    numbers_ = data_.numbers;
    original_ = data_.numbers;
    stack_.push_back(numbers_);
}

const Fn* Level::findFunction(const std::string& name) const {
    for (const Fn& f : data_.functions)
        if (f.name == name) return &f;
    return nullptr;
}

void Level::printNumbers() const {
    std::cout << "У вас есть ограниченный набор чисел:" << std::endl;
    for (Num n : numbers_) std::cout << n << " ";
    std::cout << std::endl << std::endl;
}

void Level::printFunctions() const {
    std::cout << "У вас есть функции:" << std::endl;
    for (const Fn& f : data_.functions) std::cout << f.description << std::endl;
    std::cout << std::endl;
}

void Level::printHeader() const {
    std::cout << data_.title << std::endl;
    printNumbers();
    printFunctions();
    std::cout << "Вам нужно из данных чисел, используя данные функции, получить число "
              << data_.answer << "." << std::endl << std::endl;
}

void Level::printTutorial() {
    std::cout << "Используя функцию над двумя числами, вы забираете эти два числа из набора и в набор добавляется результат функции." << std::endl;
    std::cout << "Запись выражения происходит так:   func a b    / это выполнение функции func над параметрами a и b." << std::endl;
    std::cout << "Пример одного из выражений:        sum 1 3     / удалит из набора числа 1 и 3 и добавит в него 4." << std::endl;
    std::cout << "Обратите внимание: скобки и запятые в выражении должны отсутствовать.\n" << std::endl;
    std::cout << "Введите help, чтобы узнать полный список команд." << std::endl;
    std::cout << "Введите clear, чтобы очистить экран от лишних выражений." << std::endl;
    std::cout << "Введите nums, чтобы узнать текущий набор чисел." << std::endl;
    std::cout << "Введите history, чтобы увидеть историю ходов." << std::endl;
    std::cout << "Введите reset, чтобы перезагрузить уровень." << std::endl;
    std::cout << "Введите exit, чтобы выйти из уровня.\n" << std::endl;
    tutorial_ = false;
}

void Level::printHelp() {
    std::cout << "Используя функцию над двумя числами, вы забираете эти два числа из набора и в набор добавляется результат функции." << std::endl;
    std::cout << "Запись выражения происходит так:   func a b    / это выполнение функции func над параметрами a и b." << std::endl;
    std::cout << "Пример одного из выражений:        sum 1 3     / удалит из набора числа 1 и 3 и добавит в него 4." << std::endl;
    std::cout << "Обратите внимание: скобки и запятые в выражении должны отсутствовать.\n" << std::endl;
    std::cout << "Введите help, чтобы узнать полный список команд." << std::endl;
    std::cout << "Введите clear, чтобы очистить экран от лишних выражений." << std::endl;
    std::cout << "Введите nums, чтобы узнать текущий набор чисел." << std::endl;
    std::cout << "Введите history, чтобы увидеть историю ходов." << std::endl;
    std::cout << "Введите hint, чтобы получить подсказку." << std::endl;
    std::cout << "Введите calc {выражение}, чтобы посчитать выражение, не меняя набор." << std::endl;
    std::cout << "Введите back, чтобы откатить предыдущее выражение." << std::endl;
    std::cout << "Введите reset, чтобы перезагрузить уровень." << std::endl;
    std::cout << "Введите clear (или cls), чтобы очистить экран." << std::endl;
    std::cout << "Введите exit, чтобы выйти из уровня.\n" << std::endl;
    tutorial_ = true;
}

void Level::start() {
    helper::clearScreen();
    printHeader();
    if (tutorial_) printTutorial();
}

void Level::nums() const {
    std::cout << "=";
    for (Num n : numbers_) std::cout << " " << n;
    std::cout << std::endl;
}

void Level::printHistory() const {
    if (history_.empty()) {
        std::cout << "(пока нет ходов)" << std::endl;
        return;
    }
    for (const std::string& h : history_) std::cout << h << std::endl;
}

void Level::restart() {
    numbers_ = original_;
    stack_.clear();
    stack_.push_back(numbers_);
    history_.clear();
    helper::clearScreen();
    start();
}

void Level::goBack() {
    if (stack_.size() <= 1) {
        std::cout << "= Нет хода назад!" << std::endl;
        return;
    }
    stack_.pop_back();
    numbers_ = stack_.back();
    if (!history_.empty()) history_.pop_back();
    nums();
}

bool Level::multisetContains(const std::vector<Num>& args) const {
    std::vector<Num> pool = numbers_;
    for (Num a : args)
        if (!take(pool, a)) return false;
    return true;
}

void Level::useFunction(const std::vector<std::string>& words) {
    const std::string& name = words[0];
    const Fn* function = findFunction(name);
    if (function == nullptr) return;
    int args_count = function->numberOfArgs;

    if (static_cast<int>(words.size()) - 1 < args_count) {
        std::cout << "= Недостаточно аргументов! Введите выражение заново." << std::endl;
        return;
    }
    if (static_cast<int>(words.size()) - 1 > args_count) {
        std::cout << "= Избыток аргументов! Введите выражение заново." << std::endl;
        return;
    }

    std::vector<Num> args(args_count);
    for (int i = 0; i < args_count; ++i) {
        if (!helper::parseInt(words[i + 1], args[i])) {
            std::cout << "= Неверные аргументы: должны быть числа! Введите выражение заново." << std::endl;
            return;
        }
    }
    if (!multisetContains(args)) {
        std::cout << "= Неверные аргументы: какие-то числа не существуют в наборе! Введите выражение заново." << std::endl;
        return;
    }

    std::vector<Num> pool = numbers_;
    for (Num a : args) take(pool, a);
    numbers_ = pool;

    Num result = function->use(args);
    numbers_.push_back(result);
    std::cout << "= " << result << "   (в наборе:";
    for (Num v : numbers_) std::cout << " " << v;
    std::cout << ")" << std::endl;

    std::string record = name;
    for (Num v : args) record += " " + std::to_string(v);
    record += " = " + std::to_string(result);
    history_.push_back(record);

    completed_ = (result == data_.answer);
    stack_.push_back(numbers_);
}

void Level::calc(const std::vector<std::string>& words) {
    const std::string& name = words[1];
    const Fn* function = findFunction(name);
    if (function == nullptr) {
        std::cout << "= Такой функции у нас нет :(" << std::endl;
        return;
    }
    int args_count = function->numberOfArgs;

    if (static_cast<int>(words.size()) - 2 < args_count) {
        std::cout << "= Недостаточно аргументов! Введите выражение заново." << std::endl;
        return;
    }
    if (static_cast<int>(words.size()) - 2 > args_count) {
        std::cout << "= Избыток аргументов! Введите выражение заново." << std::endl;
        return;
    }

    std::vector<Num> args(args_count);
    for (int i = 0; i < args_count; ++i) {
        if (!helper::parseInt(words[i + 2], args[i])) {
            std::cout << "= Неверные аргументы: должны быть числа! Введите выражение заново." << std::endl;
            return;
        }
    }
    Num result = function->use(args);
    std::cout << "= " << result << " (без изменения набора)" << std::endl;
}

void Level::hint() {
    std::vector<solver::Move> moves;
    if (solver::solve(numbers_, data_.functions, data_.answer, moves)) {
        if (moves.empty()) {
            std::cout << "= Нужное число уже есть в наборе!" << std::endl;
            return;
        }
        const solver::Move& m = moves.front();
        const Fn* f = findFunction(m.fn);
        const bool random = f != nullptr && !f->isStable();
        std::cout << "= Подсказка: попробуйте  " << m.fn << " " << m.a << " " << m.b;
        if (random)
            std::cout << "   (результат случайный — может понадобиться несколько попыток)";
        else
            std::cout << "   (получится " << m.result << ")";
        std::cout << std::endl;
    } else {
        std::cout << "= Подсказку найти не удалось. Можно вернуться назад командой back или перезапустить уровень (reset)." << std::endl;
    }
}

int Level::turn() {
    // Если нужное число уже в наборе (например, после back/reset или в
    // начальном наборе), уровень сразу пройден — как в старом прототипе.
    if (std::find(numbers_.begin(), numbers_.end(), data_.answer) != numbers_.end()) {
        completed_ = true;
        return 0;
    }
    std::cout << "[Level] Введите выражение: ";
    std::string line;
    if (!std::getline(std::cin, line)) {
        std::cout << std::endl;
        return -1;
    }
    std::vector<std::string> words = helper::wordsFromString(line);
    const std::string& first = words[0];

    if (first.empty()) {
        return 0;
    } else if (first == "exit") {
        return -1;
    } else if (first == "nums") {
        nums();
    } else if (first == "history") {
        printHistory();
    } else if (first == "reset") {
        restart();
    } else if (first == "help") {
        start();
        printHelp();
    } else if (first == "clear" || first == "cls") {
        start();
    } else if (first == "hint") {
        hint();
    } else if (first == "calc") {
        if (words.size() < 2) {
            std::cout << "= Недостаточно аргументов! Введите выражение заново." << std::endl;
        } else if (findFunction(words[1]) == nullptr) {
            std::cout << "= Такой функции у нас нет :(" << std::endl;
        } else {
            calc(words);
        }
    } else if (first == "back") {
        goBack();
    } else if (findFunction(first) == nullptr) {
        std::cout << "= Такой функции/команды у нас нет :( Введите help, чтобы вспомнить существующие команды." << std::endl;
    } else {
        useFunction(words);
    }
    return 0;
}

LevelResult Level::play() {
    start();
    while (!completed_) {
        int check = turn();
        if (check == -1) break;
    }
    if (completed_) {
        std::cout << "Поздравляем! Вы получили число " << data_.answer << " и прошли уровень!" << std::endl;
    }
    return LevelResult{tutorial_, completed_};
}
