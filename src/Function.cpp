#include "Function.h"

#include <string>

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

std::vector<Num> Fn::use(const std::vector<Num>& args) const {
    return fmath::apply(behavior, args);
}

std::vector<std::vector<Num>> Fn::outcomes(const std::vector<Num>& args) const {
    return fmath::outcomes(behavior, args);
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
