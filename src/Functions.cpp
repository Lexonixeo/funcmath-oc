#include "Functions.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <random>
#include <stdexcept>

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
constexpr long long kLimit = 1000000000000000000LL;  // 1e18

// Максимум значений, перебираемых за один вызов случайной функции.
// Пока диапазоны в уровнях малы, перебор полный; для гигантских диапазонов
// значения прореживаются, чтобы поиск не раздувался. Это единственное место,
// которое надо поменять, если позже понадобится другая стратегия.
constexpr long long kMaxOutcomes = 4096;

long long clampLL(long double v) {
    if (v > static_cast<long double>(kLimit)) return kLimit;
    if (v < -static_cast<long double>(kLimit)) return -kLimit;
    return static_cast<long long>(v);
}

// Если хотя бы один аргумент integer, результат integer; natural + natural
// остаётся natural. Позже здесь появится полноценная таблица конверсий.
ValueType commonType(const Value& a, const Value& b) {
    return (a.type() == ValueType::Natural && b.type() == ValueType::Natural)
               ? ValueType::Natural
               : ValueType::Integer;
}

Value make(ValueType type, long long result) {
    if (type == ValueType::Natural && result < 0) {
        throw std::domain_error("natural: отрицательный результат");
    }
    return Value::of(type, result);
}

Value make(ValueType type, long double value) {
    return make(type, clampLL(value));
}

Value addDelta(const Value& base, long long delta) {
    return make(base.type(), static_cast<long double>(base.asLongLong()) + delta);
}

// Остаток как в Java: всегда неотрицательный при b != 0.
long long modLL(long long a, long long b) {
    if (b == 0 || b == -1) return 0;
    long long m = b < 0 ? -b : b;
    long long r = a % m;
    if (r < 0) r += m;
    return r;
}

// --- Реализации функций (порт SimplestFunctions) -------------------------

Value opSum(Value a, Value b) {
    return make(commonType(a, b), static_cast<long double>(a.asLongLong()) +
                                      static_cast<long double>(b.asLongLong()));
}

Value opSub(Value a, Value b) {
    ValueType type = commonType(a, b);
    if (type == ValueType::Natural && a.asLongLong() < b.asLongLong()) {
        throw std::domain_error("natural: нельзя вычесть большее из меньшего");
    }
    return make(type, static_cast<long double>(a.asLongLong()) -
                          static_cast<long double>(b.asLongLong()));
}

Value opMul(Value a, Value b) {
    return make(commonType(a, b), static_cast<long double>(a.asLongLong()) *
                                      static_cast<long double>(b.asLongLong()));
}

Value opAbs(Value a, Value) {
    if (a.isNatural()) return a;
    return make(ValueType::Integer,
                static_cast<long double>(std::llabs(a.asLongLong())));
}

// Специальные функции из раннего однофайлового прототипа. Там они были
// «неизвестными» A и B; теперь доступны как обычные поведения, чтобы
// сохранить старые уровни.
Value opFive(Value a, Value b) { return make(commonType(a, b), 5LL); }
Value opNeg2(Value a, Value b) {
    return make(commonType(a, b),
                2.0L - static_cast<long double>(a.asLongLong()));
}

Value opMod(Value a, Value b) {
    return make(commonType(a, b), static_cast<long double>(
                                      modLL(a.asLongLong(), b.asLongLong())));
}

Value opDiv(Value a, Value b) {
    ValueType type = commonType(a, b);
    if (b == 0) return make(type, 0LL);
    long long x = a.asLongLong();
    long long y = b.asLongLong();
    long long mod = modLL(x, y);
    return make(type, static_cast<long double>((x - mod) / y));
}

Value opAvg(Value a, Value b) {
    return make(commonType(a, b),
                (static_cast<long double>(a.asLongLong()) +
                 static_cast<long double>(b.asLongLong())) /
                    2.0L);
}

Value opPow(Value a, Value b) {
    ValueType type = commonType(a, b);
    long long x = a.asLongLong();
    long long y = b.asLongLong();
    if (y < 0) return make(type, 0LL);
    if (x == 1) return make(type, 1LL);
    if (x == 0) return make(type, y == 0 ? 1LL : 0LL);
    if (x == -1) return make(type, (y % 2 == 0) ? 1LL : -1LL);
    long long result = 1;
    for (long long i = 0; i < y; ++i) {
        long double est = static_cast<long double>(result) * x;
        if (est > static_cast<long double>(kLimit) ||
            est < -static_cast<long double>(kLimit)) {
            return make(type, est);
        }
        result = static_cast<long long>(est);
    }
    return make(type, static_cast<long double>(result));
}

Value opRand(Value a, Value b) {
    long long x = a.asLongLong();
    long long y = b.asLongLong();
    if (x > y) std::swap(x, y);
    return make(commonType(a, b),
                static_cast<long double>(randBetween(x, y)));
}

