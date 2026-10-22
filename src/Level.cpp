// Level.cpp
// ---------
// Прохождение одного уровня: набор чисел, команды, разбор префиксных
// выражений, история/откат и подсказки.
//
// Ключевой инвариант: pool_ — мультимножество PoolItem. Ошибка при разборе
// выражения не меняет pool_: applyExpression работает с копией и фиксирует
// изменения только после успешного разбора.
//
// Имена чисел — только метки. Решатель и сравнение цели работают со
// значениями, поэтому имя не влияет на достижимость.
#include "Level.h"

#include <algorithm>
#include <exception>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "Expression.h"
#include "Helper.h"
#include "Solver.h"

namespace {

std::vector<Num> valuesOf(const std::vector<PoolItem>& pool) {
    // Решатель и проверка цели не должны видеть имена.
    std::vector<Num> result;
    result.reserve(pool.size());
    for (const PoolItem& item : pool) result.push_back(item.value);
    return result;
}

bool allSameType(const std::vector<Num>& values, ValueType type) {
    for (const Num& value : values)
        if (value.type() != type) return false;
    return true;
}

ValueType defaultType(const std::vector<Num>& values) {
    // Тип для голых литералов в calc: если набор однороден, используем его
    // тип, иначе integer (самый широкий из поддерживаемых).
    if (!values.empty() && allSameType(values, values.front().type()))
        return values.front().type();
    return ValueType::Integer;
}

bool hasNames(const std::vector<PoolItem>& pool) {
    for (const PoolItem& item : pool)
        if (!item.name.empty()) return true;
    return false;
}

bool showTypesForPool(const std::vector<PoolItem>& pool) {
    // Показываем типы, если набор смешанный или если есть имена natural:
    // "n=2" без типа читалось бы как integer. Для чистого integer-набора
    // суффикс не нужен.
    std::vector<Num> values = valuesOf(pool);
    bool mixed = !values.empty() && !allSameType(values, ValueType::Integer) &&
                 !allSameType(values, ValueType::Natural);
    if (mixed) return true;
    // Имена показываем вместе с типом, если тип не integer: иначе запись
    // "n2=2" уже однозначна.
    return hasNames(pool) && !allSameType(values, ValueType::Integer);
}

bool showTypesForValues(const std::vector<Num>& values) {
    if (values.empty()) return false;
    if (!allSameType(values, ValueType::Integer)) return true;
    return false;
}

std::string formatValue(const Num& value, bool showType) {
    std::string result = value.str();
    if (showType) result += "(" + value.typeName() + ")";
    return result;
}

std::string formatItem(const PoolItem& item, bool showType) {
    std::string result;
    if (!item.name.empty()) result += item.name + "=";
    result += formatValue(item.value, showType);
    return result;
}

void printQuickExpressionHelp() {
    // Намеренно коротко: подробности живут в docs/gameplay.md, а не в
    // стартовом экране уровня.
    std::cout << "Цель: получить ответ из чисел набора." << std::endl;
    std::cout << "Выражения: sum 1 3 (префикс) или 1 + 3 (инфикс)." << std::endl;
    std::cout << "Вызов: sum(1, 3); типы: natural=2, integer=-3." << std::endl;
    std::cout << "calc <выражение> — посчитать, не меняя набор." << std::endl;
    std::cout << "help — команды; docs/gameplay.md — полное руководство."
              << std::endl;
    std::cout << std::endl;
}

bool parseNumberToken(const std::string& token, Num& value, bool& typed,
                      std::string& error) {
    // Форматы: "2" (integer), "natural=2", "integer=-3".
    // Ошибка заполняется только для похожего на литерал токена; если это
    // просто слово, возвращаем false с пустым error, и вызывающий код
    // попробует имя/функцию.
    std::size_t eq = token.find('=');
    if (eq != std::string::npos) {
        ValueType type;
        if (!valueTypeFromName(token.substr(0, eq), type)) {
            error = "неизвестный тип в литерале: " + token;
            return false;
        }
        long long number = 0;
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
        typed = true;
        return true;
    }
    long long number = 0;
    if (!helper::parseInt(token, number)) return false;
    value = Value::integer(number);
    typed = false;
    return true;
}

std::string ambiguousNumberMessage(long long value) {
    return "число " + std::to_string(value) +
           " есть в наборе в нескольких типах; укажите natural=" +
           std::to_string(value) + " или integer=" + std::to_string(value);
}

}  // namespace

