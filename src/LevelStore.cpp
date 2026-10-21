#include "LevelStore.h"

#include <algorithm>
#include <filesystem>
#include <set>

#include "Helper.h"

namespace levelstore {

namespace {

bool isBlockKey(const std::string& key) {
    return key == "description" || key == "in" || key == "out" || key == "use" ||
           key == "body";
}

bool startsBlock(const std::vector<std::string>& lines, std::size_t index) {
    for (std::size_t i = index + 1; i < lines.size(); ++i) {
        std::string line = helper::trim(lines[i]);
        if (line.empty() || line[0] == '#') continue;
        std::vector<std::string> words = helper::wordsFromString(line);
        return !words.empty() && isBlockKey(words[0]);
    }
    return false;
}

std::string restAfter(const std::string& line, const std::string& key) {
    std::size_t pos = line.find(key);
    if (pos == std::string::npos) return {};
    return helper::trim(line.substr(pos + key.size()));
}

bool parseInputTypeToken(const std::string& text, ValueType& type,
                         std::string& error) {
    std::string value = helper::trim(text);
    if (!valueTypeFromName(value, type)) {
        error = "неизвестный тип параметра: " + text;
        return false;
    }
    return true;
}

bool parseFunctionBlock(const std::vector<std::string>& lines, std::size_t start,
                        FunctionBlock& block, std::size_t& next, std::string& error) {
    std::vector<std::string> header = helper::wordsFromString(helper::trim(lines[start]));
    block = FunctionBlock{};
    block.name = header[1];
    block.line = static_cast<int>(start + 1);

    bool inBody = false;
    bool closed = false;
    std::size_t i = start + 1;
    for (; i < lines.size(); ++i) {
        std::string line = helper::trim(lines[i]);
        if (line.empty() || line[0] == '#') continue;
        if (line == "end") {
            closed = true;
            ++i;
            break;
        }
        if (inBody) {
            block.bodyLines.push_back(line);
            continue;
        }
        std::vector<std::string> words = helper::wordsFromString(line);
        if (words.empty()) continue;
        const std::string& key = words[0];
        if (key == "description") {
            block.description = restAfter(line, key);
        } else if (key == "in") {
            if (words.size() != 3) {
                error = "ожидается 'in имя: тип': " + line;
                return false;
            }
            ParamDef param;
            param.name = words[1];
            if (!param.name.empty() && param.name.back() == ':')
                param.name.pop_back();
            if (param.name.empty()) {
                error = "пустое имя параметра: " + line;
                return false;
            }
            if (!parseInputTypeToken(words[2], param.type, error)) return false;
            block.params.push_back(std::move(param));
        } else if (key == "out") {
            if (words.size() != 3) {
                error = "ожидается 'out имя: тип': " + line;
                return false;
            }
            OutputDef output;
            output.name = words[1];
            if (!output.name.empty() && output.name.back() == ':')
                output.name.pop_back();
            if (output.name.empty()) {
                error = "пустое имя выхода: " + line;
                return false;
            }
            if (!parseResultType(words[2], output.spec, error)) return false;
            block.outputs.push_back(std::move(output));
        } else if (key == "use") {
            block.useLines.push_back(restAfter(line, key));
        } else if (key == "body") {
            inBody = true;
            std::string inlineBody = restAfter(line, key);
            if (!inlineBody.empty()) block.bodyLines.push_back(inlineBody);
        } else {
            error = "неизвестный ключ в блоке функции: " + line;
            return false;
        }
    }
    if (!closed) {
        error = "у функции " + block.name + " нет end";
        return false;
    }
    next = i;
    return true;
}

bool validName(const std::string& name) {
    if (name.empty()) return false;
    long long ignored;
    if (helper::parseInt(name, ignored)) return false;
    return name.find('=') == std::string::npos;
}

}  // namespace

bool load(const std::string& path, LevelData& out, std::string& error) {
    std::string text = helper::readFile(path);
    if (text.empty() && !helper::fileExists(path)) {
        error = "файл не найден: " + path;
        return false;
    }
    out = LevelData{};
    bool haveNumbers = false, haveAnswer = false;
    std::vector<FunctionBlock> customBlocks;
    std::set<std::string> names;
    std::vector<std::string> lines = helper::splitLines(text);

    for (std::size_t i = 0; i < lines.size(); ++i) {
        std::string line = helper::trim(lines[i]);
        if (line.empty() || line[0] == '#') continue;
        std::vector<std::string> words = helper::wordsFromString(line);
        if (words.empty() || words[0].empty()) continue;
        const std::string& key = words[0];

        if (key == "number") {
            if (words.size() != 3 && words.size() != 5) {
                error = "ожидается 'number <тип> <значение> [as <имя>]': " + line;
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
            std::string name;
            if (words.size() == 5) {
                if (words[3] != "as") {
                    error = "ожидается 'as <имя>' в строке: " + line;
                    return false;
                }
                name = words[4];
                if (!validName(name)) {
                    error = "некорректное имя числа: " + line;
                    return false;
                }
                if (!names.insert(name).second) {
                    error = "имя числа повторяется: " + name;
                    return false;
                }
            }
            out.numbers.push_back(Value::of(type, value));
            out.names.push_back(name);
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
            for (long long value : nums) {
                out.numbers.push_back(Value::integer(value));
                out.names.push_back({});
            }
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
            if (words.size() >= 4 && (words[2] == "file" || words[2] == "fn")) {
                // function myfn file myfn  -> data/functions/myfn.fn
                std::filesystem::path levelFile(path);
                std::string dataDir =
                    levelFile.parent_path().parent_path().string();
                std::string fileName = words[3];
                if (fileName.size() < 3 ||
                    fileName.substr(fileName.size() - 3) != ".fn")
                    fileName += ".fn";
                std::string functionPath = dataDir + "/functions/" + fileName;
                std::string functionText = helper::readFile(functionPath);
                if (functionText.empty() && !helper::fileExists(functionPath)) {
                    error = "файл функции не найден: " + functionPath;
                    return false;
                }
                std::vector<std::string> functionLines =
                    helper::splitLines(functionText);
                std::size_t start = functionLines.size();
                for (std::size_t j = 0; j < functionLines.size(); ++j) {
                    std::vector<std::string> header = helper::wordsFromString(
                        helper::trim(functionLines[j]));
                    if (header.size() >= 2 && header[0] == "function") {
                        start = j;
                        break;
                    }
                }
                if (start == functionLines.size()) {
                    error = "в файле функции нет строки function: " + functionPath;
                    return false;
                }
                FunctionBlock block;
                std::size_t next = 0;
                if (!parseFunctionBlock(functionLines, start, block, next, error))
                    return false;
                block.name = words[1];
                customBlocks.push_back(std::move(block));
            } else if (words.size() == 2 && startsBlock(lines, i)) {
                FunctionBlock block;
                std::size_t next = 0;
                if (!parseFunctionBlock(lines, i, block, next, error)) return false;
                customBlocks.push_back(std::move(block));
                i = next - 1;
            } else {
                const std::string& fname = words[1];
                if (words.size() >= 4 && words[2] == "unknown") {
                    out.functions.push_back(makeUnknownFunction(fname, words[3]));
                } else {
                    out.functions.push_back(makeKnownFunction(fname));
                }
            }
        } else {
            error = "неизвестный ключ в строке: " + line;
            return false;
        }
    }

    if (!haveNumbers || !haveAnswer) {
        error = "уровень неполный (нужны number/numbers и answer)";
        return false;
    }
    if (!customBlocks.empty()) {
        if (!compileFunctionBlocks(customBlocks, out.functions, error)) return false;
    }
    if (out.functions.empty()) {
        error = "уровень неполный (нужна хотя бы одна function)";
        return false;
    }
    for (const std::string& name : out.names) {
        if (name.empty()) continue;
        bool collision = fmath::hasFunction(name);
        for (const Fn& function : out.functions)
            if (function.name == name) collision = true;
        if (collision) {
            error = "имя числа совпадает с функцией: " + name;
            return false;
        }
    }
    if (out.names.size() < out.numbers.size())
        out.names.resize(out.numbers.size());
    return true;
}

bool save(const std::string& path, const LevelData& data) {
    std::string content;
    content += "# func(math) level " + std::to_string(data.number) + "\n";

    bool allInteger = data.answer.isInteger();
    bool haveNames = false;
    for (std::size_t i = 0; i < data.numbers.size(); ++i) {
        if (!data.numbers[i].isInteger()) allInteger = false;
        if (i < data.names.size() && !data.names[i].empty()) haveNames = true;
    }

    if (allInteger && !haveNames) {
        // Старый компактный формат.
        content += "numbers";
        for (const Num& n : data.numbers) content += " " + n.str();
        content += "\n";
        content += "answer " + data.answer.str() + "\n";
    } else {
        for (std::size_t i = 0; i < data.numbers.size(); ++i) {
            content += "number " + data.numbers[i].typeName() + " " +
                       data.numbers[i].str();
            if (i < data.names.size() && !data.names[i].empty())
                content += " as " + data.names[i];
            content += "\n";
        }
        content += "answer " + data.answer.typeName() + " " + data.answer.str() + "\n";
    }

    for (const Fn& f : data.functions) {
        if (f.isCustom()) {
            for (const auto& overload : f.overloads)
                content += formatFunctionBlock(overload->source);
        } else if (f.name == f.behavior) {
            content += "function " + f.name + "\n";
        } else {
            content += "function " + f.name + " unknown " + f.behavior + "\n";
        }
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
