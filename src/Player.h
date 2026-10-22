// SPDX-License-Identifier: MIT
#pragma once
// Аккаунт игрока и сохранения — порт класса Game (часть логики входа/сохранения).
// Пароли хранятся как FNV-1a-хеш: это учебный проект, не криптографическая
// защита. Файлы сохранений привязаны к хешу имени, а не к самому имени.

#include <set>
#include <string>

struct Player {
    std::string name;
    std::set<int> completed;
    int lastLevel = -1;
    bool tutorial = true;  // подсказка показывается один раз за запуск
};

namespace playerstore {

// Логин или регистрация. Файлы: data/players/players.txt и data/players/<hash>.sav
Player login(const std::string& dataDir);

// Сохраняет прогресс игрока.
void save(const std::string& dataDir, const Player& player);

}  // namespace playerstore
