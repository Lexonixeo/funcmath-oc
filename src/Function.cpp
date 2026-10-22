// Function.cpp
// ------------
// Пользовательские функции: разбор блока, проверка, выбор перегрузки и
// runtime-интерпретатор тела.
//
// Конвейер:
//
//   .lvl/.fn
//     → LevelStore::parseFunctionBlock   (сырой FunctionBlock)
//     → compileFunctionBlocks           (проверка + FunctionOverload)
//     → selectOverload                  (выбор по типам аргументов)
//     → runBody                         (вычисление тела)
//
// Тело — последовательность иммутабельных присваиваний. Если выражение
// случайное, runBody размножает окружения: каждое окружение — один возможный
// мир. Это позволяет решателю перебрать все исходы, не смешивая их.
//
// Типы проверяются во время вычисления, а не статически. Структура блока
// (имена, выходы, присваивания) проверяется при загрузке.
#include "Function.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "Expression.h"
#include "Helper.h"

namespace {

constexpr int kMaxCallDepth = 64;
// Ограничение на число альтернативных окружений/результатов. Нужно, чтобы
// случайная функция с большим потоком не взорвала память и DFS.
constexpr std::size_t kMaxCustomOutcomes = 4096;

std::string trim(const std::string& text) { return helper::trim(text); }

bool isIdentifier(const std::string& name) {
    // Идентификаторы ASCII: буква/подчёркивание, затем буквы/цифры.
    // Имена типов (natural/integer) запрещены, чтобы "natural" нельзя было
    // спутать с литералом/типом.
    if (name.empty()) return false;
    auto isStart = [](unsigned char c) {
        return std::isalpha(c) || c == '_';
    };
    auto isPart = [](unsigned char c) {
        return std::isalnum(c) || c == '_';
    };
    if (!isStart(static_cast<unsigned char>(name.front()))) return false;
    for (std::size_t i = 1; i < name.size(); ++i)
        if (!isPart(static_cast<unsigned char>(name[i]))) return false;
    ValueType ignoredType;
    if (valueTypeFromName(name, ignoredType)) return false;
    return true;
}

bool isReservedLocalName(const std::string& name) {
    // Специальные формы тела не могут быть перекрыты локальным именем.
    return name == "take" || name == "drop" || name == "first" || name == "rest" ||
           name == "count";
}

bool findOutput(const FunctionOverload& overload, const std::string& name,
                const OutputDef** out) {
    for (const OutputDef& output : overload.source.outputs) {
        if (output.name == name) {
            *out = &output;
            return true;
        }
    }
    return false;
}

std::string blockError(const FunctionBlock& block, const std::string& message) {
    // Единый формат диагностики: имя функции + строка, если она известна.
    std::string result = "функция " + block.name;
    if (block.line > 0) result += " (строка " + std::to_string(block.line) + ")";
    result += ": " + message;
    return result;
}

bool parseInputType(const std::string& text, ValueType& out, std::string& error) {
    std::string value = trim(text);
    if (value.size() > 3 && value.substr(value.size() - 3) == "...") {
        error = "поток допустим только в out: " + text;
        return false;
    }
    if (!valueTypeFromName(value, out)) {
        error = "неизвестный тип: " + text;
        return false;
    }
    return true;
}

bool parseResultTokens(const std::vector<std::string>& words, std::size_t from,
                       fmath::ResultSpec& out, std::string& error) {
    // Список результатов после "->": "integer natural..." и т.п.
    // Поток может быть только последним; фиксированные типы идут в fixed.
    out = fmath::ResultSpec{};
    if (from >= words.size()) {
        error = "не указан тип результата после ->";
        return false;
    }
    for (std::size_t i = from; i < words.size(); ++i) {
        std::string token = words[i];
        if (token.empty()) continue;
        bool stream = false;
        if (token.size() > 3 && token.substr(token.size() - 3) == "...") {
            stream = true;
            token = token.substr(0, token.size() - 3);
        }
        ValueType type;
        if (!valueTypeFromName(token, type)) {
            error = "неизвестный тип результата: " + words[i];
            return false;
        }
        if (stream) {
            if (i + 1 != words.size()) {
                error = "поток ... может быть только последним результатом";
                return false;
            }
            out.hasStream = true;
            out.streamType = type;
        } else {
            out.fixed.push_back(type);
        }
    }
    if (!out.hasStream && out.fixed.empty()) {
        error = "пустой список результатов";
        return false;
    }
    return true;
}

bool parseAliasLine(const std::string& raw, AliasDef& out, std::string& error) {
    // use <алиас> = <функция> <типы> -> <результаты>
    // Алиас фиксирует сигнатуру, чтобы снять неоднозначность перегрузки.
    std::string line = trim(raw);
    if (line.rfind("use ", 0) == 0) line = trim(line.substr(4));
    std::size_t eq = line.find('=');
    if (eq == std::string::npos) {
        error = "ожидается 'use имя = функция типы -> результаты': " + raw;
        return false;
    }
    std::vector<std::string> left = helper::wordsFromString(line.substr(0, eq));
    if (left.size() != 1 || !isIdentifier(left[0])) {
        error = "некорректное имя алиаса: " + raw;
        return false;
    }
    out = AliasDef{};
    out.name = left[0];

    std::vector<std::string> right = helper::wordsFromString(line.substr(eq + 1));
    auto arrow = std::find(right.begin(), right.end(), "->");
    if (arrow == right.end()) {
        error = "в use не найден разделитель ->: " + raw;
        return false;
    }
    std::size_t arrowIndex = static_cast<std::size_t>(arrow - right.begin());
    if (arrowIndex == 0) {
        error = "в use не указана функция: " + raw;
        return false;
    }
    out.function = right[0];
    for (std::size_t i = 1; i < arrowIndex; ++i) {
        ValueType type;
        if (!parseInputType(right[i], type, error)) return false;
        out.args.push_back(type);
    }
    std::vector<std::string> results(right.begin() + arrowIndex + 1, right.end());
    if (!parseResultTokens(results, 0, out.results, error)) return false;
    return true;
}

bool parseBindingLine(const std::string& raw, Binding& out, std::string& error) {
    // Присваивание: "target = expr" или "a, b = expr".
    // Правая часть не вычисляется здесь — только режется на слова.
    std::string line = trim(raw);
    std::size_t eq = line.find('=');
    if (eq == std::string::npos) {
        error = "ожидается присваивание 'имя = выражение': " + raw;
        return false;
    }
    std::string left = trim(line.substr(0, eq));
    std::string right = trim(line.substr(eq + 1));
    if (left.empty()) {
        error = "пустое имя в присваивании: " + raw;
        return false;
    }
    if (right.empty()) {
        error = "пустое выражение в присваивании: " + raw;
        return false;
    }
    out = Binding{};
    out.targets = helper::wordsFromString(left);
    // rhs пока остаётся старым префиксным разбором. Инфикс нормализуется
    // позже, когда известны все функции уровня: иначе нельзя отличить
    // префиксный вызов sum 1 2 от переменной a в выражении a - b.
    out.rhs = helper::wordsFromString(right);
    out.rawRhs = right;
    out.source = raw;
    if (out.targets.empty() || out.rhs.empty()) {
        error = "некорректное присваивание: " + raw;
        return false;
    }
    return true;
}

bool prepareOverload(const FunctionBlock& block, FunctionOverload& out,
                     std::string& error) {
    // Структурная проверка одной перегрузки. Здесь проверяется всё, что можно
    // проверить без вычисления: уникальность имён, форма выходов, алиасы,
    // синтаксис присваиваний и то, что каждый выход получает значение.
    //
    // Типы выражений намеренно не проверяются: они зависят от перегрузок,
    // значений и случайных исходов.
    out = FunctionOverload{};
    out.source = block;
    if (!isIdentifier(block.name)) {
        error = blockError(block, "некорректное имя функции");
        return false;
    }
    std::set<std::string> params;
    for (const ParamDef& param : block.params) {
        if (!isIdentifier(param.name)) {
            error = blockError(block, "некорректное имя параметра: " + param.name);
            return false;
        }
        if (isReservedLocalName(param.name)) {
            error = blockError(block, "имя параметра зарезервировано: " + param.name);
            return false;
        }
        if (!params.insert(param.name).second) {
            error = blockError(block, "параметр повторяется: " + param.name);
            return false;
        }
        out.inputTypes.push_back(param.type);
    }
    if (block.outputs.empty()) {
        error = blockError(block, "нужен хотя бы один out");
        return false;
    }
    std::set<std::string> outputs;
    for (std::size_t i = 0; i < block.outputs.size(); ++i) {
        const OutputDef& output = block.outputs[i];
        if (!isIdentifier(output.name)) {
            error = blockError(block, "некорректное имя выхода: " + output.name);
            return false;
        }
        if (isReservedLocalName(output.name)) {
            error = blockError(block, "имя выхода зарезервировано: " + output.name);
            return false;
        }
        if (!outputs.insert(output.name).second) {
            error = blockError(block, "выход повторяется: " + output.name);
            return false;
        }
        if (output.spec.hasStream && i + 1 != block.outputs.size()) {
            error = blockError(block, "поток ... может быть только последним выходом");
            return false;
        }
        if (output.spec.fixed.empty() && !output.spec.hasStream) {
            error = blockError(block, "у выхода нет типа");
            return false;
        }
    }
    out.results = fmath::ResultSpec{};
    for (const OutputDef& output : block.outputs) {
        for (ValueType type : output.spec.fixed) out.results.fixed.push_back(type);
        if (output.spec.hasStream) {
            out.results.hasStream = true;
            out.results.streamType = output.spec.streamType;
        }
    }

    std::set<std::string> aliases;
    for (const std::string& line : block.useLines) {
        AliasDef alias;
        std::string aliasError;
        if (!parseAliasLine(line, alias, aliasError)) {
            error = blockError(block, aliasError);
            return false;
        }
        // Алиас — локальное имя; оно не должно конфликтовать с параметрами,
        // выходами, другими алиасами и специальными формами.
        if (!aliases.insert(alias.name).second) {
            error = blockError(block, "алиас повторяется: " + alias.name);
            return false;
        }
        if (isReservedLocalName(alias.name)) {
            error = blockError(block, "имя алиаса зарезервировано: " + alias.name);
            return false;
        }
        if (params.count(alias.name) || outputs.count(alias.name)) {
            error = blockError(block, "имя алиаса конфликтует с параметром/выходом: " +
                                          alias.name);
            return false;
        }
        out.aliases.push_back(std::move(alias));
    }

    std::set<std::string> assignedOutputs;
    std::set<std::string> temps;
    if (block.bodyLines.empty()) {
        error = blockError(block, "тело пустое");
        return false;
    }
    // Выходы можно присваивать только один раз; временные переменные тоже
    // должны быть уникальны. Параметры и алиасы присваивать нельзя.
    for (const std::string& line : block.bodyLines) {
        Binding binding;
        std::string bindingError;
        if (!parseBindingLine(line, binding, bindingError)) {
            error = blockError(block, bindingError);
            return false;
        }
        for (const std::string& target : binding.targets) {
            if (!isIdentifier(target)) {
                error = blockError(block, "некорректное имя в присваивании: " + target);
                return false;
            }
            if (isReservedLocalName(target)) {
                error = blockError(block, "имя в присваивании зарезервировано: " + target);
                return false;
            }
            if (params.count(target)) {
                error = blockError(block, "нельзя присваивать параметру " + target);
                return false;
            }
            if (aliases.count(target)) {
                error = blockError(block, "нельзя присваивать алиасу " + target);
                return false;
            }
            if (outputs.count(target)) {
                if (!assignedOutputs.insert(target).second) {
                    error = blockError(block, "выход присваивается повторно: " + target);
                    return false;
                }
            } else {
                if (!temps.insert(target).second) {
                    error = blockError(block, "имя уже используется: " + target);
                    return false;
                }
            }
        }
        out.body.push_back(std::move(binding));
    }
    for (const OutputDef& output : block.outputs) {
        if (!assignedOutputs.count(output.name)) {
            error = blockError(block, "выход не получает значения: " + output.name);
            return false;
        }
    }
    return true;
}

const Fn* findFn(const std::vector<Fn>& functions, const std::string& name) {
    for (const Fn& function : functions)
        if (function.name == name) return &function;
    return nullptr;
}

const AliasDef* findAlias(const FunctionOverload& overload, const std::string& name) {
    for (const AliasDef& alias : overload.aliases)
        if (alias.name == name) return &alias;
    return nullptr;
}

Num coerceValue(const Num& value, ValueType type, std::string& error) {
    // Приведение только по правилам canPass:
    //   natural -> integer — всегда;
    //   integer -> natural — только если значение неотрицательно.
    // Это проверяемое сужение, поэтому ошибка возможна во время вычисления.
    if (value.type() == type) return value;
    if (value.isNatural() && type == ValueType::Integer) return Value::integer(value.asLongLong());
    if (value.isInteger() && type == ValueType::Natural) {
        if (value.asLongLong() < 0) {
            error = "natural: отрицательный результат " + value.str();
            return value;
        }
        return Value::natural(value.asLongLong());
    }
    error = "нельзя преобразовать " + value.typeName() + " в " + valueTypeName(type);
    return value;
}

bool selectOverload(const Fn& function, const std::vector<Num>& args,
                    std::shared_ptr<FunctionOverload>& out, std::string& error) {
    // Выбор перегрузки по score: +1 за каждый точный тип. Расширение
    // natural -> integer допустимо, но даёт меньший score. Равный лучший
    // score — неоднозначность; её снимает use-алиас.
    int bestScore = -1;
    std::shared_ptr<FunctionOverload> best;
    for (const auto& overload : function.overloads) {
        if (overload->inputTypes.size() != args.size()) continue;
        bool ok = true;
        int score = 0;
        for (std::size_t i = 0; i < args.size(); ++i) {
            if (!fmath::canPass(args[i].type(), overload->inputTypes[i])) {
                ok = false;
                break;
            }
            if (args[i].type() == overload->inputTypes[i]) ++score;
        }
        if (!ok) continue;
        if (score > bestScore) {
            bestScore = score;
            best = overload;
        } else if (score == bestScore && best != nullptr) {
            error = "неоднозначная перегрузка функции " + function.name;
            return false;
        }
    }
    if (best == nullptr) {
        error = "неподходящие аргументы для функции " + function.name;
        return false;
    }
    out = best;
    return true;
}

using Env = std::map<std::string, std::vector<Num>>;
// Один элемент ResultList — один возможный набор результатов вызова.
// Несколько элементов появляются из-за случайности или потока.
using ResultList = std::vector<std::vector<Num>>;

struct EvalContext {
    const FunctionOverload& overload;
    const std::vector<Fn>& functions;
    int depth = 0;          // защита от рекурсии
    bool allOutcomes = false;  // перебирать ли все случайные исходы
};

bool runBody(const FunctionOverload& overload, const std::vector<Num>& args,
             const std::vector<Fn>& functions, bool allOutcomes, int depth,
             ResultList& out, std::string& error);

bool callFunction(const std::string& name, const std::vector<Num>& args,
                  EvalContext& context, ResultList& out, std::string& error) {
    // Вызов по имени: пользовательская функция уровня, простейшая по
    // behavior или встроенная по имени. Используется для алиасов и
    // вложенных вызовов внутри тела.
    const Fn* function = findFn(context.functions, name);
    if (function != nullptr) {
        if (function->isCustom()) {
            std::shared_ptr<FunctionOverload> overload;
            if (!selectOverload(*function, args, overload, error)) return false;
            return runBody(*overload, args, context.functions, context.allOutcomes,
                           context.depth + 1, out, error);
        }
        try {
            if (context.allOutcomes)
                out = fmath::outcomes(function->behavior, args);
            else
                out = {fmath::apply(function->behavior, args)};
            return true;
        } catch (const std::exception& e) {
            error = e.what();
            return false;
        }
    }
    if (fmath::hasFunction(name)) {
        try {
            if (context.allOutcomes)
                out = fmath::outcomes(name, args);
            else
                out = {fmath::apply(name, args)};
            return true;
        } catch (const std::exception& e) {
            error = e.what();
            return false;
        }
    }
    error = "неизвестная функция: " + name;
    return false;
}

bool evalExpr(const std::vector<std::string>& words, std::size_t& pos,
              const Env& env, EvalContext& context, ResultList& out,
              std::string& error);

bool parseLiteral(const std::string& token, Num& out) {
    std::size_t eq = token.find('=');
    if (eq != std::string::npos) {
        ValueType type;
        long long value;
        if (!valueTypeFromName(token.substr(0, eq), type)) return false;
        if (!helper::parseInt(token.substr(eq + 1), value)) return false;
        try {
            out = Value::of(type, value);
        } catch (const std::exception&) {
            return false;
        }
        return true;
    }
    long long value;
    if (!helper::parseInt(token, value)) return false;
    out = Value::integer(value);
    return true;
}

bool evalSpecial(const std::string& name, const std::vector<std::string>& words,
                 std::size_t& pos, const Env& env, EvalContext& context,
                 ResultList& out, std::string& error) {
    // Специальные формы работают с ResultList, а не с одним значением:
    // take/drop/first/rest/count могут применяться к случайному выражению
    // и должны сохранить все его миры.
    ++pos;
    if (name == "take" || name == "drop") {
        if (pos >= words.size()) {
            error = name + ": не указано количество";
            return false;
        }
        // Количество — литерал, а не выражение: это упрощает грамматику.
        long long count;
        if (!helper::parseInt(words[pos], count) || count < 0) {
            error = name + ": количество должно быть неотрицательным целым";
            return false;
        }
        ++pos;
        if (pos >= words.size()) {
            error = name + ": не указано выражение";
            return false;
        }
        ResultList inner;
        if (!evalExpr(words, pos, env, context, inner, error)) return false;
        out.clear();
        for (const std::vector<Num>& values : inner) {
            if (name == "take") {
                if (static_cast<long long>(values.size()) < count) {
                    error = "take: нужно " + std::to_string(count) +
                            " результатов, получено " + std::to_string(values.size());
                    return false;
                }
                out.push_back(std::vector<Num>(
                    values.begin(), values.begin() + static_cast<std::size_t>(count)));
            } else {
                if (static_cast<long long>(values.size()) <= count) {
                    out.push_back({});
                } else {
                    out.push_back(std::vector<Num>(
                        values.begin() + static_cast<std::size_t>(count), values.end()));
                }
            }
        }
        return true;
    }
    if (name == "first" || name == "rest" || name == "count") {
        if (pos >= words.size()) {
            error = name + ": не указано выражение";
            return false;
        }
        // first на пустом потоке — ошибка; rest на пустом даёт пустой поток;
        // count всегда возвращает integer.
        ResultList inner;
        if (!evalExpr(words, pos, env, context, inner, error)) return false;
        out.clear();
        for (const std::vector<Num>& values : inner) {
            if (name == "first") {
                if (values.empty()) {
                    error = "first: пустой результат";
                    return false;
                }
                out.push_back({values.front()});
            } else if (name == "rest") {
                if (values.empty())
                    out.push_back({});
                else
                    out.push_back(std::vector<Num>(values.begin() + 1, values.end()));
            } else {
                out.push_back({Value::integer(static_cast<long long>(values.size()))});
            }
        }
        return true;
    }
    error = "неизвестная специальная форма: " + name;
    return false;
}

bool evalCall(const std::string& name, const std::vector<std::string>& words,
              std::size_t& pos, const Env& env, EvalContext& context,
              ResultList& out, std::string& error) {
    // Приоритет имени: алиас -> функция уровня -> встроенная функция.
    // Алиас существует только внутри текущей перегрузки.
    const AliasDef* alias = findAlias(context.overload, name);
    const Fn* function = alias == nullptr ? findFn(context.functions, name) : nullptr;
    const fmath::FunctionSpec* builtin =
        (alias == nullptr && function == nullptr) ? fmath::findSpec(name) : nullptr;
    if (alias == nullptr && function == nullptr && builtin == nullptr) {
        error = "не число и не функция: " + name;
        return false;
    }
    ++pos;

    int arity = 0;
    const std::string* targetName = nullptr;
    if (alias != nullptr) {
        arity = static_cast<int>(alias->args.size());
        targetName = &alias->function;
    } else if (function != nullptr) {
        arity = function->numberOfArgs;
    } else {
        arity = builtin->arity();
    }

    // Аргументы набираются по значениям, а не по синтаксическим выражениям:
    // mul primes 77 должен получить два числа от одного вложенного вызова.
    // partials — набор префиксов аргументов; каждый вложенный вызов может
    // добавить несколько значений и размножить варианты.
    std::vector<std::vector<Num>> partials = {{}};
    while (true) {
        bool needMore = false;
        for (const std::vector<Num>& values : partials)
            if (static_cast<int>(values.size()) < arity) needMore = true;
        if (!needMore) break;
        if (pos >= words.size()) break;

        ResultList part;
        if (!evalExpr(words, pos, env, context, part, error)) return false;
        if (part.empty()) {
            partials.clear();
            break;
        }
        std::vector<std::vector<Num>> next;
        for (const std::vector<Num>& prefix : partials) {
            for (const std::vector<Num>& values : part) {
                std::vector<Num> combined = prefix;
                combined.insert(combined.end(), values.begin(), values.end());
                next.push_back(std::move(combined));
            }
        }
        if (next.size() > kMaxCustomOutcomes) next.resize(kMaxCustomOutcomes);
        partials = std::move(next);
    }

    if (partials.empty()) {
        out.clear();
        return true;
    }
    for (const std::vector<Num>& values : partials) {
        if (static_cast<int>(values.size()) != arity) {
            error = "функция " + name + " ожидает " + std::to_string(arity) +
                    " аргумент(ов), получено " + std::to_string(values.size());
            return false;
        }
    }

    out.clear();
    for (const std::vector<Num>& values : partials) {
        ResultList callResults;
        std::string callError;
        bool ok = false;
        if (alias != nullptr) {
            // Алиас проверяет типы по своей сигнатуре и приводит аргументы
            // (natural -> integer) до вызова цели.
            std::vector<Num> aliasArgs = values;
            ok = true;
            for (std::size_t i = 0; i < aliasArgs.size(); ++i) {
                if (!fmath::canPass(aliasArgs[i].type(), alias->args[i])) {
                    callError = "аргумент " + std::to_string(i + 1) +
                                " не подходит к алиасу " + alias->name;
                    ok = false;
                    break;
                }
                std::string convertError;
                aliasArgs[i] = coerceValue(aliasArgs[i], alias->args[i], convertError);
                if (!convertError.empty()) {
                    callError = "алиас " + alias->name + ": " + convertError;
                    ok = false;
                    break;
                }
            }
            if (ok)
                ok = callFunction(*targetName, aliasArgs, context, callResults, callError);
        } else if (function != nullptr) {
            if (function->isCustom()) {
                std::shared_ptr<FunctionOverload> overload;
                ok = selectOverload(*function, values, overload, callError);
                if (ok)
                    ok = runBody(*overload, values, context.functions,
                                 context.allOutcomes, context.depth + 1,
                                 callResults, callError);
            } else {
                try {
                    if (context.allOutcomes)
                        callResults = fmath::outcomes(function->behavior, values);
                    else
                        callResults = {fmath::apply(function->behavior, values)};
                    ok = true;
                } catch (const std::exception& e) {
                    callError = e.what();
                }
            }
        } else {
            try {
                if (context.allOutcomes)
                    callResults = fmath::outcomes(name, values);
                else
                    callResults = {fmath::apply(name, values)};
                ok = true;
            } catch (const std::exception& e) {
                callError = e.what();
            }
        }
        if (!ok) {
            error = "в вызове " + name + ": " + callError;
            return false;
        }
        out.insert(out.end(), callResults.begin(), callResults.end());
        if (out.size() > kMaxCustomOutcomes) {
            out.resize(kMaxCustomOutcomes);
            break;
        }
    }
    return true;
}

bool evalExpr(const std::vector<std::string>& words, std::size_t& pos,
              const Env& env, EvalContext& context, ResultList& out,
              std::string& error) {
    // Порядок важен: специальные формы не могут быть перекрыты функцией;
    // затем проверяются локальные имена (алиас/функция), встроенные функции,
    // литералы и переменные окружения.
    if (pos >= words.size()) {
        error = "недостаточно аргументов";
        return false;
    }
    const std::string token = words[pos];
    if (token == "take" || token == "drop" || token == "first" || token == "rest" ||
        token == "count") {
        return evalSpecial(token, words, pos, env, context, out, error);
    }

    const AliasDef* alias = findAlias(context.overload, token);
    const Fn* function = alias == nullptr ? findFn(context.functions, token) : nullptr;
    const fmath::FunctionSpec* builtin =
        (alias == nullptr && function == nullptr) ? fmath::findSpec(token) : nullptr;
    if (alias != nullptr || function != nullptr || builtin != nullptr) {
        return evalCall(token, words, pos, env, context, out, error);
    }

    Num literal;
    if (parseLiteral(token, literal)) {
        // Bare-литерал в теле — integer; natural=... нужен явно.
        out = {{literal}};
        ++pos;
        return true;
    }

    // Переменная окружения: параметр, выход или временная переменная.
    auto it = env.find(token);
    if (it != env.end()) {
        out = {it->second};
        ++pos;
        return true;
    }
    error = "не число, не параметр и не функция: " + token;
    return false;
}

bool assignBinding(const Binding& binding, const FunctionOverload& overload,
                   const std::vector<Num>& values, Env& env, std::string& error) {
    // Один target получает весь вектор значений (это выход-поток).
    // Несколько targets — покомпонентное присваивание; поток в таком
    // присваивании запрещён, потому что его длина неизвестна.
    if (binding.targets.size() == 1) {
        const std::string& target = binding.targets.front();
        const OutputDef* output = nullptr;
        std::vector<Num> stored = values;
        if (findOutput(overload, target, &output)) {
            if (!output->spec.hasStream && stored.size() != 1) {
                error = "выход " + target + " ожидает одно значение, получено " +
                        std::to_string(stored.size());
                return false;
            }
            ValueType type = output->spec.hasStream
                                 ? output->spec.streamType
                                 : output->spec.fixed.front();
            for (Num& value : stored) {
                std::string convertError;
                value = coerceValue(value, type, convertError);
                if (!convertError.empty()) {
                    error = "выход " + target + ": " + convertError;
                    return false;
                }
            }
        }
        env[target] = std::move(stored);
        return true;
    }

    if (values.size() != binding.targets.size()) {
        error = "присваивание ожидает " + std::to_string(binding.targets.size()) +
                " значений, получено " + std::to_string(values.size());
        return false;
    }
    for (std::size_t i = 0; i < binding.targets.size(); ++i) {
        const std::string& target = binding.targets[i];
        Num value = values[i];
        const OutputDef* output = nullptr;
        if (findOutput(overload, target, &output)) {
            if (output->spec.hasStream || output->spec.fixed.size() != 1) {
                error = "выход " + target + " не может быть потоком при множественном присваивании";
                return false;
            }
            std::string convertError;
            value = coerceValue(value, output->spec.fixed.front(), convertError);
            if (!convertError.empty()) {
                error = "выход " + target + ": " + convertError;
                return false;
            }
        }
        env[target] = {value};
    }
    return true;
}

bool gatherOutputs(const FunctionOverload& overload, const Env& env,
                   std::vector<Num>& out, std::string& error) {
    // Порядок результатов — порядок объявления out в блоке. Фиксированный
    // выход обязан содержать ровно одно значение; поток — любое количество.
    out.clear();
    for (const OutputDef& output : overload.source.outputs) {
        auto it = env.find(output.name);
        if (it == env.end()) {
            error = "выход " + output.name + " не получил значения";
            return false;
        }
        const std::vector<Num>& values = it->second;
        if (!output.spec.hasStream && values.size() != 1) {
            error = "выход " + output.name + " ожидает одно значение, получено " +
                    std::to_string(values.size());
            return false;
        }
        ValueType type = output.spec.hasStream ? output.spec.streamType
                                               : output.spec.fixed.front();
        for (const Num& value : values) {
            std::string convertError;
            Num converted = coerceValue(value, type, convertError);
            if (!convertError.empty()) {
                error = "выход " + output.name + ": " + convertError;
                return false;
            }
            out.push_back(converted);
        }
    }
    return true;
}

bool runBody(const FunctionOverload& overload, const std::vector<Num>& args,
             const std::vector<Fn>& functions, bool allOutcomes, int depth,
             ResultList& out, std::string& error) {
    // Каждое присваивание может дать несколько альтернатив (случайность,
    // поток). envs — множество возможных окружений; после каждой строки
    // перемножаем альтернативы. Для стабильного вызова envs почти всегда
    // остается одним элементом.
    out.clear();
    if (depth > kMaxCallDepth) {
        error = "слишком глубокая рекурсия (возможно, функция вызывает себя)";
        return false;
    }
    if (args.size() != overload.inputTypes.size()) {
        error = "функция " + overload.source.name + " ожидает " +
                std::to_string(overload.inputTypes.size()) + " аргумент(ов), получено " +
                std::to_string(args.size());
        return false;
    }
    Env initial;
    for (std::size_t i = 0; i < args.size(); ++i) {
        Num value = args[i];
        std::string convertError;
        // Параметры тоже проходят проверяемое приведение: natural-перегрузка
        // не примет отрицательный integer.
        value = coerceValue(value, overload.inputTypes[i], convertError);
        if (!convertError.empty()) {
            error = "параметр " + overload.source.params[i].name + ": " + convertError;
            return false;
        }
        initial[overload.source.params[i].name] = {value};
    }

    EvalContext context{overload, functions, depth, allOutcomes};
    std::vector<Env> envs = {initial};
    for (const Binding& binding : overload.body) {
        std::vector<Env> next;
        for (const Env& env : envs) {
            std::size_t pos = 0;
            ResultList alternatives;
            std::string evalError;
            if (!evalExpr(binding.rhs, pos, env, context, alternatives, evalError)) {
                error = "в строке \"" + binding.source + "\": " + evalError;
                return false;
            }
            if (pos != binding.rhs.size()) {
                error = "в строке \"" + binding.source + "\": лишние слова";
                return false;
            }
            if (alternatives.empty()) {
                error = "в строке \"" + binding.source + "\": нет допустимых результатов";
                return false;
            }
            for (const std::vector<Num>& values : alternatives) {
                Env assigned = env;
                std::string assignError;
                if (!assignBinding(binding, overload, values, assigned, assignError)) {
                    error = "в строке \"" + binding.source + "\": " + assignError;
                    return false;
                }
                next.push_back(std::move(assigned));
                if (next.size() > kMaxCustomOutcomes) break;
            }
            if (next.size() > kMaxCustomOutcomes) break;
        }
        envs = std::move(next);
        if (envs.empty()) break;
    }

    if (envs.empty()) {
        // Для перебора это просто отсутствие исходов; для обычного вызова —
        // ошибка, которую увидит игрок.
        if (!allOutcomes) error = "нет допустимых результатов";
        return !allOutcomes;
    }

    for (const Env& env : envs) {
        std::vector<Num> results;
        std::string gatherError;
        if (!gatherOutputs(overload, env, results, gatherError)) {
            error = gatherError;
            return false;
        }
        // Порядок окружений не важен для решателя, но сохраняет
        // детерминированный порядок результатов обычного вызова.
        out.push_back(std::move(results));
        if (out.size() > kMaxCustomOutcomes) break;
    }
    return true;
}

bool aliasTargetExists(const AliasDef& alias, const std::vector<Fn>& functions,
                       std::string& error) {
    // Цель алиаса может быть функцией уровня (включая объявленную позже) или
    // встроенной. Проверяем, что среди её сигнатур есть совместимая с
    // объявленными типами алиаса.
    const Fn* function = findFn(functions, alias.function);
    if (function != nullptr) {
        for (const auto& overload : function->overloads) {
            if (overload->inputTypes.size() != alias.args.size()) continue;
            bool ok = true;
            for (std::size_t i = 0; i < alias.args.size(); ++i)
                if (!fmath::canPass(alias.args[i], overload->inputTypes[i])) ok = false;
            if (ok) return true;
        }
        error = "для алиаса " + alias.name + " нет подходящей перегрузки " + alias.function;
        return false;
    }
    const fmath::FunctionSpec* spec = fmath::findSpec(alias.function);
    if (spec == nullptr) {
        error = "неизвестная функция в use: " + alias.function;
        return false;
    }
    for (const fmath::Signature& signature : spec->signatures) {
        if (signature.args.size() != alias.args.size()) continue;
        bool ok = true;
        for (std::size_t i = 0; i < alias.args.size(); ++i)
            if (!fmath::canPass(alias.args[i], signature.args[i])) ok = false;
        if (ok) return true;
    }
    error = "для алиаса " + alias.name + " нет подходящей сигнатуры " + alias.function;
    return false;
}

bool isSpecialName(const std::string& name) {
    return name == "take" || name == "drop" || name == "first" || name == "rest" ||
           name == "count";
}

bool hasLocalName(const FunctionOverload& overload, const std::string& name) {
    for (const ParamDef& param : overload.source.params)
        if (param.name == name) return true;
    for (const OutputDef& output : overload.source.outputs)
        if (output.name == name) return true;
    for (const Binding& binding : overload.body)
        for (const std::string& target : binding.targets)
            if (target == name) return true;
    for (const AliasDef& alias : overload.aliases)
        if (alias.name == name) return true;
    return false;
}

bool wordMakesUnstable(const std::string& word, const FunctionOverload& overload,
                       const std::vector<Fn>& functions) {
    // Локальное имя/спецформа — не вызов. Для алиаса смотрим на цель,
    // для функции уровня/встроенной — на её stable-флаг.
    if (hasLocalName(overload, word)) return false;
    if (isSpecialName(word)) return false;
    for (const AliasDef& alias : overload.aliases) {
        if (alias.name == word) {
            const Fn* target = findFn(functions, alias.function);
            if (target != nullptr) return !target->isStable();
            return !fmath::isStable(alias.function);
        }
    }
    const Fn* function = findFn(functions, word);
    if (function != nullptr) return !function->isStable();
    if (fmath::hasFunction(word)) return !fmath::isStable(word);
    return false;
}

bool isPrefixFormName(const std::string& name, const FunctionOverload& overload,
                      const std::vector<Fn>& functions) {
    // Имена, которые старый префиксный парсер умеет читать в начале
    // выражения: специальные формы, алиасы, функции уровня и встроенные.
    if (isReservedLocalName(name)) return true;
    if (findAlias(overload, name) != nullptr) return true;
    if (findFn(functions, name) != nullptr) return true;
    return fmath::hasFunction(name);
}

bool normalizeBodyExpressions(std::vector<Fn>& custom,
                              const std::vector<Fn>& functions,
                              std::string& error) {
    // Тела функций разбираются до того, как становятся известны все
    // пользовательские функции. Поэтому инфикс нормализуется вторым проходом:
    // к этому моменту можно отличить префиксный вызов sum 1 2 от переменной
    // a в выражении a - b.
    for (Fn& function : custom) {
        for (auto& overload : function.overloads) {
            for (Binding& binding : overload->body) {
                const std::string text = trim(binding.rawRhs);
                if (text.empty()) continue;

                const std::vector<std::string> firstWords =
                    helper::wordsFromString(text);
                const bool prefixForm =
                    !firstWords.empty() &&
                    isPrefixFormName(firstWords[0], *overload, functions);

                // Старый префиксный вызов без инфикса и без f(...) не
                // трогаем: так сохраняются прежние ошибки, отрицательные
                // литералы и необязательные скобки/запятые.
                if (prefixForm && !expression::hasInfixOperator(text) &&
                    !expression::hasCallSyntax(text))
                    continue;

                std::vector<std::string> normalized;
                std::string expressionError;
                if (!expression::normalize(text, normalized, expressionError)) {
                    error = blockError(overload->source,
                                       "body: " + expressionError);
                    return false;
                }
                binding.rhs = std::move(normalized);
            }
        }
    }
    return true;
}

std::string autoDescription(const Fn& function) {
    if (function.overloads.empty()) return function.name;
    const FunctionOverload& overload = *function.overloads.front();
    std::string result = function.name + "(";
    for (std::size_t i = 0; i < overload.source.params.size(); ++i) {
        if (i > 0) result += ", ";
        result += overload.source.params[i].name + ": " +
                  valueTypeName(overload.source.params[i].type);
    }
    result += ") -> ";
    for (std::size_t i = 0; i < overload.source.outputs.size(); ++i) {
        if (i > 0) result += " ";
        result += overload.source.outputs[i].name + ": " +
                  formatResultSpec(overload.source.outputs[i].spec);
    }
    return result;
}

}  // namespace

