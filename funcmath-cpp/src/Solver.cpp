#include "Solver.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <unordered_set>

namespace solver {

bool hasUnstable(const std::vector<Fn>& functions) {
    for (const Fn& f : functions)
        if (!f.isStable()) return true;
    return false;
}

namespace {

// 64-битный хеш мультимножества (порядок не важен).
uint64_t signature(std::vector<Num> v) {
    std::sort(v.begin(), v.end());
    uint64_t h = 1469598103934665603ULL;
    for (Num x : v) {
        uint64_t u = static_cast<uint64_t>(x);
        h ^= u + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        h *= 1099511628211ULL;
        h ^= h >> 29;
    }
    return h;
}

struct Search {
    const std::vector<Fn>& funcs;
    Num target;
    Num valueBound;  // отсекаем бессмысленно большие промежуточные числа
    long long nodes = 0;
    long long nodeLimit;
    long long timeLimitMs;
    bool aborted = false;
    std::chrono::steady_clock::time_point start;
    std::unordered_set<uint64_t> dead;
    std::vector<Move> path;

    Search(const std::vector<Num>& numbers, const std::vector<Fn>& f, Num t,
           long long nodeLim, long long timeLim)
        : funcs(f), target(t), nodeLimit(nodeLim), timeLimitMs(timeLim),
          start(std::chrono::steady_clock::now()) {
        Num maxAbs = t < 0 ? -t : t;
        for (Num x : numbers) {
            Num ax = x < 0 ? -x : x;
            if (ax > maxAbs) maxAbs = ax;
        }
        long double b = static_cast<long double>(maxAbs) * 16.0L + 1000.0L;
        if (b > 1000000000.0L) b = 1000000000.0L;
        valueBound = static_cast<Num>(b);
        dead.reserve(1 << 16);
    }

    bool outOfTime() {
        if ((nodes & 4095) != 0) return false;
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - start)
                           .count();
        return elapsed >= timeLimitMs;
    }

    bool contains(const std::vector<Num>& v, Num x) {
        return std::find(v.begin(), v.end(), x) != v.end();
    }

    bool tooBig(Num c) const { return c > valueBound || c < -valueBound; }

    bool dfs(const std::vector<Num>& nums) {
        if (aborted) return false;
        if (contains(nums, target)) return true;
        if (nums.size() < 2) return false;
        if (++nodes > nodeLimit || outOfTime()) {
            aborted = true;
            return false;
        }

        uint64_t sig = signature(nums);
        if (dead.count(sig)) return false;

        const int n = static_cast<int>(nums.size());
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                for (const Fn& f : funcs) {
                    if (!f.isStable()) continue;
                    Num pairs[2][2] = {{nums[i], nums[j]}, {nums[j], nums[i]}};
                    for (int order = 0; order < 2; ++order) {
                        Num c;
                        try {
                            c = f.use({pairs[order][0], pairs[order][1]});
                        } catch (...) {
                            continue;
                        }
                        if (tooBig(c)) continue;
                        std::vector<Num> next;
                        next.reserve(nums.size() - 1);
                        for (int k = 0; k < n; ++k)
                            if (k != i && k != j) next.push_back(nums[k]);
                        next.push_back(c);
                        if (contains(next, target)) {
                            path.push_back({f.name, pairs[order][0], pairs[order][1], c});
                            return true;
                        }
                        if (dfs(next)) {
                            path.push_back({f.name, pairs[order][0], pairs[order][1], c});
                            return true;
                        }
                        if (aborted) return false;
                    }
                }
            }
        }
        dead.insert(sig);
        return false;
    }
};

}  // namespace

bool solve(const std::vector<Num>& numbers, const std::vector<Fn>& functions,
           Num target, std::vector<Move>& moves, long long nodeLimit, long long timeLimitMs) {
    Search s(numbers, functions, target, nodeLimit, timeLimitMs);
    bool ok = s.dfs(numbers);
    if (ok) {
        std::reverse(s.path.begin(), s.path.end());
        moves = s.path;
    } else {
        moves.clear();
    }
    return ok;
}

}  // namespace solver
