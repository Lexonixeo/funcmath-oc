#include "Function.h"

#include <stdexcept>

std::string unknownDescription(const std::string& name, int numberOfArgs) {
    static const char* values[] = {"a", "b", "c", "d", "e", "f", "g",
                                   "h", "i", "j", "k", "l", "m", "n", "o"};
    std::string sb = name;
    sb += "(";
    for (int i = 0; i < numberOfArgs; ++i) {
        sb += values[i];
        if (i != numberOfArgs - 1) sb += ", ";
    }
    sb += ") - неизвестная функция";
    return sb;
}

Num Fn::use(const std::vector<Num>& args) const {
    const auto& registry = fmath::ops();
    auto it = registry.find(behavior);
    if (it == registry.end()) throw std::runtime_error("unknown function behavior: " + behavior);
    Num a = args.size() > 0 ? args[0] : 0;
    Num b = args.size() > 1 ? args[1] : 0;
    return it->second(a, b);
}

Fn makeKnownFunction(const std::string& name) {
    Fn f;
    f.name = name;
    f.behavior = name;
    f.numberOfArgs = 2;
    f.description = fmath::describe(name);
    return f;
}

Fn makeUnknownFunction(const std::string& name, const std::string& behavior) {
    Fn f;
    f.name = name;
    f.behavior = behavior;
    f.numberOfArgs = 2;
    f.description = unknownDescription(name, f.numberOfArgs);
    return f;
}