Level::Level(LevelData data, bool tutorial) : data_(std::move(data)), tutorial_(tutorial) {
    // names короче numbers допустимы: отсутствующее имя — пустая строка.
    for (std::size_t i = 0; i < data_.numbers.size(); ++i) {
        PoolItem item;
        item.value = data_.numbers[i];
        if (i < data_.names.size()) item.name = data_.names[i];
        pool_.push_back(item);
    }
    original_ = pool_;
    // stack_ всегда содержит хотя бы начальное состояние: back не должен
    // откатывать ниже него.
    stack_.push_back(pool_);
}

const Fn* Level::findFunction(const std::string& name) const {
    for (const Fn& f : data_.functions)
        if (f.name == name) return &f;
    return nullptr;
}

void Level::printNumbers() const {
    std::cout << "У вас есть ограниченный набор чисел:" << std::endl;
    const bool showTypes = showTypesForPool(pool_);
    for (const PoolItem& item : pool_)
        std::cout << formatItem(item, showTypes) << " ";
    std::cout << std::endl << std::endl;
}

void Level::printFunctions() const {
    std::cout << "У вас есть функции:" << std::endl;
    for (const Fn& f : data_.functions) std::cout << f.description << std::endl;
    std::cout << std::endl;
}

void Level::printHeader() const {
    // Заголовок и тип набора выводятся один раз при входе/сбросе. Тип ответа
    // показываем, только если он отличается от типа набора — иначе это шум.
    std::cout << data_.title << std::endl;
    std::vector<Num> values = valuesOf(pool_);
    if (!values.empty() && allSameType(values, ValueType::Natural)) {
        std::cout << "Тип чисел: natural (неотрицательные)" << std::endl;
    } else if (!values.empty() && !allSameType(values, ValueType::Integer)) {
        std::cout << "Тип чисел: смешанный" << std::endl;
    }
    printNumbers();
    printFunctions();
    const bool answerType = !values.empty() && !allSameType(values, data_.answer.type());
    std::cout << "Вам нужно из данных чисел, используя данные функции, получить число "
              << formatValue(data_.answer, answerType) << "." << std::endl << std::endl;
}

void Level::printTutorial() {
    printQuickExpressionHelp();
    tutorial_ = false;
}

void Level::printHelp() {
    printQuickExpressionHelp();
    std::cout << "Команды: nums, history, hint, back, reset, clear, exit."
              << std::endl;
}

void Level::start() {
    helper::clearScreen();
    printHeader();
    if (tutorial_) printTutorial();
}

void Level::nums() const {
    std::cout << "=";
    const bool showTypes = showTypesForPool(pool_);
    for (const PoolItem& item : pool_) std::cout << " " << formatItem(item, showTypes);
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
    // reset возвращает и набор, и историю, и флаг завершения. tutorial_ не
    // сбрасывается: обучение показывается один раз за запуск.
    pool_ = original_;
    stack_.clear();
    stack_.push_back(pool_);
    history_.clear();
    completed_ = false;
    helper::clearScreen();
    start();
}

void Level::goBack() {
    // stack_ хранит снимки после каждого успешного хода, поэтому back — это
    // просто откат к предыдущему снимку. История синхронизирована с ним.
    if (stack_.size() <= 1) {
        std::cout << "= Нет хода назад!" << std::endl;
        return;
    }
    stack_.pop_back();
    pool_ = stack_.back();
    if (!history_.empty()) history_.pop_back();
    completed_ = false;
    nums();
}

