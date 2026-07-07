#include <algorithm>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

static int global_ctor_value;

struct GlobalProbe
{
    GlobalProbe()
    {
        global_ctor_value = 17;
    }
};

static GlobalProbe global_probe;

static void fail(const char *name)
{
    printf("GXX_CONTRACT_FAIL %s\n", name);
}

static int check(bool condition, const char *name)
{
    if (condition)
        return 0;
    fail(name);
    return 1;
}

int main()
{
    int failures = 0;
    std::vector<std::string> words;

    words.push_back(std::string("cpp"));
    words.push_back(std::string("gxx"));
    words.push_back(std::string("myos"));
    std::sort(words.begin(), words.end());

    std::string joined = words[0] + "-" + words[1] + "-" + words[2];
    char buf[64];
    snprintf(buf, sizeof(buf), "%s:%d", joined.c_str(), (int)words.size());

    failures += check(global_ctor_value == 17, "global-ctor");
    failures += check(joined == std::string("cpp-gxx-myos"), "std-string-vector-sort");
    failures += check(strcmp(buf, "cpp-gxx-myos:3") == 0, "libc-snprintf-strcmp");

    if (failures)
        return 1;

    printf("GXX_CONTRACT_PASS %s\n", buf);
    return 0;
}
