// SPDX-License-Identifier: MIT
#pragma once
// Данные уровня и чтение/запись файлов уровней.
// Формат текстовый (переносимый, в отличие от Java-сериализации):
//
//   # комментарий
//   number natural 3
//   number natural 4
//   number integer -5
//   answer natural 12
//   function sum
//   function A unknown mul      # неизвестная функция A, ведущая себя как mul
//
// Старый формат без типов тоже поддерживается: все числа и ответ считаются
// integer.
//   numbers 2 5 7
//   answer 9
//
// Тип хранится у каждого числа (Value), а не у уровня.

#include <string>
#include <vector>

#include "Function.h"

struct LevelData {
    int number = -1;            // номер уровня (-1 для случайного)
    std::string title;          // заголовок, напр. "Уровень 5."
    std::vector<Num> numbers;
    std::vector<std::string> names;  // необязательные имена чисел (пустая строка = нет)
    std::vector<Fn> functions;
    Num answer = Num(0);
};

namespace levelstore {

// load не бросает исключений: при ошибке возвращает false и текст в error.
// Частично заполненный out после ошибки не считается валидным.
bool load(const std::string& path, LevelData& out, std::string& error);
// save создаёт родительские каталоги; формат старается сохранить старый
// компактный вид, если все числа integer и имён нет.
bool save(const std::string& path, const LevelData& data);

// Путь к файлу уровня по его номеру относительно каталога data.
std::string levelPath(const std::string& dataDir, int number);

// Отсортированный список существующих уровней.
std::vector<int> listLevelNumbers(const std::string& dataDir);

// Следующий свободный номер уровня.
int nextLevelNumber(const std::string& dataDir);

}  // namespace levelstore
