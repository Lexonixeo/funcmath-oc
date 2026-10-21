#pragma once
// Значение в наборе уровня — аналог MathObject из Java-прототипа.
//
// В прототипе MathObject был интерфейсом, а FNatural/FInteger — его
// реализациями. В C++ для решателя важны дешёвое копирование, сравнение и
// хеширование, поэтому здесь используется value-семантика:
//
//   Value = variant<Natural, Integer>
//
// Natural и Integer — отдельные типы-значения, а не наследники. Позже сюда
// можно добавить Rational, Real, Complex и Gaussian как новые альтернативы,
// не меняя уровни и решатель.

#include <cstdint>
#include <ostream>
#include <stdexcept>
#include <string>
#include <variant>

enum class ValueType {
    Natural,
    Integer,
    // Пока не реализованы, но перечислены, чтобы формат и реестр функций
    // можно было расширять без смены базовых типов. Порядок enum участвует
    // в Value::operator<, поэтому новые типы лучше добавлять в конец.
    Rational,
    Real,
    Complex,
    Gaussian,
};

// Обёртки нужны, чтобы variant различал natural и integer с одинаковым
// числовым значением. Обе структуры намеренно тривиальны: копирование и
// сравнение должны быть дешёвыми для решателя.
struct Natural {
    long long value = 0;
};

struct Integer {
    long long value = 0;
};

inline std::string valueTypeName(ValueType type) {
    switch (type) {
        case ValueType::Natural:
            return "natural";
        case ValueType::Integer:
            return "integer";
        case ValueType::Rational:
            return "rational";
        case ValueType::Real:
            return "real";
        case ValueType::Complex:
            return "complex";
        case ValueType::Gaussian:
            return "gaussian";
    }
    return "unknown";
}

inline bool valueTypeFromName(const std::string& name, ValueType& out) {
    if (name == "natural") {
        out = ValueType::Natural;
        return true;
    }
    if (name == "integer") {
        out = ValueType::Integer;
        return true;
    }
    return false;
}

class Value {
public:
    // Голое целое и значение по умолчанию — integer: так старые уровни и
    // bare-литералы в телах функций сохраняют прежнюю семантику.
    Value() : data_(Integer{0}) {}
    Value(long long value) : data_(Integer{value}) {}

    // Единственная точка создания natural: проверка домена выполняется здесь,
    // чтобы отрицательное natural не могло появиться даже случайно.
    static Value natural(long long value) {
        if (value < 0)
            throw std::domain_error("natural не может быть отрицательным");
        return Value(Natural{value});
    }
    static Value integer(long long value) { return Value(Integer{value}); }

    // Создание по динамическому типу (парсеры .lvl, .fn и литералов).
    // Для ещё не реализованных типов бросает invalid_argument.
    static Value of(ValueType type, long long value) {
        switch (type) {
            case ValueType::Natural:
                return natural(value);
            case ValueType::Integer:
                return integer(value);
            default:
                throw std::invalid_argument("тип значения пока не поддерживается: " +
                                            valueTypeName(type));
        }
    }

    ValueType type() const {
        return std::holds_alternative<Natural>(data_) ? ValueType::Natural
                                                       : ValueType::Integer;
    }

    bool isNatural() const { return type() == ValueType::Natural; }
    bool isInteger() const { return type() == ValueType::Integer; }

    // Для natural/integer. Для будущих rational/real/complex будет отдельный
    // доступ к числителю/действительной части.
    long long asLongLong() const {
        return std::visit([](const auto& number) { return number.value; }, data_);
    }

    std::string str() const { return std::to_string(asLongLong()); }
    std::string typeName() const { return valueTypeName(type()); }

    // Сравнение с учётом типа: natural 1 и integer 1 — разные значения.
    // Именно это сравнение использует решатель и проверка цели уровня.
    bool same(const Value& other) const {
        return type() == other.type() && asLongLong() == other.asLongLong();
    }

    bool operator==(const Value& other) const { return same(other); }
    bool operator!=(const Value& other) const { return !same(other); }

    // Полный порядок нужен для сортировки в сигнатуре состояния решателя.
    // Сначала сравнивается величина, затем тип; это не «математический»
    // порядок, а технический, чтобы одинаковые мультимножества сортировались
    // одинаково.
    bool operator<(const Value& other) const {
        if (asLongLong() != other.asLongLong())
            return asLongLong() < other.asLongLong();
        return static_cast<int>(type()) < static_cast<int>(other.type());
    }

    // Хеш обязан согласовываться с same(): тип входит в хеш, иначе
    // natural 1 и integer 1 могли бы считаться одним состоянием в решателе.
    uint64_t hash() const {
        uint64_t h = static_cast<uint64_t>(asLongLong());
        h ^= static_cast<uint64_t>(type()) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        h *= 1099511628211ULL;
        h ^= h >> 29;
        return h;
    }

private:
    explicit Value(Natural number) : data_(number) {}
    explicit Value(Integer number) : data_(number) {}

    std::variant<Natural, Integer> data_;
};

// Числовые сравнения ниже игнорируют тип: они нужны для проверок вида
// «аргумент равен нулю» и для отображения. Для семантики значений
// используйте Value::same().
inline bool operator==(const Value& value, long long number) {
    return value.asLongLong() == number;
}
inline bool operator==(long long number, const Value& value) {
    return value.asLongLong() == number;
}
inline bool operator!=(const Value& value, long long number) {
    return value.asLongLong() != number;
}
inline bool operator!=(long long number, const Value& value) {
    return value.asLongLong() != number;
}
inline bool operator<(const Value& value, long long number) {
    return value.asLongLong() < number;
}
inline bool operator<(long long number, const Value& value) {
    return number < value.asLongLong();
}
inline bool operator>(const Value& value, long long number) {
    return value.asLongLong() > number;
}
inline bool operator>(long long number, const Value& value) {
    return number > value.asLongLong();
}
inline bool operator<=(const Value& value, long long number) {
    return value.asLongLong() <= number;
}
inline bool operator<=(long long number, const Value& value) {
    return number <= value.asLongLong();
}
inline bool operator>=(const Value& value, long long number) {
    return value.asLongLong() >= number;
}
inline bool operator>=(long long number, const Value& value) {
    return number >= value.asLongLong();
}

inline std::ostream& operator<<(std::ostream& out, const Value& value) {
    return out << value.str();
}
