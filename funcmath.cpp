// func(math) -- пре-альфа прототип
// Игра: из набора чисел с помощью набора функций нужно получить заданное число.
//
// Эта версия -- доработанный объединённый вариант (раньше были отдельные
// funcmath.cpp для Windows и funcmath_linux.cpp для Linux).
//
// Что изменено по сравнению с прототипом:
//   * один файл для Windows и Linux (авто-выбор команды очистки экрана);
//   * исправлена функция rand: было `rand() << 32 + rand()` -- из-за
//     приоритета операторов это `rand() << (32 + rand())`, т.е. сдвиг на
//     огромное число (UB) плюс отрицательный результат;
//   * исправлен литерал -9223372036854775808 (сужался до unsigned);
//   * устранён выход за границы массива при `level 0` и подобных;
//   * ввод читается построчно: неверный ввод больше не зацикливает игру;
//   * убрано дублирование кода меню уровня (level / random_level);
//   * корректный выбор двух чисел из набора, включая случай одинаковых чисел;
//   * добавлена команда history (история ходов) и вывод остатка набора;
//   * отлажена генерация случайного уровня (без риска зависнуть/упасть).

#include <iostream>
#include <functional>
#include <vector>
#include <ctime>
#include <string>
#include <algorithm>
#include <sstream>
#include <numeric>
#include <climits>
#include <cstdlib>

#define ll long long

using namespace std;

// --- Очистка экрана: Windows и POSIX -------------------------------------
static void clear_screen()
{
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

// --- Математические помощники -------------------------------------------
// Математический (неотрицательный) остаток.
static ll math_mod(ll a, ll b)
{
    if (b == 0) return 0;
    return ((a % b) + b) % b;
}

// Целочисленное деление с округлением вниз (как в оригинальном прототипе).
static ll floor_div(ll a, ll b)
{
    if (b == 0)
        return (a == 0) ? 0 : (a > 0 ? LLONG_MAX : LLONG_MIN);
    return (a - math_mod(a, b)) / b;
}

// Случайное целое в [min(a,b), max(a,b)].
static ll random_between(ll a, ll b)
{
    if (a > b) swap(a, b);
    ll span = b - a + 1;                     // >= 1
    unsigned long long r = ((unsigned long long)rand() << 15) ^ (unsigned long long)rand();
    return a + (ll)(r % (unsigned long long)span);
}

// --- Набор доступных функций --------------------------------------------
vector<std::function<ll(ll, ll)>> funcs = {
    [](ll x, ll y) { return x + y; },
    [](ll x, ll y) { return x - y; },
    [](ll x, ll y) { return x * y; },
    [](ll x, ll y) { return floor_div(x, y); },              // целочисленное деление
    [](ll x, ll y) { return math_mod(x, y); },               // математический остаток
    [](ll, ll)    { return 5; },                             // A - неизвестная функция
    [](ll x, ll)  { return -x + 2; },                        // B - неизвестная функция
    [](ll x, ll)  { return (ll)abs(x); },                    // abs
    [](ll x, ll y) { return random_between(x, y); }          // rand - нестабильна
};
vector<string> fnames = {
    "sum", "sub", "mul", "div", "mod", "A", "B", "abs", "rand"
};
vector<string> fdescs = {
    "(a, b) = a + b",
    "(a, b) = a - b",
    "(a, b) = a * b",
    "(a, b) = a / b (целочисленное)",
    "(a, b) = a % b (остаток от a при делении на b)",
    "(a, b) - неизвестная функция",
    "(a, b) - неизвестная функция",
    "(a, b) = abs(a) (абсолютное значение)",
    "(a, b) = случайное целое число в интервале [a, b] (нестабильно)"
};

// --- Данные уровней ------------------------------------------------------
vector<vector<ll>> nums = {
    {1, 1}, {1, 1, 1}, {2, 5, 7}, {2, 5}, {2, 3, 9}, {2, 0, 2, 4}, {1, 2, 3, 4, 5, 6, 7, 8, 9}, {1, 2, 3, 4, 5, 6, 7, 8, 9}, {2, 2, 2, 2, 2, 1, 2, 2, 2}, {1, 2, 8, 16, 32, 63},
    {2, 2}, {2, 3, 5, 7, 11}, {-1, 25, -4, 35, 9}, {6, 3}, {63, 7, 9}, {6, 4, 7, 100}, {2, 3, 5}, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 35, 99, 105, 117}, {1, 2, 3, 4, 5, 6, 7, 8, 9}, {9, 2, 5, 7, 2, 8},
    {1, 1, 1, 1, 2, 2, 2, 7, 7, 7, 7, 9, 9}, {19, 19, 20, 20, 2, 2}, {-2, 145, 18, 987653}, {5, 5, 5, 5, 5, 5, 5}, {1, 2, 3, 4}, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, {2, 7, 17}, {93, 24, 3}, {-4, 3, 5, 100, -101}, {6, 7, 5, 4},
    {2, 7, 4, 274}, {-5, 25, -125, 625}, {426, 312, 462, 984}
};
vector<vector<ll>> cmds = {
    {0}, {0}, {0}, {1}, {1}, {1}, {0, 1}, {0, 1}, {0, 1}, {0, 1},
    {2}, {2}, {0, 2}, {3}, {0, 1, 3}, {0, 3}, {2, 3}, {2, 3}, {0, 1, 2, 3}, {0, 1, 2, 3},
    {0, 2}, {0, 1, 2, 3}, {2, 3, 7}, {1, 6, 7}, {0, 1, 2, 3}, {0, 1, 2, 3}, {0, 4}, {3, 4}, {0, 4, 6}, {0, 6},
    {2, 5, 6}, {5, 6, 8}, {2, 5}
};
vector<ll> ans = {
    2, 3, 9, 3, -10, -6, 31, -27, 13, -4,
    4, 231, 26, 2, 10, 8, 1, 2048, -47, 239,
    346, 179, 16, 4, 25, 61, 8, 7, 0, -9,
    15, 6, 25
};

