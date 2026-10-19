#pragma once
// Данные уровня и чтение/запись файлов уровней.
// Формат текстовый (переносимый, в отличие от Java-сериализации):
//
//   # комментарий
//   numbers 2 5 7
//   answer 9
//   function sum
//   function A unknown mul      # неизвестная функция A, ведущая себя как mul

#include <string>
#include <vector>

#include "Function.h"

struct LevelData {
    int number = -1;            // номер уровня (-1 для случайного)
    std::string title;          // заголовок, напр. "Уровень 5."
    std::vector<Num> numbers;
    std::vector<Fn> functions;
    Num answer = 0;
};

namespace levelstore {

bool load(const std::string& path, LevelData& out, std::string& error);
bool save(const std::string& path, const LevelData& data);

// Путь к файлу уровня по его номеру относительно каталога data.
std::string levelPath(const std::string& dataDir, int number);

// Отсортированный список существующих уровней.
std::vector<int> listLevelNumbers(const std::string& dataDir);

// Следующий свободный номер уровня.
int nextLevelNumber(const std::string& dataDir);

}  // namespace levelstore
