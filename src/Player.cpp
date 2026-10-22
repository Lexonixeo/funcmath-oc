// SPDX-License-Identifier: MIT
// Player.cpp
// ----------
// Логин, регистрация и сохранение прогресса.
//
// Формат data/players/players.txt: "логин хеш_пароля".
// Формат data/players/<hash>.sav:
//   last <номер>
//   completed <номер> <номер> ...
//
// Имя файла сохранения — FNV-1a-хеш логина. Это не защита от подбора, а
// способ получить безопасное имя файла без спецсимволов.
#include "Player.h"

#include <cstdint>
#include <iostream>
#include <map>

#include "Helper.h"

namespace playerstore {

namespace {

std::string hashName(const std::string& name) {
    // FNV-1a, 64 бита, в hex — используется как имя файла сохранения.
    uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : name) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    static const char* digits = "0123456789abcdef";
    std::string s;
    for (int i = 15; i >= 0; --i) s.push_back(digits[(h >> (i * 4)) & 0xF]);
    return s;
}

std::string hashPassword(const std::string& password) {
    return hashName("pwd:" + password);
}

std::map<std::string, std::string> readRegistry(const std::string& path) {
    // Реестр намеренно простой: по одному "логин хеш" на строку. Пустые
    // строки и строки без второго слова игнорируются.
    std::map<std::string, std::string> registry;
    for (const std::string& raw : helper::splitLines(helper::readFile(path))) {
        std::string line = helper::trim(raw);
        if (line.empty()) continue;
        std::vector<std::string> w = helper::wordsFromString(line);
        if (w.size() >= 2) registry[w[0]] = w[1];
    }
    return registry;
}

void writeRegistry(const std::string& path, const std::map<std::string, std::string>& registry) {
    std::string content;
    for (const auto& kv : registry) content += kv.first + " " + kv.second + "\n";
    helper::writeFile(path, content);
}

std::string savePath(const std::string& dataDir, const std::string& name) {
    return dataDir + "/players/" + hashName(name) + ".sav";
}

// Читает строку с приглашением. При EOF завершает программу,
// иначе циклы ввода пароля зациклились бы.
bool promptLine(const std::string& prompt, std::string& out) {
    std::cout << prompt << std::flush;
    if (!std::getline(std::cin, out)) {
        std::cout << std::endl;
        return false;
    }
    // Терминал/пайплайн может прислать CRLF; убираем '\r', иначе логин и пароль
    // не совпадут с сохранёнными значениями.
    if (!out.empty() && out.back() == '\r') out.pop_back();
    return true;
}

}  // namespace

Player login(const std::string& dataDir) {
    // Регистрация происходит при первом вводе неизвестного логина. Пароль
    // подтверждается дважды, чтобы опечатка не создала недоступный аккаунт.
    const std::string registryPath = dataDir + "/players/players.txt";
    std::map<std::string, std::string> registry = readRegistry(registryPath);

    std::string name;
    if (!promptLine("Введите ваш логин: ", name)) std::exit(0);

    auto it = registry.find(name);
    if (it == registry.end()) {
        std::string p1, p2;
        while (true) {
            if (!promptLine("Придумайте пароль: ", p1)) std::exit(0);
            if (!promptLine("Повторите пароль: ", p2)) std::exit(0);
            if (p1 != p2) {
                std::cout << "Пароли отличаются!\n" << std::endl;
            } else {
                break;
            }
        }
        registry[name] = hashPassword(p1);
        writeRegistry(registryPath, registry);
    } else {
        while (true) {
            std::string password;
            if (!promptLine("Введите пароль: ", password)) std::exit(0);
            if (hashPassword(password) != it->second) {
                std::cout << "Неверный пароль! Если вы перепутали логин, перезапустите игру.\n" << std::endl;
            } else {
                break;
            }
        }
    }

    Player player;
    player.name = name;

    std::string spath = savePath(dataDir, name);
    if (helper::fileExists(spath)) {
        for (const std::string& raw : helper::splitLines(helper::readFile(spath))) {
            std::string line = helper::trim(raw);
            if (line.empty()) continue;
            std::vector<std::string> w = helper::wordsFromString(line);
            if (w.empty()) continue;
            if (w[0] == "last" && w.size() >= 2) {
                long long v;
                if (helper::parseInt(w[1], v)) player.lastLevel = static_cast<int>(v);
            } else if (w[0] == "completed") {
                for (std::size_t i = 1; i < w.size(); ++i) {
                    long long v;
                    if (helper::parseInt(w[i], v)) player.completed.insert(static_cast<int>(v));
                }
            }
        }
    } else {
        save(dataDir, player);
    }
    return player;
}

void save(const std::string& dataDir, const Player& player) {
    // Сохраняем весь прогресс целиком: файл маленький, а инкрементальное
    // обновление усложнило бы восстановление после сбоя.
    std::string content;
    content += "last " + std::to_string(player.lastLevel) + "\n";
    content += "completed";
    for (int lvl : player.completed) content += " " + std::to_string(lvl);
    content += "\n";
    helper::writeFile(savePath(dataDir, player.name), content);
}

}  // namespace playerstore