bool Level::evalExpression(const std::vector<std::string>& words, std::size_t& pos,
                           bool consumeFromSet, std::vector<PoolItem>& pool,
                           ValueType literalType, std::vector<Num>& result,
                           std::string& error) const {
    // Рекурсивный разбор префиксной записи. Аргументы функции набираются
    // значениями: вложенный вызов с потоком может дать сразу несколько
    // значений (mul primes 77). Поэтому цикл ниже добавляет part целиком.
    if (pos >= words.size()) {
        error = "недостаточно аргументов!";
        return false;
    }

    const std::string& token = words[pos];
    const Fn* function = findFunction(token);
    if (function != nullptr) {
        ++pos;
        std::vector<Num> args;
        // Один вложенный вызов может вернуть несколько значений и заполнить
        // сразу несколько аргументов родителя, например mul primes 77.
        while (static_cast<int>(args.size()) < function->numberOfArgs) {
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
            result = function->use(args, &data_.functions);
        } catch (const std::exception& e) {
            // Ошибка домена/типа — обычное сообщение игроку, не падение.
            error = e.what();
            return false;
        }
        return true;
    }

    // Имя числа важнее числового синтаксиса. В режиме calc pool — копия
    // набора, поэтому имя можно использовать, но оно не удаляется.
    for (auto it = pool.begin(); it != pool.end(); ++it) {
        if (!it->name.empty() && it->name == token) {
            result = {it->value};
            if (consumeFromSet) pool.erase(it);
            ++pos;
            return true;
        }
    }
    Num number;
    bool typed = false;
    if (!parseNumberToken(token, number, typed, error)) {
        if (!error.empty()) return false;
        error = "не число и не функция: " + token;
        return false;
    }

    if (!consumeFromSet) {
        if (!typed) {
            if (literalType == ValueType::Natural && number.asLongLong() < 0) {
                error = "в natural-выражении нельзя использовать отрицательные числа";
                return false;
            }
            number = Value::of(literalType, number.asLongLong());
        }
        result = {number};
        ++pos;
        return true;
    }

    if (typed) {
        // Типизированный литерал ищет точное совпадение и типа, и значения.
        for (auto it = pool.begin(); it != pool.end(); ++it) {
            if (it->value.same(number)) {
                result = {it->value};
                pool.erase(it);
                ++pos;
                return true;
            }
        }
        error = "числа " + token + " нет в наборе!";
        return false;
    }

    // Голое число: если в наборе есть 2(natural) и 2(integer), выбор
    // неоднозначен. Собираем все типы, чтобы дать понятную подсказку.
    std::set<int> types;
    auto found = pool.end();
    for (auto it = pool.begin(); it != pool.end(); ++it) {
        if (it->value.asLongLong() == number.asLongLong()) {
            if (found == pool.end()) found = it;
            types.insert(static_cast<int>(it->value.type()));
        }
    }
    if (found == pool.end()) {
        error = "числа " + token + " нет в наборе!";
        return false;
    }
    if (types.size() > 1) {
        error = ambiguousNumberMessage(number.asLongLong());
        return false;
    }
    result = {found->value};
    pool.erase(found);
    ++pos;
    return true;
}

bool Level::prepareExpression(const std::string& raw,
                              std::vector<std::string>& words,
                              std::string& error) const {
    // Совместимость важнее единообразия: старые префиксные строки должны
    // разбираться ровно как раньше. Новый парсер включается только тогда,
    // когда строка не может быть старым префиксным выражением:
    //   * она начинается не с функции;
    //   * в ней есть бинарный инфиксный оператор;
    //   * она содержит вызов f(...) без пробела перед скобкой.
    const std::string text = helper::trim(raw);
    if (text.empty()) {
        error = "Пустое выражение.";
        return false;
    }

    std::vector<std::string> firstWords = helper::wordsFromString(text);
    const bool prefixCandidate =
        !firstWords.empty() && !firstWords[0].empty() &&
        findFunction(firstWords[0]) != nullptr;

    if (!prefixCandidate || expression::hasInfixOperator(text) ||
        expression::hasCallSyntax(text)) {
        if (!expression::normalize(text, words, error)) return false;
    } else {
        words = helper::wordsFromString(text);
    }
    return true;
}

void Level::applyExpression(const std::string& raw) {
    // Работаем с копией: если разбор/вычисление упадут, pool_ останется
    // прежним. Изменения фиксируются только после проверки pos == size.
    std::vector<std::string> words;
    std::string error;
    if (!prepareExpression(raw, words, error)) {
        std::cout << "= " << error << std::endl;
        return;
    }

    std::vector<PoolItem> pool = pool_;
    std::size_t pos = 0;
    std::vector<Num> result;
    if (!evalExpression(words, pos, true, pool, defaultType(valuesOf(pool_)), result,
                        error)) {
        std::cout << "= " << error << std::endl;
        return;
    }
    if (pos != words.size()) {
        std::cout << "= Лишние аргументы! Введите выражение заново." << std::endl;
        return;
    }

    pool_ = std::move(pool);
    // У результатов нет имён: имя было свойством конкретного числа в наборе,
    // а не значения вообще.
    for (const Num& value : result) pool_.push_back(PoolItem{value, {}});

    const bool showTypes = showTypesForPool(pool_);
    std::cout << "=";
    if (result.empty()) {
        std::cout << " (пусто)";
    } else {
        for (const Num& value : result)
            std::cout << " " << formatValue(value, showTypes);
    }
    std::cout << "   (в наборе:";
    for (const PoolItem& item : pool_)
        std::cout << " " << formatItem(item, showTypes);
    std::cout << ")" << std::endl;

    // В историю пишем то, что ввёл игрок, а не нормализованный префикс.
    std::string record = helper::trim(raw) + " =";
    if (result.empty()) {
        record += " (пусто)";
    } else {
        for (const Num& value : result) record += " " + value.str();
    }
    history_.push_back(record);

    std::vector<Num> current = valuesOf(pool_);
    // Сравнение с ответом учитывает тип: natural 2 не равен integer 2.
    completed_ = std::find(current.begin(), current.end(), data_.answer) != current.end();
    stack_.push_back(pool_);
}

