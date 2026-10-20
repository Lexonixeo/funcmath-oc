#pragma once
// Именованная функция уровня — порт класса Function.
// Функция может быть "известной" (описание из data/functions.txt) или
// "неизвестной" (A, B, C...): игрок видит только то, что это какая-то
// функция, а её поведение = одна из простейших функций.
//
// Арность и поведение берутся из fmath::registry(). Функция может возвращать
// несколько значений (например, primes).

#include <string>
#include <vector>

#include "Functions.h"

struct Fn {
    std::string name;         // имя, которое вводит игрок (sum, A, B, ...)
    std::string description;  // строка для показа игроку
    std::string behavior;     // ключ в fmath::registry(), что реально считается
    int numberOfArgs = 2;

    std::vector<Num> use(const std::vector<Num>& args) const;
    std::vector<std::vector<Num>> outcomes(const std::vector<Num>& args) const;
    bool isStable() const { return fmath::isStable(behavior); }
};

// Известная функция: поведение и описание берутся по имени.
Fn makeKnownFunction(const std::string& name);

// Неизвестная функция: своё имя, поведение — заданная функция.
Fn makeUnknownFunction(const std::string& name, const std::string& behavior);

// Описание вида "A(a, b) - неизвестная функция".
std::string unknownDescription(const std::string& name, int numberOfArgs);
