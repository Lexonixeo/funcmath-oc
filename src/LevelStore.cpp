#include "LevelStore.h"

#include <algorithm>
#include <filesystem>

#include "Helper.h"

namespace levelstore {

bool load(const std::string& path, LevelData& out, std::string& error) {
    std::string text = helper::readFile(path);
    if (text.empty() && !helper::fileExists(path)) {
        error = "файл не найден: " + path;
        return false;
    }
    out = LevelData{};
    bool haveNumbers = false, haveAnswer = false;
    for (const std::string& raw : helper::splitLines(text)) {
        std::string line = helper::trim(raw);
        if (line.empty() || line[0] == '#') continue;
        std::vector<std::string> words = helper::wordsFromString(line);
        if (words.empty() || words[0].empty()) continue;
        const std::string& key = words[0];
        if (key == "number") {
            if (words.size() != 3) {
                error = "ожидается 'number <тип> <значение>': " + line;
                return false;
            }
            ValueType type;
            long long value;
            if (!valueTypeFromName(words[1], type)) {
                error = "неизвестный тип числа '" + words[1] + "' в строке: " + line;
                return false;
            }
            if (!helper::parseInt(words[2], value)) {
                error = "некорректное число в строке: " + line;
                return false;
            }
            if (type == ValueType::Natural && value < 0) {
                error = "natural не может быть отрицательным: " + line;
                return false;
            }
            out.numbers.push_back(Value::of(type, value));
            haveNumbers = true;
        } else if (key == "numbers") {
            // Старый формат: все числа integer.
            std::vector<std::string> rest(words.begin() + 1, words.end());
            bool ok = false;
            std::vector<long long> nums = helper::numbersFromWords(rest, ok);
            if (!ok) {
                error = "некорректные числа в строке: " + line;
                return false;
            }
            for (long long value : nums) out.numbers.push_back(Value::integer(value));
            haveNumbers = true;
        } else if (key == "answer") {
            ValueType type = ValueType::Integer;
            long long value = 0;
            if (words.size() == 2) {
                if (!helper::parseInt(words[1], value)) {
                    error = "некорректный ответ в строке: " + line;
                    return false;
                }
            } else if (words.size() == 3 && valueTypeFromName(words[1], type)) {
                if (!helper::parseInt(words[2], value)) {
                    error = "некорректный ответ в строке: " + line;
                    return false;
                }
            } else {
                error = "ожидается 'answer [<тип>] <значение>': " + line;
                return false;
            }
            if (type == ValueType::Natural && value < 0) {
                error = "natural-ответ не может быть отрицательным: " + line;
                return false;
            }
            out.answer = Value::of(type, value);
            haveAnswer = true;
        } else if (key == "function") {
            if (words.size() < 2) {
                error = "некорректная строка функции: " + line;
                return false;
            }
            const std::string& fname = words[1];
            if (words.size() >= 4 && words[2] == "unknown") {
                out.functions.push_back(makeUnknownFunction(fname, words[3]));
            } else {
                out.functions.push_back(makeKnownFunction(fname));
            }
        } else {
            error = "неизвестный ключ в строке: " + line;
            return false;
        }
    }
    if (!haveNumbers || !haveAnswer || out.functions.empty()) {
        error = "уровень неполный (нужны number/numbers, answer и хотя бы одна function)";
        return false;
    }
    return true;
}

bool save(const std::string& path, const LevelData& data) {
    std::string content;
    content += "# func(math) level " + std::to_string(data.number) + "\n";

    bool allInteger = data.answer.isInteger();
    for (const Num& n : data.numbers) {
        if (!n.isInteger()) {
            allInteger = false;
            break;
        }
    }

    if (allInteger) {
        // Сохраняем старый компактный формат, чтобы существующие уровни
        // оставались читаемыми.
        content += "numbers";
        for (const Num& n : data.numbers) content += " " + n.str();
        content += "\n";
        content += "answer " + data.answer.str() + "\n";
    } else {
        for (const Num& n : data.numbers)
            content += "number " + n.typeName() + " " + n.str() + "\n";
        content += "answer " + data.answer.typeName() + " " + data.answer.str() + "\n";
    }

    for (const Fn& f : data.functions) {
        if (f.name == f.behavior)
            content += "function " + f.name + "\n";
        else
            content += "function " + f.name + " unknown " + f.behavior + "\n";
    }
    return helper::writeFile(path, content);
}

std::string levelPath(const std::string& dataDir, int number) {
    return dataDir + "/levels/level" + std::to_string(number) + ".lvl";
}

std::vector<int> listLevelNumbers(const std::string& dataDir) {
    std::vector<int> result;
    std::string dir = dataDir + "/levels";
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) return result;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file()) continue;
        std::string fname = entry.path().filename().string();
        if (fname.rfind("level", 0) != 0) continue;
        std::size_t dot = fname.rfind(".lvl");
        if (dot == std::string::npos) continue;
        std::string numStr = fname.substr(5, dot - 5);
        long long n;
        if (!helper::parseInt(numStr, n)) continue;
        result.push_back(static_cast<int>(n));
    }
    std::sort(result.begin(), result.end());
    return result;
}

int nextLevelNumber(const std::string& dataDir) {
    std::vector<int> nums = listLevelNumbers(dataDir);
    if (nums.empty()) return 1;
    return nums.back() + 1;
}

}  // namespace levelstore