int playlvl = 1, lastlvl = 1;
bool lvlhelp = true;

// Находит позиции двух чисел a и b (позиции обязательно разные).
// Корректно работает, когда a == b и в наборе несколько таких чисел.
static bool locate_two(const vector<ll>& v, ll a, ll b, int& ia, int& ib)
{
    ia = ib = -1;
    for (int i = 0; i < (int)v.size(); ++i) {
        if (ia == -1 && v[i] == a)      ia = i;
        else if (v[i] == b)             ib = i;
    }
    return ia != -1 && ib != -1;
}

static void print_numbers(const vector<ll>& numbers)
{
    cout << "У вас есть ограниченный набор чисел:" << endl;
    for (ll x : numbers) cout << x << " ";
    cout << endl << endl;
}

static void print_functions(const vector<ll>& commands)
{
    cout << "У вас есть функции:" << endl;
    for (ll c : commands) cout << fnames[c] << fdescs[c] << endl;
    cout << endl;
}

// Печатает шапку уровня (используется и при входе, и при reset/cls/help).
static void print_level(const string& title, const vector<ll>& numbers,
                        const vector<ll>& commands, ll answer)
{
    cout << title << endl;
    print_numbers(numbers);
    print_functions(commands);
    cout << "Вам нужно из данных чисел, используя данные функции, получить число " << answer << "." << endl << endl;
}

static void print_controls()
{
    cout << "Используя функцию над двумя числами, вы забираете эти два числа из набора и в набор добавляется результат функции." << endl;
    cout << "Использовать функцию так: {название функции} {число 1} {число 2}" << endl;
    cout << "Пример использования: sum 1 3       / выведет и запишет в набор число 4" << endl << endl;
    cout << "help - инструкция и список команд, cls - очистить экран, reset - перезагрузить уровень" << endl;
    cout << "nums - текущий набор чисел, history - история ходов, exit - выход из уровня" << endl << endl;
}

