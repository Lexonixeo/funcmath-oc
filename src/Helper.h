#pragma once
// Вспомогательные утилиты: работа со строками, файлами и консолью.
// Порт класса Helper из Java-версии (funcmathjava1).
// Все функции здесь не зависят от игровой логики; ошибки ввода-вывода
// возвращаются пустой строкой/false, а не исключениями.

#include <map>
#include <string>
#include <vector>

namespace helper {

// Разбивает строку на слова. Запятые и круглые скобки считаются разделителями,
// поэтому "sum(1, 2)" превращается в {"sum", "1", "2"} (как в Java-версии).
std::vector<std::string> wordsFromString(const std::string& str);

std::string stringFromWords(const std::vector<std::string>& words);

// Строгий разбор целого числа (вся строка должна быть числом).
bool parseInt(const std::string& s, long long& out);

// Разбирает список слов в числа. ok = false, если хотя бы одно слово не число.
std::vector<long long> numbersFromWords(const std::vector<std::string>& words, bool& ok);

// Настраивает консоль (UTF-8 в Windows). Не запускает внешних процессов,
// чтобы не трогать перенаправленный stdin.
void initConsole();

// Очистка экрана (кроссплатформенно). Ничего не делает, если вывод
// перенаправлен в файл.
void clearScreen();

// Файловый ввод-вывод в UTF-8.
std::string readFile(const std::string& path);
bool writeFile(const std::string& path, const std::string& content);
bool appendLine(const std::string& path, const std::string& line);
bool fileExists(const std::string& path);

// Возвращает количество обычных файлов в каталоге.
int filesCount(const std::string& dir);

// Читает файл вида "ключ остаток строки" в map (аналог Helper.readValues).
std::map<std::string, std::string> readValues(const std::string& path);

int countWord(const std::vector<std::string>& words, const std::string& word);

std::string trim(const std::string& s);
std::vector<std::string> splitLines(const std::string& text);

// Ищет каталог "data" рядом с рабочим каталогом (для запуска из build/).
std::string resolveDataDir();

}  // namespace helper
