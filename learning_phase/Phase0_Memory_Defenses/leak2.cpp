
#include<iostream>
using namespace std;

int main(){

    int* data = new int[50];// we allocate memeory

    delete[] data; // we delllocated it securely

    // telling os to delete it again <-- Critical flaw

    delete[] data;

    return 0;

}

/*
14359== Memcheck, a memory error detector
==14359== Copyright (C) 2002-2024, and GNU GPL'd, by Julian Seward et al.
==14359== Using Valgrind-3.26.0 and LibVEX; rerun with -h for copyright info
==14359== Command: ./leak2_test
==14359== 
==14359== Invalid free() / delete / delete[] / realloc()
==14359==    at 0x4856494: operator delete[](void*) (vg_replace_malloc.c:1413)
==14359==    by 0x40011A8: main (in /mnt/d/memory_test/leak2_test)
==14359==  Address 0x4e98080 is 0 bytes inside a block of size 200 free'd
==14359==    at 0x4856494: operator delete[](void*) (vg_replace_malloc.c:1413)
==14359==    by 0x4001195: main (in /mnt/d/memory_test/leak2_test)
==14359==  Block was alloc'd at
==14359==    at 0x48525F3: operator new[](unsigned long) (vg_replace_malloc.c:730)
==14359==    by 0x400117E: main (in /mnt/d/memory_test/leak2_test)
==14359== 
==14359== 
==14359== HEAP SUMMARY:
==14359==     in use at exit: 0 bytes in 0 blocks
==14359==   total heap usage: 2 allocs, 3 frees, 73,928 bytes allocated
==14359== 
==14359== All heap blocks were freed -- no leaks are possible
==14359== 
==14359== For lists of detected and suppressed errors, rerun with: -s
==14359== ERROR SUMMARY: 1 errors from 1 contexts (suppressed: 0 from 0)
sathi@SATHI:/mnt/d/memory_test$ 

In your code, you only wrote one new (1 allocation) and two deletes (2 frees). 
So where the hell did the extra 1 allocation and 1 free come from?The 
"Ghost" Allocation (The C++ Engine)The culprit is the very first line of your file: #include <iostream>.
Before your main() function even begins executing, the C++ engine has to wake up and prepare the system s
s you can print things to the terminal. To do this, it quietly borrows a tiny block of memory in the background to set up 
std::cout and std::cin.When your program hits return 0 and dies, the C++ engine cleans up its own mess and gives 
that background memory back.Here is the exact math of what Valgrind saw:The C++ Engine's Setup: +1 Alloc, 
+1 FreeYour Manual Code: +1 Alloc (new), +2 Frees (delete twice)Total: 2 Allocs, 3 FreesThe Core Error: 
Invalid free()Now look at the big red warning at the top of your log:Invalid free() / delete / delete[] / realloc()This is Valgrind 
screaming at you for trying to return the same key twice.The hotel manager (the OS) wiped Room 400 the first time you called delete. 
The second time you called it, the manager panicked and threw an Invalid free() error because you are trying to unlock a room that is 
already available. If this happened in a real server without Valgrind watching, the program would instantly 
crash with a "Segmentation Fault."Look at the LEAK SUMMARY at the bottom. It says 0 bytes definitely lost.
 You didn't leak any memory this time! You just broke the logic of how memory is returned.
*/