// Общий игровой цикл для обычного и случайного уровня.
static int play_level(const string& title, vector<ll> numbers,
                      const vector<ll>& commands, ll answer)
{
    const vector<ll> initial = numbers;
    vector<string> history;

    print_level(title, numbers, commands, answer);
    if (lvlhelp) {
        print_controls();
        lvlhelp = false;
    }

    string line;
    while (true) {
        if (find(numbers.begin(), numbers.end(), answer) != numbers.end()) {
            cout << "Поздравляем! Вы получили число " << answer << " и прошли уровень!" << endl;
            return 0;
        }

        cout << "[Level] Введите выражение: ";
        if (!getline(cin, line)) {          // конец ввода (например, при перенаправлении)
            cout << endl;
            return 1;
        }
        istringstream iss(line);
        string s;
        if (!(iss >> s)) continue;          // пустая строка

        if (s == "exit") {
            return 1;
        } else if (s == "nums") {
            cout << "=";
            for (ll x : numbers) cout << " " << x;
            cout << endl;
        } else if (s == "history") {
            if (history.empty()) cout << "(пока нет ходов)" << endl;
            for (const string& h : history) cout << h << endl;
        } else if (s == "reset") {
            numbers = initial;
            history.clear();
            clear_screen();
            print_level(title, numbers, commands, answer);
        } else if (s == "help") {
            clear_screen();
            print_level(title, numbers, commands, answer);
            print_controls();
        } else if (s == "cls") {
            clear_screen();
            print_level(title, numbers, commands, answer);
        } else if (find(fnames.begin(), fnames.end(), s) == fnames.end()) {
            cout << "= Такой функции у вас нет, попробуйте снова. Введите help, если забыли что можно вводить." << endl;
        } else if (find_if(commands.begin(), commands.end(),
                           [&](ll c) { return fnames[c] == s; }) == commands.end()) {
            cout << "= На этом уровне такой функции нет, попробуйте снова." << endl;
        } else {
            int index = (int)distance(fnames.begin(), find(fnames.begin(), fnames.end(), s));
            auto func = funcs[index];

            ll a, b;
            if (!(iss >> a >> b)) {
                cout << "= Неверный формат. Пример: " << s << " 1 3" << endl;
                continue;
            }

            int ia, ib;
            if (!locate_two(numbers, a, b, ia, ib)) {
                cout << "Какое(ие)-то из этих чисел отсутствует в наборе, попробуйте снова." << endl;
                continue;
            }

            ll c = func(a, b);
            if (ia > ib) swap(ia, ib);
            numbers.erase(numbers.begin() + ib);
            numbers.erase(numbers.begin() + ia);
            numbers.push_back(c);

            ostringstream rec;
            rec << s << " " << a << " " << b << " = " << c;
            history.push_back(rec.str());

            cout << "= " << c << "   (в наборе:";
            for (ll x : numbers) cout << " " << x;
            cout << ")" << endl;
        }
    }
}

int help()
{
    clear_screen();
    cout << "Список команд:" << endl;
    cout << "play - Начать/продолжить игру" << endl;
    cout << "random {difficulty} - Запустить уровень сложности {difficulty} (от 1)" << endl;
    cout << "level {lvl} - Перейти к уровню {lvl}" << endl;
    cout << "last - Перейти к предыдущему (прошедшему) уровню" << endl;
    cout << "next - Перейти к следующему уровню" << endl;
    cout << "exit - Выйти из игры" << endl;
    cout << "help - Узнать список команд" << endl;
    return 0;
}

int level(int lvl)
{
    if (lvl < 0 || lvl >= (int)nums.size()) {
        cout << "Такого уровня не существует!" << endl;
        return 1;
    }
    clear_screen();
    return play_level("Уровень " + to_string(lvl + 1) + ".", nums[lvl], cmds[lvl], ans[lvl]);
}

