#include <algorithm>
#include <new>
#include <string>
#include <vector>

#include "myos-cxx-sys.h"

static int constructed_count;
static int destructed_count;

static void fail(const char *name)
{
    myos_write_str("CXX_DYN_CONTRACT_FAIL ");
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

    Record() : key(0), value(0)
    {
        constructed_count++;
    }

    Record(int k, int v) : key(k), value(v)
    {
        constructed_count++;
    }

    Record(const Record &other) : key(other.key), value(other.value)
    {
        constructed_count++;
    }

    Record(Record &&other) noexcept : key(other.key), value(other.value)
    {
        constructed_count++;
        other.key = 0;
        other.value = 0;
    }

    ~Record()
    {
        destructed_count++;
    }

    Record &operator=(const Record &other)
    {
        key = other.key;
        value = other.value;
        return *this;
    }

    Record &operator=(Record &&other) noexcept
    {
        key = other.key;
        value = other.value;
        other.key = 0;
        other.value = 0;
        return *this;
    }
};

static void test_vector_int()
{
    std::vector<int> values;
    check(values.empty(), "vector-empty");
    for (int i = 9; i >= 1; i--)
        values.push_back(i);

    check(values.size() == 9, "vector-size");
    check(values.capacity() >= values.size(), "vector-capacity");
    std::sort(values.begin(), values.end());
    check(values.front() == 1, "vector-sort-first");
    check(values.back() == 9, "vector-sort-last");

    int total = 0;
    for (auto item : values)
        total += item;
    check(total == 45, "vector-range-total");
}

static void test_vector_record()
{
    int before_constructed = constructed_count;
    int before_destructed = destructed_count;

    {
        std::vector<Record> records;
        records.emplace_back(3, 30);
        records.emplace_back(1, 10);
        records.emplace_back(4, 40);
        records.emplace_back(2, 20);

        check(records.size() == 4, "record-vector-size");
        std::sort(records.begin(), records.end(), [](const Record &left, const Record &right) {
            return left.key < right.key;
        });
        check(records[0].value == 10, "record-vector-first");
        check(records[3].value == 40, "record-vector-last");
    }

    check(constructed_count > before_constructed, "record-constructed");
    check(destructed_count > before_destructed, "record-destructed");
    check(destructed_count == constructed_count, "record-balanced");
}

static void test_string()
{
    std::string hello("hello");
    std::string space(" ");
    std::string world("world");
    std::string message = hello + space + world;

    check(message.size() == 11, "string-size");
    check(message[0] == 'h' && message[10] == 'd', "string-index");
    check(message == std::string("hello world"), "string-eq");
    check(std::string("abc") < std::string("abd"), "string-less");

    message += "!";
    message.push_back('?');
    check(message == std::string("hello world!?"), "string-append");

    std::vector<std::string> words;
    words.push_back(std::string("delta"));
    words.push_back(std::string("alpha"));
    words.push_back(std::string("charlie"));
    std::sort(words.begin(), words.end());
    check(words[0] == std::string("alpha"), "string-vector-sort-first");
    check(words[2] == std::string("delta"), "string-vector-sort-last");
}

static void test_reusable_heap()
{
    auto exercise_heap = []() {
        std::vector<std::string> words;

        for (int i = 0; i < 32; i++)
        {
            std::string item("item-");
            item += char('a' + (i % 26));
            words.push_back(item);
        }
        check(words.size() == 32, "heap-round-size");
        check(words[0] == std::string("item-a"), "heap-round-first");
    };

    exercise_heap();
    myos_uint64 before = myos_syscall3(MYOS_SYS_sbrk, 0, 0, 0);

    for (int round = 0; round < 64; round++)
        exercise_heap();

    myos_uint64 after = myos_syscall3(MYOS_SYS_sbrk, 0, 0, 0);
    check(after == before, "heap-reuse-break");
}

static void test_nothrow_new()
{
    int *value = new (std::nothrow) int(42);
    check(value != nullptr && *value == 42, "nothrow-new");
    delete value;

    int *items = new (std::nothrow) int[3];
    check(items != nullptr, "nothrow-new-array");
    items[0] = 7;
    items[1] = 11;
    items[2] = 13;
    check(items[0] + items[1] + items[2] == 31, "nothrow-new-array-values");
    delete[] items;
}

int main(int, char **)
{
    test_vector_int();
    test_vector_record();
    test_string();
    test_reusable_heap();
    test_nothrow_new();
    myos_write_str("CXX_DYN_CONTRACT_PASS\n");
    return 0;
}
