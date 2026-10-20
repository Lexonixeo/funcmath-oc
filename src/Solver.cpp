#include "Solver.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <unordered_set>
#include <vector>

namespace solver {

bool hasUnstable(const std::vector<Fn>& functions) {
    for (const Fn& f : functions)
        if (!f.isStable()) return true;
    return false;
}

namespace {

// 64-битный хеш мультимножества (порядок не важен).
uint64_t signature(std::vector<Num> values) {
    std::sort(values.begin(), values.end());
    uint64_t h = 1469598103934665603ULL;
    for (const Num& value : values) {
        uint64_t u = value.hash();
        h ^= u + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        h *= 1099511628211ULL;
        h ^= h >> 29;
    }
    return h;
}

struct Search {
    const std::vector<Fn>& funcs;
    Num target;
    long long valueBound;  // отсекаем бессмысленно большие промежуточные числа
    long long nodes = 0;
    long long nodeLimit;
    long long timeLimitMs;
    bool aborted = false;
    std::chrono::steady_clock::time_point start;
    std::unordered_set<uint64_t> dead;
    std::unordered_set<uint64_t> active;
    std::vector<Move> path;

    Search(const std::vector<Num>& numbers, const std::vector<Fn>& f, Num t,
           long long nodeLim, long long timeLim)
        : funcs(f), target(t), nodeLimit(nodeLim), timeLimitMs(timeLim),
          start(std::chrono::steady_clock::now()) {
        long long maxAbs = std::llabs(t.asLongLong());
        for (const Num& value : numbers) {
            long long current = std::llabs(value.asLongLong());
            if (current > maxAbs) maxAbs = current;
        }
        long double bound = static_cast<long double>(maxAbs) * 16.0L + 1000.0L;
        if (bound > 1000000000.0L) bound = 1000000000.0L;
        valueBound = static_cast<long long>(bound);
        dead.reserve(1 << 16);
        active.reserve(1 << 12);
    }

    bool outOfTime() {
        if ((nodes & 4095) != 0) return false;
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - start)
                           .count();
        return elapsed >= timeLimitMs;
    }

    bool contains(const std::vector<Num>& values, const Num& value) {
        return std::find(values.begin(), values.end(), value) != values.end();
    }

    bool tooBig(const Num& value) const {
        long long number = value.asLongLong();
        return number > valueBound || number < -valueBound;
    }

    // Применяет функцию к выбранным индексам и добавляет ход в path, если
    // решение найдено.
    bool tryResults(const std::vector<Num>& numbers, const std::vector<int>& indices,
                    const std::vector<Num>& args, const std::vector<Num>& results,
                    const std::string& fn) {
        for (const Num& result : results)
            if (tooBig(result)) return false;

        std::vector<char> removed(numbers.size(), 0);
        for (int index : indices) removed[static_cast<std::size_t>(index)] = 1;

        std::vector<Num> next;
        next.reserve(numbers.size() - indices.size() + results.size());
        for (std::size_t i = 0; i < numbers.size(); ++i)
            if (!removed[i]) next.push_back(numbers[i]);
        for (const Num& result : results) next.push_back(result);

        if (contains(next, target) || dfs(next)) {
            path.push_back(Move{fn, args, results});
            return true;
        }
        return false;
    }

    bool tryTuple(const std::vector<Num>& numbers, const std::vector<int>& indices,
                  const Fn& function) {
        std::vector<Num> args;
        args.reserve(indices.size());
        for (int index : indices) args.push_back(numbers[static_cast<std::size_t>(index)]);

        if (function.isStable()) {
            std::vector<Num> results;
            try {
                results = function.use(args);
            } catch (...) {
                return false;
            }
            return tryResults(numbers, indices, args, results, function.name);
        }

        // Случайная функция: перебираем все возможные наборы результатов.
        std::vector<std::vector<Num>> all;
        try {
            all = function.outcomes(args);
        } catch (...) {
            return false;
        }
        for (const std::vector<Num>& results : all) {
            if (tryResults(numbers, indices, args, results, function.name)) return true;
            if (aborted) return false;
        }
        return false;
    }

    bool tryFunction(const std::vector<Num>& numbers, const Fn& function) {
        const int arity = function.numberOfArgs;
        const int n = static_cast<int>(numbers.size());
        if (arity <= 0 || n < arity) return false;

        if (arity == 1) {
            std::vector<int> indices(1);
            for (int i = 0; i < n; ++i) {
                indices[0] = i;
                if (tryTuple(numbers, indices, function)) return true;
                if (aborted) return false;
            }
            return false;
        }

        if (arity == 2) {
            std::vector<int> indices(2);
            for (int i = 0; i < n; ++i) {
                for (int j = 0; j < n; ++j) {
                    if (i == j) continue;
                    indices[0] = i;
                    indices[1] = j;
                    if (tryTuple(numbers, indices, function)) return true;
                    if (aborted) return false;
                }
            }
            return false;
        }

        // Общий случай: упорядоченный выбор arity различных индексов.
        std::vector<int> indices;
        indices.reserve(static_cast<std::size_t>(arity));
        std::function<bool()> choose = [&]() -> bool {
            if (static_cast<int>(indices.size()) == arity)
                return tryTuple(numbers, indices, function);
            for (int i = 0; i < n; ++i) {
                if (std::find(indices.begin(), indices.end(), i) != indices.end()) continue;
                indices.push_back(i);
                if (choose()) return true;
                indices.pop_back();
                if (aborted) return false;
            }
            return false;
        };
        return choose();
    }

    bool dfs(const std::vector<Num>& numbers) {
        if (aborted) return false;
        if (contains(numbers, target)) return true;
        if (++nodes > nodeLimit || outOfTime()) {
            aborted = true;
            return false;
        }

        uint64_t sig = signature(numbers);
        if (dead.count(sig) || active.count(sig)) return false;
        active.insert(sig);

        for (const Fn& function : funcs) {
            if (tryFunction(numbers, function)) return true;
            if (aborted) {
                active.erase(sig);
                return false;
            }
        }

        active.erase(sig);
        dead.insert(sig);
        return false;
    }
};

}  // namespace

bool solve(const std::vector<Num>& numbers, const std::vector<Fn>& functions,
           Num target, std::vector<Move>& moves, long long nodeLimit,
           long long timeLimitMs) {
    Search search(numbers, functions, target, nodeLimit, timeLimitMs);
    bool ok = search.dfs(numbers);
    if (ok) {
        std::reverse(search.path.begin(), search.path.end());
        moves = search.path;
    } else {
        moves.clear();
    }
    return ok;
}

}  // namespace solver
