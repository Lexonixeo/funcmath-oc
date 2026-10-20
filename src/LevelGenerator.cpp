#include "LevelGenerator.h"

#include <algorithm>
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

}  // namespace

LevelData generateRandom(int difficulty) {
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
    Num answer = Value::integer(0);
    bool ok = false;
    for (int attempt = 0; attempt < 200 && !ok; ++attempt) {
        std::vector<Num> work = numbers;
        int steps = randInt(1, std::max(1, count - 1));
        for (int s = 0; s < steps && work.size() >= 2; ++s) {
            int n = static_cast<int>(work.size());
            int i = randInt(0, n - 2);
            int j = randInt(i + 1, n - 1);
            Num a = work[i], b = work[j];
            if (randInt(0, 1) == 1) std::swap(a, b);
            const Fn& f = functions[randInt(0, static_cast<int>(functions.size()) - 1)];
            if (!f.isStable()) continue;
            std::vector<Num> results;
            try {
                results = f.use({a, b});
            } catch (...) {
                continue;
            }
            if (results.empty()) continue;
            Num c = results.front();
            // удаляем i и j
            std::vector<Num> next;
            next.reserve(work.size() - 1);
            for (int k = 0; k < n; ++k)
                if (k != i && k != j) next.push_back(work[k]);
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

}  // namespace levelgen
