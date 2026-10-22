// SPDX-License-Identifier: MIT
// Helper.cpp
// ----------
// Строковые и файловые утилиты, общие для всех модулей.
//
// Главная особенность — wordsFromString(): она приводит запись в стиле
// "sum(1, 2)" к префиксной форме "sum 1 2". Игровой парсер работает только
// со словами, поэтому скобки и запятые здесь просто заменяются пробелами.
//
// Файлы читаются и пишутся в бинарном режиме: текст считается UTF-8, а
// переводы строк не преобразуются платформой. Это важно для round-trip .lvl.
#include "Helper.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace helper {

std::vector<std::string> wordsFromString(const std::string& str) {
    // 1. Заменяем запятые и скобки на пробелы. Это совместимость со старым
    //    Java-синтаксисом; внутри уровня скобки не несут смысла.
    std::string second;
    second.reserve(str.size());
    for (char c : str) {
        if (c == ',' || c == '(' || c == ')')
            second.push_back(' ');
        else
            second.push_back(c);
    }
    // 2. Схлопываем подряд идущие пробелы.
    std::string third;
    char last = ' ';
    for (char c : second) {
        if (!(c == ' ' && last == ' ')) {
            third.push_back(c);
            last = c;
        }
    }
    // 3. Убираем завершающий пробел.
    if (!third.empty() && third.back() == ' ') third.pop_back();

    // 4. Разбиваем по пробелам. Пустая строка даёт {""} — как в Java-версии.
    std::vector<std::string> words;
    std::string builder;
    for (char c : third) {
        if (c != ' ') {
            builder.push_back(c);
        } else {
            words.push_back(builder);
            builder.clear();
        }
    }
    words.push_back(builder);
    return words;
}

std::string stringFromWords(const std::vector<std::string>& words) {
    std::string sb;
    for (const std::string& w : words) {
        sb += w;
        sb += ' ';
    }
    return sb;
}

bool parseInt(const std::string& s, long long& out) {
    // Строгий разбор: после числа не должно остаться символов ("12x" — ошибка).
    // std::stoll допускает ведущие пробелы, поэтому вызывающий код обычно
    // передаёт уже обрезанную строку.
    if (s.empty()) return false;
    std::size_t pos = 0;
    try {
        long long v = std::stoll(s, &pos);
        if (pos != s.size()) return false;
        out = v;
        return true;
    } catch (...) {
        return false;
    }
}

std::vector<long long> numbersFromWords(const std::vector<std::string>& words, bool& ok) {
    std::vector<long long> numbers;
    ok = true;
    for (const std::string& w : words) {
        long long v;
        if (!parseInt(w, v)) {
            ok = false;
            return {};
        }
        numbers.push_back(v);
    }
    return numbers;
}

void initConsole() {
#ifdef _WIN32
    // Раньше здесь был system("chcp 65001"): он запускал cmd.exe, который
    // вычитывал перенаправленный stdin и приводил к ложному EOF.
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

void clearScreen() {
    // Очистка нужна только в интерактивном терминале. Если stdout перенаправлен
    // (тесты, пайпы), не трогаем поток: иначе вывод становится нечитаемым.
#if defined(_WIN32)
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (h == INVALID_HANDLE_VALUE || h == nullptr ||
        !GetConsoleScreenBufferInfo(h, &csbi)) {
        return;  // вывод перенаправлен — очищать нечего
    }
    DWORD cells = static_cast<DWORD>(csbi.dwSize.X) *
                  static_cast<DWORD>(csbi.dwSize.Y);
    DWORD written = 0;
    COORD home = {0, 0};
    FillConsoleOutputCharacterA(h, ' ', cells, home, &written);
    FillConsoleOutputAttribute(h, csbi.wAttributes, cells, home, &written);
    SetConsoleCursorPosition(h, home);
#else
    if (!isatty(fileno(stdout))) return;
    std::cout << "\033[2J\033[H" << std::flush;
#endif
}

std::string readFile(const std::string& path) {
    // Отсутствующий файл и пустой файл неразличимы по возвращаемому значению;
    // вызывающий код при необходимости проверяет fileExists().
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::string();
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool writeFile(const std::string& path, const std::string& content) {
    // Родительские каталоги создаются здесь, чтобы сохранение уровня/функции
    // не требовало отдельной подготовки директорий.
    fs::path p(path);
    if (p.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(p.parent_path(), ec);
    }
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << content;
    return static_cast<bool>(out);
}

bool appendLine(const std::string& path, const std::string& line) {
    fs::path p(path);
    if (p.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(p.parent_path(), ec);
    }
    std::ofstream out(path, std::ios::binary | std::ios::app);
    if (!out) return false;
    out << line << "\n";
    return true;
}

bool fileExists(const std::string& path) {
    std::error_code ec;
    return fs::exists(path, ec) && fs::is_regular_file(path, ec);
}

int filesCount(const std::string& dir) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return 0;
    int n = 0;
    for (const auto& e : fs::directory_iterator(dir, ec)) {
        if (e.is_regular_file()) n++;
    }
    return n;
}

std::map<std::string, std::string> readValues(const std::string& path) {
    // Формат "ключ остаток строки": ключ — первое слово, значение — весь
    // остаток после него. Используется для data/functions.txt.
    std::map<std::string, std::string> ans;
    std::string text = readFile(path);
    for (const std::string& raw : splitLines(text)) {
        std::string line = raw;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::vector<std::string> words = wordsFromString(line);
        if (words.empty() || words[0].empty()) continue;
        std::string key = words[0];
        std::string value;
        if (line.size() > key.size()) value = line.substr(key.size() + 1);
        ans[key] = value;
    }
    return ans;
}

int countWord(const std::vector<std::string>& words, const std::string& word) {
    int ans = 0;
    for (const std::string& w : words)
        if (w == word) ans++;
    return ans;
}

std::string trim(const std::string& s) {
    std::size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) b++;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) e--;
    return s.substr(b, e - b);
}

std::vector<std::string> splitLines(const std::string& text) {
    // Сохраняем CR в конце строки: trim() затем удалит его как пробельный
    // символ. Так функция одинаково работает с LF и CRLF.
    std::vector<std::string> lines;
    std::string cur;
    for (char c : text) {
        if (c == '\n') {
            lines.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) lines.push_back(cur);
    return lines;
}

std::string resolveDataDir() {
    // Игра может запускаться из корня репозитория или из build/ рядом с
    // скопированным data/. Проверяем несколько уровней вверх, но не ищем
    // data по всему диску.
    std::error_code ec;
    if (fs::is_directory("data", ec)) return "data";
    if (fs::is_directory("../data", ec)) return "../data";
    if (fs::is_directory("../../data", ec)) return "../../data";
    return "data";
}

}  // namespace helper