void Level::calc(const std::string& raw) {
    // calc повторяет разбор, но с consumeFromSet=false: имена читаются из
    // копии, литералы создаются, а настоящий набор не меняется.
    std::vector<std::string> words;
    std::string error;
    if (!prepareExpression(raw, words, error)) {
        std::cout << "= " << error << std::endl;
        return;
    }

    std::size_t pos = 0;
    std::vector<PoolItem> pool = pool_;
    std::vector<Num> result;
    if (!evalExpression(words, pos, false, pool, defaultType(valuesOf(pool_)), result,
                        error)) {
        std::cout << "= " << error << std::endl;
        return;
    }
    if (pos != words.size()) {
        std::cout << "= Лишние аргументы! Введите выражение заново." << std::endl;
        return;
    }
    const bool showTypes = showTypesForValues(result);
    std::cout << "=";
    if (result.empty()) {
        std::cout << " (пусто)";
    } else {
        for (const Num& value : result)
            std::cout << " " << formatValue(value, showTypes);
    }
    std::cout << " (без изменения набора)" << std::endl;
}

void Level::hint() {
    // Подсказка — best-effort: решатель ограничен по времени/узлам и может
    // не найти решение. Показываем только первый ход, чтобы не раскрывать
    // весь маршрут.
    std::vector<solver::Move> moves;
    std::vector<Num> numbers = valuesOf(pool_);
    if (solver::solve(numbers, data_.functions, data_.answer, moves)) {
        if (moves.empty()) {
            std::cout << "= Нужное число уже есть в наборе!" << std::endl;
            return;
        }
        const solver::Move& move = moves.front();
        const Fn* function = findFunction(move.fn);
        const bool random = function != nullptr && !function->isStable();
        const bool showTypes = showTypesForValues(move.args) ||
                               showTypesForValues(move.results);
        std::cout << "= Подсказка: попробуйте  " << move.fn;
        for (const Num& arg : move.args) std::cout << " " << formatValue(arg, showTypes);
        if (random) {
            std::cout << "   (результат случайный — может понадобиться несколько попыток)";
        } else {
            std::cout << "   (получится";
            if (move.results.empty()) {
                std::cout << " пусто";
            } else {
                for (const Num& value : move.results)
                    std::cout << " " << formatValue(value, showTypes);
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
    // Это важно для уровней, где ответ совпадает с начальным числом.
    std::vector<Num> current = valuesOf(pool_);
    if (std::find(current.begin(), current.end(), data_.answer) != current.end()) {
        completed_ = true;
        return 0;
    }
    std::cout << "[Level] Введите выражение: ";
    std::string line;
    if (!std::getline(std::cin, line)) {
        std::cout << std::endl;
        return -1;
    }
    const std::string text = helper::trim(line);
    if (text.empty()) return 0;
    std::vector<std::string> words = helper::wordsFromString(text);
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
        const std::string rest =
            text.size() > first.size() ? helper::trim(text.substr(first.size())) : "";
        if (rest.empty()) {
            std::cout << "= Недостаточно аргументов! Введите выражение заново." << std::endl;
        } else {
            calc(rest);
        }
    } else if (first == "back") {
        goBack();
    } else {
        // Любая другая строка — выражение. Оно может начинаться с функции
        // (старый префикс), числа, имени или открывающей скобки (инфикс).
        applyExpression(text);
    }
    return 0;
}

LevelResult Level::play() {
    // Цикл заканчивается либо победой, либо exit. Возвращаем tutorial_,
    // чтобы Game сохранил, показывать ли обучение в следующий раз.
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
