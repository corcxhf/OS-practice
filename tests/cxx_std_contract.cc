#include <algorithm>
#include <array>
#include <type_traits>
#include <utility>

#include "myos-cxx-sys.h"

static void fail(const char *name)
{
    myos_write_str("CXX_STD_CONTRACT_FAIL ");
    myos_write_str(name);
    myos_write_str("\n");
    myos_exit(1);
}

static void check(bool condition, const char *name)
{
    if (!condition)
        fail(name);
}

struct Record
{
    int key;
    int value;
};

constexpr bool compile_time_checks()
{
    std::array<int, 4> values{{4, 1, 3, 2}};
    std::sort(values.begin(), values.end());
    return values.front() == 1 && values.back() == 4 && values.size() == 4;
}

static int exercise_std_subset()
{
    static_assert(std::is_same_v<std::array<int, 3>::value_type, int>);
    static_assert(std::is_same_v<std::remove_reference_t<int &>, int>);
    static_assert(compile_time_checks());

    std::array<int, 6> values{{9, 1, 5, 3, 7, 2}};
    std::sort(values.begin(), values.end());
    check(values[0] == 1, "array-sort-first");
    check(values[5] == 9, "array-sort-last");

    int total = 0;
    for (auto item : values)
        total += item;
    check(total == 27, "range-for-array");

    auto pair = std::make_pair(values.front(), values.back());
    check(pair.first == 1 && pair.second == 9, "make-pair");

    std::array<Record, 4> records{{{3, 30}, {1, 10}, {4, 40}, {2, 20}}};
    std::sort(records.begin(), records.end(), [](const Record &left, const Record &right) {
        return left.key < right.key;
    });
    check(records[0].value == 10, "custom-sort-first");
    check(records[3].value == 40, "custom-sort-last");

    check(std::min(7, 4) == 4, "min");
    check(std::max(7, 4) == 7, "max");

    return 0;
}

int main(int, char **)
{
    exercise_std_subset();
    myos_write_str("CXX_STD_CONTRACT_PASS\n");
    return 0;
}
