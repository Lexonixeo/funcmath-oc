#include "Functions.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <random>

#include "Helper.h"

namespace fmath {

namespace {

std::mt19937_64& rng() {
    static std::mt19937_64 gen(std::random_device{}());
    return gen;
}

std::map<std::string, std::string>& descriptionMap() {
    static std::map<std::string, std::string> m;
    return m;
}

// Ограничение на величины, чтобы избежать переполнений и UB.
constexpr Num kLimit = 1000000000000000000LL;  // 1e18

// Максимум значений, перебираемых за один вызов случайной функции.
// Пока диапазоны в уровнях малы, перебор полный; для гигантских диапазонов
// значения прореживаются, чтобы поиск не раздувался. Это единственное место,
// которое надо поменять, если позже понадобится другая стратегия.
constexpr Num kMaxOutcomes = 4096;

Num clampLD(long double v) {
    if (v > static_cast<long double>(kLimit)) return kLimit;
    if (v < -static_cast<long double>(kLimit)) return -kLimit;
    return static_cast<Num>(v);
}

// --- Реализации функций (порт SimplestFunctions) -------------------------

Num opSum(Num a, Num b) { return clampLD(static_cast<long double>(a) + b); }
Num opSub(Num a, Num b) { return clampLD(static_cast<long double>(a) - b); }
Num opMul(Num a, Num b) { return clampLD(static_cast<long double>(a) * b); }

Num opAbs(Num a, Num) { return a < 0 ? -a : a; }

// Специальные функции из раннего однофайлового прототипа. Там они были
// «неизвестными» A и B; теперь доступны как обычные поведения, чтобы
// сохранить старые уровни.
Num opFive(Num, Num) { return 5; }  // игнорирует аргументы
Num opNeg2(Num a, Num) { return clampLD(2.0L - static_cast<long double>(a)); }

Num opMod(Num a, Num b) {
    if (b == 0 || b == -1) return 0;  // b == -1: остаток всегда 0
    Num m = b < 0 ? -b : b;
    Num r = a % m;
    if (r < 0) r += m;
    return r;
}

Num opDiv(Num a, Num b) {
    if (b == 0) return 0;
    Num mod = opMod(a, b);
    return (a - mod) / b;  // величины ограничены kLimit, переполнения нет
}

Num opAvg(Num a, Num b) { return (a + b) / 2; }

Num opPow(Num a, Num b) {
    if (b < 0) return 0;
    if (a == 1) return 1;
    if (a == 0) return b == 0 ? 1 : 0;
    if (a == -1) return (b % 2 == 0) ? 1 : -1;
    Num result = 1;
    for (Num i = 0; i < b; ++i) {
        long double est = static_cast<long double>(result) * a;
        if (est > static_cast<long double>(kLimit) ||
            est < -static_cast<long double>(kLimit))
            return clampLD(est);
        result = static_cast<Num>(est);
    }
    return result;
}

Num opRand(Num a, Num b) { return randBetween(a, b); }

// Побитовые операции — как в Java (32-битные int).
int32_t to32(Num x) { return static_cast<int32_t>(x); }

Num opAnd(Num a, Num b) { return static_cast<Num>(to32(a) & to32(b)); }
Num opOr(Num a, Num b) { return static_cast<Num>(to32(a) | to32(b)); }
Num opNot(Num a, Num) { return static_cast<Num>(~to32(a)); }
Num opXor(Num a, Num b) { return static_cast<Num>(to32(a) ^ to32(b)); }

Num opSumr(Num a, Num b) { return opSum(a, b) + randBetween(-10, 10); }
Num opSubr(Num a, Num b) { return opSub(a, b) + randBetween(-10, 10); }
Num opMulr(Num a, Num b) { return opMul(a, b) + randBetween(-25, 25); }
Num opDivr(Num a, Num b) { return opDiv(a, b) + randBetween(-25, 25); }

Num opConc(Num a, Num b) {
    std::string s = std::to_string(a) + std::to_string(b);
    long long v;
    if (!helper::parseInt(s, v)) return 0;
    return v;
}

Num opBase(Num a, Num b) {
    // Перевод записи числа a в системе счисления b в десятичное.
    if (b < 2 || b > 36) return 0;
    std::string s = std::to_string(a);
    bool neg = false;
    std::size_t i = 0;
    if (!s.empty() && (s[0] == '-' || s[0] == '+')) {
        neg = (s[0] == '-');
        i = 1;
    }
    Num result = 0;
    for (; i < s.size(); ++i) {
        char c = s[i];
        int d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'z') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'Z') d = c - 'A' + 10;
        else return 0;
        if (d >= b) return 0;
        result = result * b + d;
    }
    return neg ? -result : result;
}