std::string unknownDescription(const std::string& name, int numberOfArgs) {
    std::string result = name + "(";
    for (int i = 0; i < numberOfArgs; ++i) {
        if (i < 26) {
            result += static_cast<char>('a' + i);
        } else {
            result += "x" + std::to_string(i);
        }
        if (i != numberOfArgs - 1) result += ", ";
    }
    result += ") - неизвестная функция";
    return result;
}

std::vector<Num> Fn::use(const std::vector<Num>& args,
                         const std::vector<Fn>* functions) const {
    // Обычный вызов: для простейшей — напрямую в реестр; для пользовательской
    // — выбрать перегрузку и выполнить тело в детерминированном режиме.
    if (!isCustom()) return fmath::apply(behavior, args);
    if (functions == nullptr)
        throw std::runtime_error("для пользовательской функции нужно окружение функций");
    std::shared_ptr<FunctionOverload> overload;
    std::string error;
    if (!selectOverload(*this, args, overload, error))
        throw std::runtime_error(error);
    ResultList results;
    if (!runBody(*overload, args, *functions, false, 0, results, error))
        throw std::runtime_error("функция " + name + ": " + error);
    if (results.empty()) throw std::runtime_error("функция " + name + ": нет результатов");
    return results.front();
}

std::vector<std::vector<Num>> Fn::outcomes(
    const std::vector<Num>& args, const std::vector<Fn>* functions) const {
    // Для простейшей — исходы реестра. Для пользовательской — все миры,
    // которые порождает тело. Stable-перегрузка всё равно может дать
    // несколько результатов из-за потока, поэтому результат — список списков.
    if (!isCustom()) return fmath::outcomes(behavior, args);
    if (functions == nullptr)
        throw std::runtime_error("для пользовательской функции нужно окружение функций");
    std::shared_ptr<FunctionOverload> overload;
    std::string error;
    if (!selectOverload(*this, args, overload, error))
        throw std::runtime_error(error);
    ResultList results;
    bool all = !overload->stable;
    if (!runBody(*overload, args, *functions, all, 0, results, error))
        throw std::runtime_error("функция " + name + ": " + error);
    return results;
}

