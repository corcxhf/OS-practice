#include "myos-cxx-sys.h"

static void fail(const char *name)
{
    myos_write_str("CXX_CONTRACT_FAIL ");
    myos_write_str(name);
    myos_write_str("\n");
    myos_exit(1);
}

static void check(bool condition, const char *name)
{
    if (!condition)
        fail(name);
}

static int global_value;

class GlobalCounter
{
public:
    explicit GlobalCounter(int seed) : seed_(seed)
    {
        global_value += seed_;
    }

private:
    int seed_;
};

static GlobalCounter global_counter(17);

template <typename T>
static T add(T left, T right)
{
    return left + right;
}

template <int N>
struct Fib
{
    static constexpr int value = Fib<N - 1>::value + Fib<N - 2>::value;
};

template <>
struct Fib<1>
{
    static constexpr int value = 1;
};

template <>
struct Fib<0>
{
    static constexpr int value = 0;
};

class Tool
{
public:
    virtual ~Tool() {}
    virtual int run(int value) const = 0;
};

class Multiplier final : public Tool
{
public:
    explicit Multiplier(int factor) : factor_(factor) {}

    int run(int value) const override
    {
        return value * factor_;
    }

private:
    int factor_;
};

static __attribute__((noinline)) int run_tool(const Tool &tool, int value)
{
    return tool.run(value);
}

static int bump(int &value)
{
    value += 3;
    return value;
}

static int overload(int value)
{
    return value + 4;
}

static long overload(long value)
{
    return value + 9;
}

static int exercise_cpp()
{
    constexpr int fib6 = Fib<6>::value;
    check(fib6 == 8, "constexpr-template");

    check(global_value == 17, "global-constructor");
    check(add<int>(20, 22) == 42, "function-template-int");
    check(add<long>(30, 12) == 42, "function-template-long");

    int value = 5;
    check(bump(value) == 8 && value == 8, "reference");
    check(overload(38) == 42, "overload-int");
    check(overload(33L) == 42L, "overload-long");

    Tool *tool = new Multiplier(6);
    int result = run_tool(*tool, 7);
    delete tool;
    check(result == 42, "virtual-new-delete");

    int *items = new int[4];
    for (int i = 0; i < 4; i++)
        items[i] = i + 1;
    check(items[0] + items[1] + items[2] + items[3] == 10, "new-array");
    delete[] items;

    return 0;
}

int main(int, char **)
{
    exercise_cpp();
    myos_write_str("CXX_CONTRACT_PASS\n");
    return 0;
}
