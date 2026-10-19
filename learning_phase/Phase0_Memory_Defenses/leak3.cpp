
#include<iostream>
using namespace std;

int main(){

    int* data = new int[50];
    delete[] data;

    data[1]=99;

return 0;
}
/*
sathi@SATHI:/mnt/d/memory_test$ g++ leak3.cpp -o leak_test
sathi@SATHI:/mnt/d/memory_test$ valgrind --leak-check=full ./leak3_test
valgrind: ./leak3_test: No such file or directory
sathi@SATHI:/mnt/d/memory_test$ valgrind --leak-check=full ./leak_test
==15075== Memcheck, a memory error detector
==15075== Copyright (C) 2002-2024, and GNU GPL'd, by Julian Seward et al.
==15075== Using Valgrind-3.26.0 and LibVEX; rerun with -h for copyright info
==15075== Command: ./leak_test
==15075== 
==15075== Invalid write of size 4
==15075==    at 0x400119E: main (in /mnt/d/memory_test/leak_test)
==15075==  Address 0x4e98084 is 4 bytes inside a block of size 200 free'd
==15075==    at 0x4856494: operator delete[](void*) (vg_replace_malloc.c:1413)
==15075==    by 0x4001195: main (in /mnt/d/memory_test/leak_test)
==15075==  Block was alloc'd at
==15075==    at 0x48525F3: operator new[](unsigned long) (vg_replace_malloc.c:730)
==15075==    by 0x400117E: main (in /mnt/d/memory_test/leak_test)
==15075== 
==15075== 
==15075== HEAP SUMMARY:
==15075==     in use at exit: 0 bytes in 0 blocks
==15075==   total heap usage: 2 allocs, 2 frees, 73,928 bytes allocated
==15075== 
==15075== All heap blocks were freed -- no leaks are possible
==15075== 
==15075== For lists of detected and suppressed errors, rerun with: -s
==15075== ERROR SUMMARY: 1 errors from 1 contexts (suppressed: 0 from 0)
*/

/*
he Ghost Room Breakdown
Look at the massive red flag Valgrind threw at you:

==15075== Invalid write of size 4
==15075==  Address 0x4e98084 is 4 bytes inside a block of size 200 free'd

Here is the exact hardware logic of what the machine is screaming about:

The "4 bytes": You tried to assign an integer into the array. In this architecture, an integer takes up exactly 4 bytes of physical RAM.

The Crime: Valgrind is pointing directly to the fact that you tried to write those 4 bytes into a block of memory that was already freed by your delete[] command right before it.

The Danger of the Dangling Pointer
This is the most dangerous hardware flaw in all of C++ programming. We call it a Dangling Pointer.

You gave the memory back to the hotel manager (the OS), and the manager marked the room as empty. But your pointer variable still held the physical memory address (the ghost key). When you wrote data into it, you essentially snuck back into the hotel and shoved an integer into a room you no longer owned.

Why is this catastrophic in a real, long-running system?
Because in a split second, the OS might have already assigned that exact memory address to another running program. By sneaking in and writing data there, you could quietly corrupt another program's live memory. The OS acts as a ruthless guard—the moment it catches you doing an invalid write, it kills your process instantly and throws a fatal Segmentation Fault.

We have successfully bridged the C++ engine into a pure Linux kernel, bypassed the graphical interface completely, and intentionally triggered the three most fatal hardware flaws in computer science. You didn't just read the theory; you built the traps and caught the exact logical flaws in real-time


*/