#include "Level.h"

#include <algorithm>
#include <exception>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "Helper.h"
#include "Solver.h"

namespace {

bool allSameType(const std::vector<Num>& values, ValueType type) {
    for (const Num& value : values)
        if (value.type() != type) return false;
    return true;
}

ValueType defaultType(const std::vector<Num>& values) {
    if (!values.empty() && allSameType(values, values.front().type()))
        return values.front().type();
    return ValueType::Integer;
}

std::string joinWords(const std::vector<std::string>& words, std::size_t from = 0) {
    std::string result;
    for (std::size_t i = from; i < words.size(); ++i) {
        if (!result.empty()) result += " ";
        result += words[i];
    }
    return result;
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
    const bool mixed = !numbers_.empty() &&
                       !(allSameType(numbers_, ValueType::Integer) ||
                         allSameType(numbers_, ValueType::Natural));
    for (const Num& n : numbers_) {
        std::cout << n.str();
        if (mixed) std::cout << "(" << n.typeName() << ")";
        std::cout << " ";
    }
    std::cout << std::endl << std::endl;
}

void Level::printFunctions() const {
    std::cout << "У вас есть функции:" << std::endl;
    for (const Fn& f : data_.functions) std::cout << f.description << std::endl;
    std::cout << std::endl;
}

void Level::printHeader() const {
    std::cout << data_.title << std::endl;
    if (!data_.numbers.empty() && allSameType(data_.numbers, ValueType::Natural)) {
        std::cout << "Тип чисел: natural (неотрицательные)" << std::endl;
    } else if (!data_.numbers.empty() &&
               !allSameType(data_.numbers, ValueType::Integer)) {
        std::cout << "Тип чисел: смешанный" << std::endl;
    }
    printNumbers();
    printFunctions();
    std::cout << "Вам нужно из данных чисел, используя данные функции, получить число "
              << data_.answer.str() << "." << std::endl << std::endl;
}

void Level::printTutorial() {
    std::cout << "Функция забирает из набора свои аргументы и добавляет результаты."
              << std::endl;
    std::cout << "Запись выражения: func a b    / например, sum 1 3." << std::endl;
    std::cout << "Аргументы могут быть вложенными: sum 1 sum 2 3." << std::endl;
    std::cout << "У некоторых функций один аргумент или несколько результатов (primes)."
              << std::endl;
    std::cout << "Обратите внимание: скобки и запятые в выражении должны отсутствовать.\n"
              << std::endl;
    std::cout << "Введите help, чтобы узнать полный список команд." << std::endl;
    std::cout << "Введите clear, чтобы очистить экран от лишних выражений." << std::endl;
    std::cout << "Введите nums, чтобы узнать текущий набор чисел." << std::endl;
    std::cout << "Введите history, чтобы увидеть историю ходов." << std::endl;
    std::cout << "Введите reset, чтобы перезагрузить уровень." << std::endl;
    std::cout << "Введите exit, чтобы выйти из уровня.\n" << std::endl;
    tutorial_ = false;
}

void Level::printHelp() {
    std::cout << "Функция забирает из набора свои аргументы и добавляет результаты."
              << std::endl;
    std::cout << "Запись выражения: func a b    / например, sum 1 3." << std::endl;
    std::cout << "Аргументы могут быть вложенными: sum 1 sum 2 3." << std::endl;
    std::cout << "У некоторых функций один аргумент или несколько результатов (primes)."
              << std::endl;
    std::cout << "Обратите внимание: скобки и запятые в выражении должны отсутствовать.\n"
              << std::endl;
    std::cout << "Введите help, чтобы узнать полный список команд." << std::endl;
    std::cout << "Введите clear, чтобы очистить экран от лишних выражений." << std::endl;
    std::cout << "Введите nums, чтобы узнать текущий набор чисел." << std::endl;
    std::cout << "Введите history, чтобы увидеть историю ходов." << std::endl;
    std::cout << "Введите hint, чтобы получить подсказку." << std::endl;
    std::cout << "Введите calc {выражение}, чтобы посчитать выражение, не меняя набор."
              << std::endl;
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
    for (const Num& n : numbers_) std::cout << " " << n.str();
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
    completed_ = false;
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
    completed_ = false;
    nums();
}

bool Level::evalExpression(const std::vector<std::string>& words, std::size_t& pos,
                           bool consumeFromSet, std::vector<Num>& pool,
                           ValueType literalType, std::vector<Num>& result,
                           std::string& error) const {
    if (pos >= words.size()) {
        error = "недостаточно аргументов!";
        return false;
    }

    const std::string& token = words[pos];
    const Fn* function = findFunction(token);
    if (function != nullptr) {
        ++pos;
        std::vector<Num> args;
        for (int i = 0; i < function->numberOfArgs; ++i) {
            std::vector<Num> part;
            if (!evalExpression(words, pos, consumeFromSet, pool, literalType, part,
                                error))
                return false;
            args.insert(args.end(), part.begin(), part.end());
        }
        if (static_cast<int>(args.size()) != function->numberOfArgs) {
            error = "функция " + function->name + " ожидает " +
                    std::to_string(function->numberOfArgs) + " аргумент(ов), получено " +
                    std::to_string(args.size()) + "!";
            return false;
        }
        try {
            result = function->use(args);
        } catch (const std::exception& e) {
            error = e.what();
            return false;
        }
        return true;
    }

    long long value = 0;
    if (!helper::parseInt(token, value)) {
        error = "не число и не функция: " + token;
        return false;
    }

    if (consumeFromSet) {
        auto it = std::find_if(pool.begin(), pool.end(),
                               [value](const Num& n) { return n.asLongLong() == value; });
        if (it == pool.end()) {
            error = "числа " + token + " нет в наборе!";
            return false;
        }
        result = {*it};
        pool.erase(it);
    } else {
        if (literalType == ValueType::Natural && value < 0) {
            error = "в natural-выражении нельзя использовать отрицательные числа";
            return false;
        }
        result = {Value::of(literalType, value)};
    }
    ++pos;
    return true;
}

void Level::applyExpression(const std::vector<std::string>& words) {
    std::vector<Num> pool = numbers_;
    std::size_t pos = 0;
    std::vector<Num> result;
    std::string error;
    if (!evalExpression(words, pos, true, pool, defaultType(numbers_), result, error)) {
        std::cout << "= " << error << std::endl;
        return;
    }
    if (pos != words.size()) {
        std::cout << "= Лишние аргументы! Введите выражение заново." << std::endl;
        return;
    }

    numbers_ = std::move(pool);
    numbers_.insert(numbers_.end(), result.begin(), result.end());

    std::cout << "=";
    if (result.empty()) {
        std::cout << " (пусто)";
    } else {
        for (const Num& value : result) std::cout << " " << value.str();
    }
    std::cout << "   (в наборе:";
    for (const Num& value : numbers_) std::cout << " " << value.str();
    std::cout << ")" << std::endl;

    std::string record = joinWords(words) + " =";
    if (result.empty()) {
        record += " (пусто)";
    } else {
        for (const Num& value : result) record += " " + value.str();
    }
    history_.push_back(record);

    completed_ = std::find(numbers_.begin(), numbers_.end(), data_.answer) != numbers_.end();
    stack_.push_back(numbers_);
}

void Level::calc(const std::vector<std::string>& words) {
    std::size_t pos = 1;
    std::vector<Num> pool;
    std::vector<Num> result;
    std::string error;
    if (!evalExpression(words, pos, false, pool, defaultType(numbers_), result, error)) {
        std::cout << "= " << error << std::endl;
        return;
    }
    if (pos != words.size()) {
        std::cout << "= Лишние аргументы! Введите выражение заново." << std::endl;
        return;
    }
    std::cout << "=";
    if (result.empty()) {
        std::cout << " (пусто)";
    } else {
        for (const Num& value : result) std::cout << " " << value.str();
    }
    std::cout << " (без изменения набора)" << std::endl;
}

void Level::hint() {
    std::vector<solver::Move> moves;
    if (solver::solve(numbers_, data_.functions, data_.answer, moves)) {
        if (moves.empty()) {
            std::cout << "= Нужное число уже есть в наборе!" << std::endl;
            return;
        }
        const solver::Move& move = moves.front();
        const Fn* function = findFunction(move.fn);
        const bool random = function != nullptr && !function->isStable();
        std::cout << "= Подсказка: попробуйте  " << move.fn;
        for (const Num& arg : move.args) std::cout << " " << arg.str();
        if (random) {
            std::cout << "   (результат случайный — может понадобиться несколько попыток)";
        } else {
            std::cout << "   (получится";
            if (move.results.empty()) {
                std::cout << " пусто";
            } else {
                for (const Num& value : move.results) std::cout << " " << value.str();
            }
            std::cout << ")";
        }
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
        } else {
            calc(words);
        }
    } else if (first == "back") {
        goBack();
    } else if (findFunction(first) == nullptr) {
        std::cout << "= Такой функции/команды у нас нет :( Введите help, чтобы вспомнить существующие команды." << std::endl;
    } else {
        applyExpression(words);
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
        std::cout << "Поздравляем! Вы получили число " << data_.answer.str()
                  << " и прошли уровень!" << std::endl;
    }
    return LevelResult{tutorial_, completed_};
}