int random_level(ll difficulty)
{
    if (difficulty < 1) {
        cout << "Неверно указана сложность!" << endl;
        return 1;
    }

    const int maxFn = (int)fnames.size();

    // Случайный набор чисел.
    int count = (int)max(2ll, difficulty + rand() % (int)difficulty);
    vector<ll> generated_nums(count);
    for (ll& x : generated_nums)
        x = rand() % (int)(40 * difficulty + 1) - (int)(20 * difficulty);

    // Случайный неповторяющийся набор функций.
    int want = (int)max(min((ll)maxFn, max(1ll, difficulty / 3) + rand() % (int)(1 + (difficulty + 1) / 2)), 1ll);
    want = min(want, maxFn);
    vector<int> pool(maxFn);
    iota(pool.begin(), pool.end(), 0);
    for (int i = maxFn - 1; i > 0; --i) swap(pool[i], pool[rand() % (i + 1)]);
    vector<ll> commands(pool.begin(), pool.begin() + want);
    sort(commands.begin(), commands.end());

    // Генерируем ответ, реально применяя функции к числам (чтобы он был достижим).
    ll generated_ans = 0;
    bool ok = false;
    for (int att = 0; att < 100 && !ok; ++att) {
        vector<ll> gnums = generated_nums;
        int steps = rand() % (count - 1) + 1;         // count >= 2
        for (int i = 0; i < steps; ++i) {
            int sz = (int)gnums.size();
            if (sz < 2) break;
            int x = rand() % (sz - 1);
            int y = x + 1 + rand() % (sz - 1 - x);
            if (rand() % 2 == 1) swap(x, y);
            ll a = gnums[x], b = gnums[y];
            ll c = funcs[commands[rand() % commands.size()]](a, b);

            if (x > y) swap(x, y);
            gnums.erase(gnums.begin() + y);
            gnums.erase(gnums.begin() + x);
            gnums.push_back(c);
        }
        generated_ans = gnums[rand() % gnums.size()];
        ok = find(generated_nums.begin(), generated_nums.end(), generated_ans) == generated_nums.end();
    }
    if (!ok) {
        cout << "Не удалось сгенерировать уровень :(" << endl;
        cout << "Попробуйте ещё раз." << endl;
        return 1;
    }

    clear_screen();
    return play_level("Уровень случайный сложности " + to_string(difficulty) + ".",
                      generated_nums, commands, generated_ans);
}

int commander()
{
    cout << endl;
    string line;
    while (true) {
        cout << "[Commander] Введите команду: ";
        if (!getline(cin, line)) break;
        istringstream iss(line);
        string s;
        if (!(iss >> s)) continue;

        if (s == "exit") {
            break;
        } else if (s == "level") {
            int lvl;
            if (!(iss >> lvl)) { cout << "Укажите номер уровня: level {lvl}" << endl; continue; }
            if (level(lvl - 1) == 0 || (lvl >= 1 && lvl <= (int)nums.size()))
                lastlvl = lvl;
        } else if (s == "help") {
            help();
        } else if (s == "play") {
            lastlvl = playlvl;
            if (level(playlvl - 1) == 0) playlvl++;
        } else if (s == "last") {
            if (lastlvl - 1 >= 1) { lastlvl--; level(lastlvl - 1); }
            else cout << "Это уже первый уровень." << endl;
        } else if (s == "next") {
            if (lastlvl + 1 <= (int)nums.size()) { lastlvl++; level(lastlvl - 1); }
            else cout << "Это уже последний уровень." << endl;
        } else if (s == "random") {
            ll difficulty;
            if (!(iss >> difficulty)) { cout << "Укажите сложность: random {difficulty}" << endl; continue; }
            random_level(difficulty);
        } else {
            cout << "Неизвестная команда! Введите help, чтобы узнать список команд." << endl;
        }
    }
    return 0;
}

int menu()
{
    clear_screen();
    cout << "Добро пожаловать!" << endl;
    cout << "Вы сейчас играете в пре-альфа версию игры func(math)." << endl;
    cout << "Она создана с целью попробовать реализовать геймплей игры, а также узнать, может ли она вам понравиться." << endl;
    cout << endl;
    cout << "Вы находитесь в главном меню." << endl;
    cout << "Игра начинается с 1-ого уровня. Всего в игре " << nums.size() << " уровней." << endl;
    cout << "Чтобы начать игру, введите команду play." << endl;
    cout << "Чтобы играть в случайный уровень, введите команду random и сложность. (число от 1)" << endl;
    cout << "Чтобы узнать список команд, введите команду help." << endl;
    cout << "Чтобы выйти из игры, введите команду exit." << endl;
    commander();
    return 0;
}

int main()
{
#ifdef _WIN32
    system("chcp 65001 > nul");     // UTF-8 в консоли Windows
#endif
    srand((unsigned)time(0));
    menu();
    return 0;
}
