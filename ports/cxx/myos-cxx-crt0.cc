#include "myos-cxx-sys.h"

extern "C" void (*__init_array_start[])();
extern "C" void (*__init_array_end[])();

extern "C" int main(int argc, char **argv);

extern "C" void _start(int argc, char **argv)
{
    for (void (**ctor)() = __init_array_start; ctor != __init_array_end; ctor++)
        (*ctor)();

    myos_exit(main(argc, argv));
}