Num opRound(Num a, Num b) {
    if (b < 0) b = 0;
    Num c = opPow(10, b);
    if (c == 0) c = 1;
    Num preans1 = a / c * c;
    Num preans2 = preans1 + c;
    Num d1 = preans2 - a, d2 = a - preans1;
    if (d1 < 0) d1 = -d1;
    if (d2 < 0) d2 = -d2;
    return (d1 > d2) ? preans1 : preans2;
}

std::map<std::string, Op> buildOps() {
    return {
        {"sum", opSum},   {"sub", opSub},   {"mul", opMul},   {"div", opDiv},
        {"mod", opMod},   {"abs", opAbs},   {"pow", opPow},   {"rand", opRand},
        {"and", opAnd},   {"or", opOr},     {"not", opNot},   {"xor", opXor},
        {"avg", opAvg},   {"sumr", opSumr}, {"subr", opSubr}, {"mulr", opMulr},
        {"divr", opDivr}, {"conc", opConc}, {"base", opBase}, {"round", opRound},
        {"five", opFive}, {"neg2", opNeg2},
    };
}

const std::vector<std::string> kGenerationPool = {
    "sum", "sub", "mul", "div", "mod", "abs", "pow", "avg", "and", "or", "xor", "conc"};

}  // namespace

const std::map<std::string, Op>& ops() {
    static const std::map<std::string, Op> registry = buildOps();
    return registry;
}

void loadDescriptions(const std::string& path) {
    descriptionMap() = helper::readValues(path);
}

std::string describe(const std::string& name) {
    auto it = descriptionMap().find(name);
    if (it == descriptionMap().end()) return name;
    return it->second;
}

bool isStable(const std::string& name) {
    return !(name == "rand" || name == "sumr" || name == "subr" || name == "mulr" ||
             name == "divr");
}

std::vector<Num> outcomes(const std::string& name, Num a, Num b) {
    std::vector<Num> result;

    if (name == "rand") {
        if (a > b) std::swap(a, b);
        long double span = static_cast<long double>(b) - static_cast<long double>(a) + 1.0L;
        if (span <= 0.0L) {
            result.push_back(a);
            return result;
        }
        if (span <= static_cast<long double>(kMaxOutcomes)) {
            // Полный перебор диапазона [a, b].
            result.reserve(static_cast<std::size_t>(span));
            for (Num v = a;; ++v) {
                result.push_back(v);
                if (v == b) break;
            }
        } else {
            result.reserve(static_cast<std::size_t>(kMaxOutcomes));
            for (Num k = 0; k < kMaxOutcomes; ++k) {
                long double t = static_cast<long double>(k) /
                                static_cast<long double>(kMaxOutcomes - 1);
                result.push_back(static_cast<Num>(
                    static_cast<long double>(a) + t * (span - 1.0L)));
            }
        }
        return result;
    }

    // Функции «с погрешностью»: базовое значение ± delta.
    Num base = 0;
    Num delta = 0;
    if (name == "sumr") {
        base = opSum(a, b);
        delta = 10;
    } else if (name == "subr") {
        base = opSub(a, b);
        delta = 10;
    } else if (name == "mulr") {
        base = opMul(a, b);
        delta = 25;
    } else if (name == "divr") {
        base = opDiv(a, b);
        delta = 25;
    } else {
        // Стабильная функция (или неизвестная): единственный результат.
        auto it = ops().find(name);
        if (it == ops().end()) return result;
        result.push_back(it->second(a, b));
        return result;
    }
    result.reserve(static_cast<std::size_t>(2 * delta + 1));
    for (Num k = -delta; k <= delta; ++k)
        result.push_back(clampLD(static_cast<long double>(base) + k));
    return result;
}

Num randBetween(Num a, Num b) {
    if (a > b) std::swap(a, b);
    Num span = b - a + 1;
    if (span <= 0) return a;
    std::uniform_int_distribution<Num> dist(0, span - 1);
    return a + dist(rng());
}

const std::vector<std::string>& generationPool() { return kGenerationPool; }

}  // namespace fmath