bool Fn::isStable() const {
    // Пользовательская функция стабильна, только если стабильны все её
    // перегрузки. Стабильность перегрузок уже посчитана транзитивно в
    // compileFunctionBlocks.
    if (!isCustom()) return fmath::isStable(behavior);
    for (const auto& overload : overloads)
        if (!overload->stable) return false;
    return true;
}

Fn makeKnownFunction(const std::string& name) {
    Fn f;
    f.name = name;
    f.behavior = name;
    f.numberOfArgs = fmath::functionArity(name);
    f.description = fmath::describe(name);
    return f;
}

Fn makeUnknownFunction(const std::string& name, const std::string& behavior) {
    Fn f;
    f.name = name;
    f.behavior = behavior;
    f.numberOfArgs = fmath::functionArity(behavior);
    f.description = unknownDescription(name, f.numberOfArgs);
    return f;
}

bool compileFunctionBlocks(const std::vector<FunctionBlock>& blocks,
                           std::vector<Fn>& functions, std::string& error) {
    // Компиляция всех пользовательских блоков уровня.
    //
    // 1. Сгруппировать блоки по имени (перегрузки).
    // 2. Проверить структуру каждой перегрузки.
    // 3. Добавить Fn в functions.
    // 4. Проверить алиасы (могут ссылаться на функции, объявленные позже).
    // 5. Проверить конфликты локальных имён с функциями.
    // 6. Распространить нестабильность до неподвижной точки.
    if (blocks.empty()) return true;

    std::vector<std::string> order;
    std::map<std::string, std::vector<const FunctionBlock*>> groups;
    for (const FunctionBlock& block : blocks) {
        // Имя пользовательской функции не должно пересекаться с
        // зарезервированными формами, встроенными функциями и функциями,
        // уже объявленными в уровне.
        if (!isIdentifier(block.name)) {
            error = blockError(block, "некорректное имя функции");
            return false;
        }
        if (isSpecialName(block.name)) {
            error = blockError(block, "имя зарезервировано: " + block.name);
            return false;
        }
        if (fmath::hasFunction(block.name)) {
            error = blockError(block, "имя совпадает с простейшей функцией: " + block.name);
            return false;
        }
        if (findFn(functions, block.name) != nullptr) {
            error = blockError(block, "функция уже есть в уровне: " + block.name);
            return false;
        }
        if (!groups.count(block.name)) order.push_back(block.name);
        groups[block.name].push_back(&block);
    }

    std::vector<Fn> custom;
    for (const std::string& name : order) {
        Fn function;
        function.name = name;
        function.behavior.clear();
        int arity = -1;
        std::set<std::string> signatures;
        std::string description;
        for (const FunctionBlock* block : groups[name]) {
            auto overload = std::make_shared<FunctionOverload>();
            std::string overloadError;
            if (!prepareOverload(*block, *overload, overloadError)) {
                error = overloadError;
                return false;
            }
            // Все перегрузки одной функции должны иметь одинаковую арность:
            // numberOfArgs — одно число на Fn, и его использует парсер
            // выражений для набора аргументов.
            if (arity < 0) arity = static_cast<int>(overload->inputTypes.size());
            if (arity != static_cast<int>(overload->inputTypes.size())) {
                error = blockError(*block, "у перегрузок разная арность");
                return false;
            }
            // Сигнатура только по типам входов; повторная перегрузка с теми
            // же типами не имеет смысла.
            std::string signature;
            for (ValueType type : overload->inputTypes)
                signature += valueTypeName(type) + ";";
            if (!signatures.insert(signature).second) {
                error = blockError(*block, "перегрузка с такими входными типами уже есть");
                return false;
            }
            if (description.empty() && !block->description.empty())
                description = block->description;
            function.overloads.push_back(std::move(overload));
        }
        function.numberOfArgs = arity < 0 ? 0 : arity;
        function.description = description.empty() ? autoDescription(function) : description;
        custom.push_back(std::move(function));
    }

    for (Fn& function : custom) functions.push_back(function);

    // Алиасы могут ссылаться и на функции, объявленные позже, поэтому
    // проверяем их после добавления всех пользовательских функций.
    for (Fn& function : custom) {
        for (auto& overload : function.overloads) {
            for (const AliasDef& alias : overload->aliases) {
                std::string aliasError;
                if (!aliasTargetExists(alias, functions, aliasError)) {
                    error = blockError(overload->source, aliasError);
                    return false;
                }
            }
        }
    }

    // Имена локальных значений не должны совпадать с функциями: иначе
    // выражение "sum" всегда вызовет функцию, а не прочитает переменную.
    for (Fn& function : custom) {
        for (auto& overload : function.overloads) {
            std::vector<std::string> localNames;
            for (const ParamDef& param : overload->source.params)
                localNames.push_back(param.name);
            for (const OutputDef& output : overload->source.outputs)
                localNames.push_back(output.name);
            for (const Binding& binding : overload->body)
                for (const std::string& target : binding.targets)
                    localNames.push_back(target);
            for (const AliasDef& alias : overload->aliases)
                localNames.push_back(alias.name);
            for (const std::string& name : localNames) {
                if (isReservedLocalName(name) || fmath::hasFunction(name) ||
                    findFn(functions, name) != nullptr) {
                    error = blockError(overload->source,
                                       "имя значения совпадает с функцией: " + name);
                    return false;
                }
            }
        }
    }

    // Инфикс в body нормализуем после проверки имён: теперь известны все
    // функции и алиасы, поэтому можно выбрать между старым префиксом и новым
    // разбором.
    if (!normalizeBodyExpressions(custom, functions, error)) return false;

    // Стабильность зависит от вызываемых функций. Изначально все
    // пользовательские функции считаются стабильными; затем распространяем
    // нестабильность по зависимостям до неподвижной точки.
    bool changed = true;
    while (changed) {
        changed = false;
        for (Fn& function : custom) {
            for (auto& overload : function.overloads) {
                bool stable = true;
                for (const Binding& binding : overload->body) {
                    for (const std::string& word : binding.rhs) {
                        if (wordMakesUnstable(word, *overload, functions)) {
                            stable = false;
                            break;
                        }
                    }
                    if (!stable) break;
                }
                if (overload->stable != stable) {
                    overload->stable = stable;
                    changed = true;
                }
            }
        }
    }
    return true;
}

