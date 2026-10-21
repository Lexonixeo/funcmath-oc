#pragma once
// Именованная функция уровня — порт класса Function.
//
// Функция может быть:
//   * известной (поведение = одна из простейших функций реестра);
//   * неизвестной (A, B, C...): игрок видит только описание, а поведение
//     скрыто;
//   * пользовательской: тело записано в .lvl блоками function/in/out/body/end.
//
// Пользовательская функция может иметь несколько перегрузок по типам входов.
// Тело вычисляется интерпретатором в Function.cpp; это осознанный первый шаг,
// а полноценная компиляция с статической проверкой типов появится позже.

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Functions.h"

struct ParamDef {
    std::string name;
    ValueType type = ValueType::Integer;
};

struct OutputDef {
    std::string name;
    fmath::ResultSpec spec;
};

// Исходный блок пользовательской функции. Хранится и для вычисления, и для
// обратной записи в .lvl без потери форматирования.
//
// Это «сырые» строки из парсера. Проверенная и подготовленная форма —
// FunctionOverload ниже. Такое разделение позволяет:
//   * точно показать ошибку с номером строки;
//   * сохранить блок обратно в файл;
//   * не тащить парсер в runtime.
struct FunctionBlock {
    std::string name;
    std::string description;
    std::vector<ParamDef> params;
    std::vector<OutputDef> outputs;
    std::vector<std::string> useLines;   // строки после use (без ключевого слова)
    std::vector<std::string> bodyLines;  // строки body (без ключевого слова)
    int line = 0;                        // строка начала блока в .lvl (для ошибок)
};

// Разобранный алиас: use d = div integer integer -> integer natural.
// Алиас — это локальное имя для конкретной сигнатуры, чтобы снять
// неоднозначность перегрузки и зафиксировать типы аргументов.
struct AliasDef {
    std::string name;
    std::string function;
    std::vector<ValueType> args;
    fmath::ResultSpec results;
};

// Одно присваивание в теле: q, r = take 2 primes n.
// targets могут ссылаться на выходы или временные переменные; rhs — уже
// разбитое на слова префиксное выражение.
struct Binding {
    std::vector<std::string> targets;
    std::vector<std::string> rhs;
    std::string source;  // исходная строка — для диагностики
};

// Готовая к вычислению перегрузка. Все проверки структуры уже выполнены.
struct FunctionOverload {
    FunctionBlock source;
    std::vector<ValueType> inputTypes;
    fmath::ResultSpec results;
    std::vector<AliasDef> aliases;
    std::vector<Binding> body;
    // Стабильность вычисляется транзитивно: вызов rand или нестабильной
    // пользовательской функции делает нестабильной всю перегрузку.
    bool stable = true;
};

struct Fn {
    std::string name;         // имя, которое вводит игрок (sum, A, B, ...)
    std::string description;  // строка для показа игроку
    std::string behavior;     // ключ в fmath::registry(), для простейших функций
    int numberOfArgs = 2;
    // Пусто для простейших функций. Несколько элементов — перегрузки одной
    // пользовательской функции; выбираются по типам аргументов.
    std::vector<std::shared_ptr<FunctionOverload>> overloads;

    bool isCustom() const { return !overloads.empty(); }

    // functions нужен пользовательским функциям, чтобы вызывать другие
    // функции уровня. Для простейших он игнорируется.
    // use() — обычное вычисление (первый допустимый исход).
    std::vector<Num> use(const std::vector<Num>& args,
                         const std::vector<Fn>* functions = nullptr) const;
    // outcomes() — все исходы случайности; для стабильной функции один.
    std::vector<std::vector<Num>> outcomes(
        const std::vector<Num>& args,
        const std::vector<Fn>* functions = nullptr) const;
    bool isStable() const;
};

// Известная функция: поведение и описание берутся по имени.
Fn makeKnownFunction(const std::string& name);

// Неизвестная функция: своё имя, поведение — заданная функция.
Fn makeUnknownFunction(const std::string& name, const std::string& behavior);

// Описание вида "A(a, b) - неизвестная функция".
std::string unknownDescription(const std::string& name, int numberOfArgs);

// Разбор и проверка пользовательских блоков. Добавляет Fn в functions.
// Ошибка содержит имя функции и строку исходника, если её можно определить.
bool compileFunctionBlocks(const std::vector<FunctionBlock>& blocks,
                           std::vector<Fn>& functions, std::string& error);

// Проверка одного блока без добавления в уровень (режим --function).
bool validateFunctionBlock(const FunctionBlock& block, std::string& error);

// Текст блока в формате .lvl.
std::string formatFunctionBlock(const FunctionBlock& block);

// Текст описания результата: "integer", "natural...", "integer natural...".
std::string formatResultSpec(const fmath::ResultSpec& spec);

// Разбирает тип результата, например "natural..." (поток) или "integer".
bool parseResultType(const std::string& text, fmath::ResultSpec& out,
                     std::string& error);
