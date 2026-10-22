// Небольшие регрессионные тесты для инфиксного слоя.
//
// Проверяем именно нормализацию в префиксные слова: вычисление значений
// остаётся ответственностью Level/Function и проверяется отдельно через
// игровые сценарии.

#include <iostream>
#include <string>
#include <vector>

#include "Expression.h"

namespace {

int failures = 0;

std::string join(const std::vector<std::string>& words) {
    std::string result;
    for (const std::string& word : words) {
        if (!result.empty()) result += " ";
        result += word;
    }
    return result;
}

void fail(const std::string& test, const std::string& message) {
    ++failures;
    std::cerr << "[FAIL] " << test << ": " << message << '\n';
}

void expectNormalize(const std::string& input,
                     const std::vector<std::string>& expected) {
    std::vector<std::string> actual;
    std::string error;
    if (!expression::normalize(input, actual, error)) {
        fail(input, "не удалось разобрать: " + error);
        return;
    }
    if (actual != expected) {
        fail(input, "ожидалось '" + join(expected) + "', получено '" +
                        join(actual) + "'");
    }
}

void expectNormalizeError(const std::string& input) {
    std::vector<std::string> actual;
    std::string error;
    if (expression::normalize(input, actual, error)) {
        fail(input, "ожидалась ошибка, получено '" + join(actual) + "'");
    } else if (error.empty()) {
        fail(input, "ошибка без текста");
    }
}

void expectInfix(const std::string& input, bool expected) {
    const bool actual = expression::hasInfixOperator(input);
    if (actual != expected) {
        fail(input, std::string("hasInfixOperator: ожидалось ") +
                        (expected ? "true" : "false"));
    }
}

void expectCall(const std::string& input, bool expected) {
    const bool actual = expression::hasCallSyntax(input);
    if (actual != expected) {
        fail(input, std::string("hasCallSyntax: ожидалось ") +
                        (expected ? "true" : "false"));
    }
}

}  // namespace

int main() {
    // Приоритеты и скобки.
    expectNormalize("1 + 2 * 3", {"sum", "1", "mul", "2", "3"});
    expectNormalize("(1 + 2) * 3", {"mul", "sum", "1", "2", "3"});
    expectNormalize("2 * (3 + 4)", {"mul", "2", "sum", "3", "4"});
    expectNormalize("10 / 3", {"div", "10", "3"});
    expectNormalize("2 ** 3 ** 2", {"pow", "2", "pow", "3", "2"});

    // Вызовы функций и вложенность.
    expectNormalize("sum(1, 2)", {"sum", "1", "2"});
    expectNormalize("sum(1, 2) + 3", {"sum", "sum", "1", "2", "3"});
    expectNormalize("f(1 + 2, 3)", {"f", "sum", "1", "2", "3"});
    expectNormalize("f(1 -2, 3)", {"f", "sub", "1", "2", "3"});
    expectNormalize("mul(2, sum(3, 4))", {"mul", "2", "sum", "3", "4"});
    expectNormalize("primes(30)", {"primes", "30"});

    // Унарный минус и типизированные литералы.
    expectNormalize("-3 + 5", {"sum", "-3", "5"});
    expectNormalize("1 - -2", {"sub", "1", "-2"});
    expectNormalize("integer=-3 + 5", {"sum", "integer=-3", "5"});

    // Старые формы не должны выглядеть как инфикс.
    expectInfix("sum 1 2", false);
    expectInfix("sum 1 -2", false);
    expectInfix("take 2 primes 30", false);
    expectInfix("sum(1,2)", false);
    expectInfix("f(-2, 3)", false);
    expectInfix("f(1, -2)", false);
    expectInfix("sum 1 - 2", true);
    expectInfix("f(1 -2, 3)", true);
    expectInfix("1 + 2", true);
    expectInfix("2 ** 3", true);

    // Вызов f(...) без пробела перед скобкой включает новый парсер.
    expectCall("sum(1,2)", true);
    expectCall("f(1 -2, 3)", true);
    expectCall("primes(30)", true);
    expectCall("sum (1,2)", false);
    expectCall("sum 1 2", false);
    expectCall("(f(1,2))", false);

    // Ошибки должны быть ошибками, а не падением.
    expectNormalizeError("sum 1 + 2");
    expectNormalizeError("2 ^ 3");
    expectNormalizeError("-integer=3 + 5");
    expectNormalizeError("(1, 2)");
    expectNormalizeError("f(1,2");
    expectNormalizeError("1 +");
    expectNormalizeError("f()");

    if (failures != 0) {
        std::cerr << failures << " тест(ов) не прошли\n";
        return 1;
    }
    std::cout << "expression_tests: OK\n";
    return 0;
}