bool validateFunctionBlock(const FunctionBlock& block, std::string& error) {
    // Валидация без добавления в уровень: используется редактором --function.
    // Компилируем во временный vector, чтобы не менять состояние игры.
    std::vector<Fn> functions;
    std::vector<FunctionBlock> blocks = {block};
    return compileFunctionBlocks(blocks, functions, error);
}

std::string formatResultSpec(const fmath::ResultSpec& spec) {
    std::string result;
    for (ValueType type : spec.fixed) {
        if (!result.empty()) result += " ";
        result += valueTypeName(type);
    }
    if (spec.hasStream) {
        if (!result.empty()) result += " ";
        result += valueTypeName(spec.streamType) + "...";
    }
    return result.empty() ? "?" : result;
}

bool parseResultType(const std::string& text, fmath::ResultSpec& out,
                     std::string& error) {
    std::string value = trim(text);
    if (value.empty()) {
        error = "пустой тип результата";
        return false;
    }
    out = fmath::ResultSpec{};
    bool stream = false;
    if (value.size() > 3 && value.substr(value.size() - 3) == "...") {
        stream = true;
        value = trim(value.substr(0, value.size() - 3));
    }
    ValueType type;
    if (!valueTypeFromName(value, type)) {
        error = "неизвестный тип результата: " + text;
        return false;
    }
    if (stream) {
        out.hasStream = true;
        out.streamType = type;
    } else {
        out.fixed.push_back(type);
    }
    return true;
}

std::string formatFunctionBlock(const FunctionBlock& block) {
    // Обратная сериализация для save/--function. Формат должен совпадать с
    // тем, что понимает parseFunctionBlock; body-строки хранятся уже
    // обрезанными, поэтому добавляем отступ.
    std::string content;
    content += "function " + block.name + "\n";
    if (!block.description.empty())
        content += "  description " + block.description + "\n";
    for (const ParamDef& param : block.params)
        content += "  in " + param.name + ": " + valueTypeName(param.type) + "\n";
    for (const OutputDef& output : block.outputs)
        content += "  out " + output.name + ": " + formatResultSpec(output.spec) + "\n";
    for (const std::string& line : block.useLines) {
        std::string value = trim(line);
        if (value.rfind("use ", 0) == 0) value = trim(value.substr(4));
        content += "  use " + value + "\n";
    }
    content += "  body\n";
    for (const std::string& line : block.bodyLines)
        content += "    " + trim(line) + "\n";
    content += "end\n";
    return content;
}