// Побитовые операции — как в Java (32-битные int).
int32_t to32(const Value& x) {
    return static_cast<int32_t>(x.asLongLong());
}

Value opAnd(Value a, Value b) {
    return make(commonType(a, b), static_cast<long long>(to32(a) & to32(b)));
}
Value opOr(Value a, Value b) {
    return make(commonType(a, b), static_cast<long long>(to32(a) | to32(b)));
}
Value opNot(Value a, Value b) {
    return make(commonType(a, b), static_cast<long long>(~to32(a)));
}
Value opXor(Value a, Value b) {
    return make(commonType(a, b), static_cast<long long>(to32(a) ^ to32(b)));
}

Value opSumr(Value a, Value b) { return addDelta(opSum(a, b), randBetween(-10, 10)); }
Value opSubr(Value a, Value b) { return addDelta(opSub(a, b), randBetween(-10, 10)); }
Value opMulr(Value a, Value b) { return addDelta(opMul(a, b), randBetween(-25, 25)); }
Value opDivr(Value a, Value b) { return addDelta(opDiv(a, b), randBetween(-25, 25)); }

Value opConc(Value a, Value b) {
    std::string s = a.str() + b.str();
    long long v;
    if (!helper::parseInt(s, v)) return make(commonType(a, b), 0LL);
    return make(commonType(a, b), static_cast<long double>(v));
}

Value opBase(Value a, Value b) {
    // Перевод записи числа a в системе счисления b в десятичное.
    long long base = b.asLongLong();
    if (base < 2 || base > 36) return make(commonType(a, b), 0LL);
    std::string s = a.str();
    bool neg = false;
    std::size_t i = 0;
    if (!s.empty() && (s[0] == '-' || s[0] == '+')) {
        neg = (s[0] == '-');
        i = 1;
    }
    long long result = 0;
    for (; i < s.size(); ++i) {
        char c = s[i];
        int d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'z') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'Z') d = c - 'A' + 10;
        else return make(commonType(a, b), 0LL);
        if (d >= base) return make(commonType(a, b), 0LL);
        result = result * base + d;
    }
    return make(commonType(a, b),
                static_cast<long double>(neg ? -result : result));
}

Value opRound(Value a, Value b) {
    ValueType type = commonType(a, b);
    long long places = b.asLongLong();
    if (places < 0) places = 0;
    long long c = 1;
    for (long long i = 0; i < places; ++i) {
        if (c > kLimit / 10) {
            c = kLimit;
            break;
        }
        c *= 10;
    }
    if (c == 0) c = 1;
    long long x = a.asLongLong();
    long long preans1 = (x / c) * c;
    long long preans2 = preans1 + c;
    long long d1 = preans2 - x;
    long long d2 = x - preans1;
    if (d1 < 0) d1 = -d1;
    if (d2 < 0) d2 = -d2;
    return make(type, static_cast<long double>((d1 > d2) ? preans1 : preans2));
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
    ValueType type = commonType(a, b);

    if (name == "rand") {
        long long x = a.asLongLong();
        long long y = b.asLongLong();
        if (x > y) std::swap(x, y);
        long double span = static_cast<long double>(y) - static_cast<long double>(x) + 1.0L;
        if (span <= 0.0L) {
            result.push_back(Value::of(type, x));
            return result;
        }
        if (span <= static_cast<long double>(kMaxOutcomes)) {
            // Полный перебор диапазона [x, y].
            result.reserve(static_cast<std::size_t>(span));
            for (long long v = x;; ++v) {
                result.push_back(Value::of(type, v));
                if (v == y) break;
            }
        } else {
            result.reserve(static_cast<std::size_t>(kMaxOutcomes));
            for (long long k = 0; k < kMaxOutcomes; ++k) {
                long double t = static_cast<long double>(k) /
                                static_cast<long double>(kMaxOutcomes - 1);
                result.push_back(Value::of(
                    type, clampLL(static_cast<long double>(x) + t * (span - 1.0L))));
            }
        }
        return result;
    }

    // Функции «с погрешностью»: базовое значение ± delta.
    Value base;
    long long delta = 0;
    try {
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
    } catch (const std::exception&) {
        // Для strict natural неудачная операция не даёт допустимых исходов.
        return result;
    }

    result.reserve(static_cast<std::size_t>(2 * delta + 1));
    for (long long k = -delta; k <= delta; ++k) {
        long double value = static_cast<long double>(base.asLongLong()) + k;
        long long candidate = clampLL(value);
        if (type == ValueType::Natural && candidate < 0) continue;
        result.push_back(Value::of(type, candidate));
    }
    return result;
}

long long randBetween(long long a, long long b) {
    if (a > b) std::swap(a, b);
    long long span = b - a + 1;
    if (span <= 0) return a;
    std::uniform_int_distribution<long long> dist(0, span - 1);
    return a + dist(rng());
}

const std::vector<std::string>& generationPool() { return kGenerationPool; }

}  // namespace fmath
