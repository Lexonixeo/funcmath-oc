// SPDX-License-Identifier: MIT
// LevelGenerator.cpp
// ------------------
// Две независимые вещи:
//   * generateRandom — случайный, но гарантированно решаемый уровень;
//   * authorLevel / functionEditor — интерактивные редакторы для человека.
//
// Генератор не вызывает решатель: он строит ответ реальным применением
// функций к числам, поэтому достижимость гарантирована по построению.
#include "LevelGenerator.h"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <iostream>
#include <random>

#include "Functions.h"
#include "Helper.h"

namespace levelgen {

namespace {

const char* unknownNames[] = {"A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L"};

int randInt(int lo, int hi) {
    if (lo > hi) std::swap(lo, hi);
    return static_cast<int>(fmath::randBetween(lo, hi));
}

bool parseArgument(const std::string& token, Num& value, std::string& error) {
    // Аргументы --function test: "6" (integer) или "natural=6".
    // Для выбора natural-перегрузки нужен явный тип.
    std::size_t eq = token.find('=');
    if (eq != std::string::npos) {
        ValueType type;
        if (!valueTypeFromName(token.substr(0, eq), type)) {
            error = "неизвестный тип в литерале: " + token;
            return false;
        }
        long long number;
        if (!helper::parseInt(token.substr(eq + 1), number)) {
            error = "некорректное число в литерале: " + token;
            return false;
        }
        try {
            value = Value::of(type, number);
        } catch (const std::exception& e) {
            error = e.what();
            return false;
        }
        return true;
    }
    long long number;
    if (!helper::parseInt(token, number)) {
        error = "аргумент должен быть числом: " + token;
        return false;
    }
    value = Value::integer(number);
    return true;
}

void printFunctionHelp() {
    std::cout << "Команды редактора функции:\n"
              << "  name <имя>                 имя функции\n"
              << "  description <текст>        описание для игрока\n"
              << "  in <имя>: <тип>            вход (natural/integer)\n"
              << "  out <имя>: <тип>           выход; тип с ... — поток\n"
              << "  use <алиас> = <функция> <типы> -> <результаты>\n"
              << "  body <присваивание>        добавить строку тела\n"
              << "  body                       читать тело до строки end\n"
              << "  list                       показать блок\n"
              << "  validate                   проверить блок\n"
              << "  test <аргументы>           вычислить функцию\n"
              << "  save                       сохранить в data/functions/<имя>.fn\n"
              << "  clear                      начать заново\n"
              << "  exit                       выйти\n";
}

std::string restOf(const std::string& line, const std::string& key) {
    std::size_t pos = line.find(key);
    if (pos == std::string::npos) return {};
    return helper::trim(line.substr(pos + key.size()));
}

}  // namespace

LevelData generateRandom(int difficulty) {
    // Сложность управляет количеством чисел, диапазоном значений, числом
    // функций и вероятностью спрятать функцию под именем A/B/C...
    if (difficulty < 1) difficulty = 1;

    const auto& pool = fmath::generationPool();

    // 1. Набор чисел.
    int count = std::max(2, difficulty + randInt(0, std::max(0, difficulty - 1)));
    std::vector<Num> numbers(count);
    int bound = 20 * difficulty;
    for (Num& x : numbers) x = Value::integer(randInt(-bound, bound));

    // 2. Набор функций.
    int want = std::max(1, difficulty / 3) + randInt(0, 1 + (difficulty + 1) / 2);
    want = std::min<int>(want, static_cast<int>(pool.size()));
    want = std::max(1, want);

    std::vector<int> idx(pool.size());
    for (int i = 0; i < static_cast<int>(idx.size()); ++i) idx[i] = i;
    std::shuffle(idx.begin(), idx.end(), std::mt19937(std::random_device{}()));
    std::vector<std::string> chosen;
    for (int i = 0; i < want; ++i) chosen.push_back(pool[idx[i]]);

    // 3. Иногда прячем функцию под именем A, B, C... (начиная со сложности 3).
    std::vector<Fn> functions;
    int hidden = 0;
    for (const std::string& behavior : chosen) {
        bool hide = difficulty >= 3 && (randInt(0, 99) < std::min(60, 10 + difficulty * 5)) &&
                    hidden < static_cast<int>(sizeof(unknownNames) / sizeof(unknownNames[0]));
        if (hide) {
            functions.push_back(makeUnknownFunction(unknownNames[hidden], behavior));
            hidden++;
        } else {
            functions.push_back(makeKnownFunction(behavior));
        }
    }
    std::sort(functions.begin(), functions.end(),
              [](const Fn& a, const Fn& b) { return a.name < b.name; });

    // 4. Ответ: реально применяем функции к числам, поэтому он достижим.
    // Пробуем несколько раз, чтобы ответ не совпал с одним из исходных
    // чисел (иначе уровень решался бы без ходов).
    Num answer = Value::integer(0);
    bool ok = false;
    for (int attempt = 0; attempt < 200 && !ok; ++attempt) {
        std::vector<Num> work = numbers;
        int steps = randInt(1, std::max(1, count - 1));
        for (int s = 0; s < steps && !work.empty(); ++s) {
            int n = static_cast<int>(work.size());
            const Fn& f = functions[randInt(0, static_cast<int>(functions.size()) - 1)];
            if (!f.isStable()) continue;
            const int arity = std::max(1, f.numberOfArgs);
            if (n < arity) continue;

            // Выбираем arity различных чисел и передаём их функции. Так
            // генератор работает и с унарными abs/not, и с обычными
            // бинарными операциями.
            std::vector<int> order(n);
            for (int k = 0; k < n; ++k) order[k] = k;
            std::vector<int> indices;
            std::vector<Num> args;
            indices.reserve(arity);
            args.reserve(arity);
            for (int k = 0; k < arity; ++k) {
                int pick = randInt(k, n - 1);
                std::swap(order[k], order[pick]);
                indices.push_back(order[k]);
                args.push_back(work[order[k]]);
            }

            std::vector<Num> results;
            try {
                // Генератор использует обычный (не allOutcomes) вызов:
                // фиксируем один случайный исход и делаем его целью.
                results = f.use(args);
            } catch (...) {
                continue;
            }
            if (results.empty()) continue;
            Num c = results.front();

            std::vector<bool> removed(n, false);
            for (int index : indices) removed[index] = true;
            std::vector<Num> next;
            next.reserve(n - arity + 1);
            for (int k = 0; k < n; ++k)
                if (!removed[k]) next.push_back(work[k]);
            next.push_back(c);
            work = next;
        }
        answer = work[randInt(0, static_cast<int>(work.size()) - 1)];
        ok = std::find(numbers.begin(), numbers.end(), answer) == numbers.end();
    }
    if (!ok) {
        // крайне маловероятный запасной вариант
        answer = Value::integer(numbers[0].asLongLong() + numbers[1].asLongLong());
    }

    LevelData data;
    data.number = -1;
    data.title = "Случайный уровень сложности " + std::to_string(difficulty) + ".";
    data.numbers = numbers;
    data.functions = functions;
    data.answer = answer;
    return data;
}

void authorLevel(const std::string& dataDir) {
    // Простой мастер: тип, числа, известные/неизвестные функции, ответ.
    // Он не проверяет решаемость — для этого есть --check.
    std::cout << "=== Создание нового уровня ===" << std::endl;
    std::cout << "Доступные функции:";
    for (const auto& kv : fmath::registry()) std::cout << " " << kv.first;
    std::cout << std::endl;

    std::string line;
    std::cout << "Тип чисел (integer/natural) [integer]: ";
    std::getline(std::cin, line);
    line = helper::trim(line);
    ValueType type = ValueType::Integer;
    if (!line.empty() && !valueTypeFromName(line, type)) {
        std::cout << "Неизвестный тип. Отмена." << std::endl;
        return;
    }

    std::cout << "Введите числа в наборе: ";
    std::getline(std::cin, line);
    bool ok = false;
    std::vector<long long> numsLL = helper::numbersFromWords(helper::wordsFromString(line), ok);
    if (!ok || numsLL.empty()) {
        std::cout << "Некорректные числа." << std::endl;
        return;
    }
    if (type == ValueType::Natural) {
        for (long long value : numsLL) {
            if (value < 0) {
                std::cout << "Natural не может быть отрицательным. Отмена." << std::endl;
                return;
            }
        }
    }

    std::cout << "Введите названия известных функций (через пробел, - если нет): ";
    std::getline(std::cin, line);
    std::vector<std::string> known = helper::wordsFromString(line);
    if (!known.empty() && known[0] == "-") known.clear();

    std::vector<Fn> functions;
    for (const std::string& name : known) {
        if (!fmath::hasFunction(name)) {
            std::cout << "Предупреждение: функции '" << name << "' нет, пропускаю." << std::endl;
            continue;
        }
        functions.push_back(makeKnownFunction(name));
    }

    std::cout << "Введите количество неизвестных функций: ";
    std::getline(std::cin, line);
    long long n = 0;
    if (!helper::parseInt(helper::trim(line), n) || n < 0) n = 0;
    for (long long i = 0; i < n && i < 12; ++i) {
        std::cout << "Какую известную функцию выполняет " << unknownNames[i] << "? ";
        std::getline(std::cin, line);
        std::vector<std::string> w = helper::wordsFromString(line);
        if (w.empty() || !fmath::hasFunction(w[0])) {
            std::cout << "Некорректно, пропускаю " << unknownNames[i] << "." << std::endl;
            continue;
        }
        functions.push_back(makeUnknownFunction(unknownNames[i], w[0]));
    }

    if (functions.empty()) {
        std::cout << "Нужна хотя бы одна функция. Отмена." << std::endl;
        return;
    }

    std::cout << "Введите ответ: ";
    std::getline(std::cin, line);
    long long answer;
    if (!helper::parseInt(helper::trim(line), answer)) {
        std::cout << "Некорректный ответ. Отмена." << std::endl;
        return;
    }
    if (type == ValueType::Natural && answer < 0) {
        std::cout << "Natural-ответ не может быть отрицательным. Отмена." << std::endl;
        return;
    }

    std::sort(functions.begin(), functions.end(),
              [](const Fn& a, const Fn& b) { return a.name < b.name; });

    LevelData data;
    data.number = levelstore::nextLevelNumber(dataDir);
    data.title = "Уровень " + std::to_string(data.number) + ".";
    data.numbers.clear();
    for (long long value : numsLL) data.numbers.push_back(Value::of(type, value));
    data.functions = functions;
    data.answer = Value::of(type, answer);

    std::string path = levelstore::levelPath(dataDir, data.number);
    if (levelstore::save(path, data)) {
        std::cout << "Уровень " << data.number << " сохранён: " << path << std::endl;
    } else {
        std::cout << "Не удалось сохранить уровень." << std::endl;
    }
}

void functionEditor(const std::string& dataDir) {
    // REPL для FunctionBlock: команды накапливают сырой блок, validate/test
    // прогоняют его через тот же compileFunctionBlocks, что и .lvl, а save
    // пишет data/functions/<имя>.fn.
    std::cout << "=== Редактор пользовательской функции ===" << std::endl;
    printFunctionHelp();

    FunctionBlock block;
    std::string line;
    while (true) {
        std::cout << "fn> ";
        if (!std::getline(std::cin, line)) break;
        line = helper::trim(line);
        if (line.empty()) continue;
        std::vector<std::string> words = helper::wordsFromString(line);
        const std::string& command = words[0];

        if (command == "exit" || command == "quit") {
            break;
        } else if (command == "help") {
            printFunctionHelp();
        } else if (command == "name") {
            if (words.size() < 2) {
                std::cout << "Нужно имя функции." << std::endl;
            } else {
                block.name = words[1];
            }
        } else if (command == "description") {
            block.description = restOf(line, command);
        } else if (command == "in") {
            if (words.size() != 3) {
                std::cout << "Ожидается: in <имя>: <тип>" << std::endl;
                continue;
            }
            ParamDef param;
            param.name = words[1];
            if (!param.name.empty() && param.name.back() == ':')
                param.name.pop_back();
            if (!valueTypeFromName(words[2], param.type)) {
                std::cout << "Неизвестный тип: " << words[2] << std::endl;
                continue;
            }
            block.params.push_back(param);
        } else if (command == "out") {
            if (words.size() != 3) {
                std::cout << "Ожидается: out <имя>: <тип>" << std::endl;
                continue;
            }
            OutputDef output;
            output.name = words[1];
            if (!output.name.empty() && output.name.back() == ':')
                output.name.pop_back();
            std::string error;
            if (!parseResultType(words[2], output.spec, error)) {
                std::cout << error << std::endl;
                continue;
            }
            block.outputs.push_back(output);
        } else if (command == "use") {
            block.useLines.push_back(restOf(line, command));
        } else if (command == "body") {
            std::string rest = restOf(line, command);
            if (!rest.empty()) {
                block.bodyLines.push_back(rest);
            } else {
                // Многострочный режим: end завершает ввод тела, но не
                // редактор. Пустые строки в тело не попадают.
                std::cout << "Вводите строки тела, пока не введёте end." << std::endl;
                while (true) {
                    std::cout << "body> ";
                    if (!std::getline(std::cin, line)) break;
                    std::string body = helper::trim(line);
                    if (body == "end") break;
                    if (!body.empty()) block.bodyLines.push_back(body);
                }
            }
        } else if (command == "list") {
            if (block.name.empty() && block.params.empty() && block.outputs.empty() &&
                block.bodyLines.empty()) {
                std::cout << "(пусто)" << std::endl;
            } else {
                std::cout << formatFunctionBlock(block);
            }
        } else if (command == "clear") {
            block = FunctionBlock{};
            std::cout << "Блок очищен." << std::endl;
        } else if (command == "validate") {
            std::string error;
            if (validateFunctionBlock(block, error))
                std::cout << "OK" << std::endl;
            else
                std::cout << "Ошибка: " << error << std::endl;
        } else if (command == "test") {
            std::vector<Num> args;
            std::string error;
            bool ok = true;
            for (std::size_t i = 1; i < words.size(); ++i) {
                Num value;
                if (!parseArgument(words[i], value, error)) {
                    ok = false;
                    break;
                }
                args.push_back(value);
            }
            if (!ok) {
                std::cout << "Ошибка: " << error << std::endl;
                continue;
            }
            std::vector<Fn> functions;
            std::vector<FunctionBlock> blocks = {block};
            // test компилирует временную функцию вместе с встроенными; так
            // проверяются те же ограничения, что и при загрузке уровня.
            if (!compileFunctionBlocks(blocks, functions, error)) {
                std::cout << "Ошибка: " << error << std::endl;
                continue;
            }
            const Fn* function = nullptr;
            for (const Fn& candidate : functions)
                if (candidate.isCustom()) function = &candidate;
            if (function == nullptr) {
                std::cout << "Ошибка: функция не собрана." << std::endl;
                continue;
            }
            try {
                std::vector<Num> results = function->use(args, &functions);
                std::cout << "=";
                if (results.empty()) {
                    std::cout << " (пусто)";
                } else {
                    for (const Num& value : results)
                        std::cout << " " << value.str() << "(" << value.typeName() << ")";
                }
                std::cout << std::endl;
            } catch (const std::exception& e) {
                std::cout << "Ошибка: " << e.what() << std::endl;
            }
        } else if (command == "save") {
            std::string error;
            if (block.name.empty()) {
                std::cout << "Сначала задайте имя." << std::endl;
                continue;
            }
            if (!validateFunctionBlock(block, error)) {
                std::cout << "Ошибка: " << error << std::endl;
                continue;
            }
            std::filesystem::create_directories(dataDir + "/functions");
            // Имя файла — имя функции; сохраняем только валидный блок.
            std::string path = dataDir + "/functions/" + block.name + ".fn";
            if (helper::writeFile(path, formatFunctionBlock(block)))
                std::cout << "Сохранено: " << path << std::endl;
            else
                std::cout << "Не удалось сохранить " << path << std::endl;
        } else {
            std::cout << "Неизвестная команда. Введите help." << std::endl;
        }
    }
}

}  // namespace levelgen
