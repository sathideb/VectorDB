
//loop test

#include<iostream>
using namespace std;

// we will allocate 40 byte at each time of loop
int main(){

   for (int i= 0 ; i < 5; i++){

    int* client_data = new int[10];

     client_data[0]=i;

   }

return 0;

}

/*
sathi@SATHI:/mnt/d/memory_test$ valgrind --leak-check=full ./leak1_test
==13570== Memcheck, a memory error detector
==13570== Copyright (C) 2002-2024, and GNU GPL'd, by Julian Seward et al.
==13570== Using Valgrind-3.26.0 and LibVEX; rerun with -h for copyright info
==13570== Command: ./leak1_test
==13570== 
==13570== 
==13570== HEAP SUMMARY:
==13570==     in use at exit: 200 bytes in 5 blocks
==13570==   total heap usage: 6 allocs, 1 frees, 73,928 bytes allocated
==13570== 
==13570== 200 bytes in 5 blocks are definitely lost in loss record 1 of 1
==13570==    at 0x48525F3: operator new[](unsigned long) (vg_replace_malloc.c:730)
==13570==    by 0x4001167: main (in /mnt/d/memory_test/leak1_test)
==13570== 
==13570== LEAK SUMMARY:
==13570==    definitely lost: 200 bytes in 5 blocks
==13570==    indirectly lost: 0 bytes in 0 blocks
==13570==      possibly lost: 0 bytes in 0 blocks
==13570==    still reachable: 0 bytes in 0 blocks
==13570==         suppressed: 0 bytes in 0 blocks

. Definitely Lost (The Volcano)
This is exactly what you just did in your loop. You rented a room (new int[10]), but when the loop restarted, you reused the exact same clientData variable to rent a new room. The key to the first room was instantly overwritten and destroyed. The memory is locked, and there is literally no variable anywhere in your program that holds its address anymore. It is 100% gone.

2. Indirectly Lost (The Nested Safes)
Imagine you rent a Master Suite, and inside that suite is a locked safe containing the key to a separate Guest Room.
If you lose the key to the Master Suite, you are completely locked out of both rooms.
In C++, this happens with complex nested data structures (like Linked Lists or Trees). If you lose the pointer to the main "Head" node (which becomes definitely lost), all the child nodes connected to it become indirectly lost. Valgrind is brilliant enough to know that the child nodes only leaked because you lost the parent.

3. Possibly Lost (The Suspicious Key)
Valgrind scans your program's memory looking for the exact address of the rented room. Sometimes, it finds a number in your code that looks like the key, but it's pointing to the middle of the room instead of the front door.
This usually happens if you are doing weird pointer arithmetic (like shifting a pointer 5 bytes forward). Valgrind says, "Bhai, I see a pointer that might belong to this room, but I can't guarantee it is still valid."

4. Still Reachable (The Hoarder)
This is the most interesting one. This means your program finished running, you did not write delete, but you still had the key in your pocket when you walked out of the hotel.
Because the pointer variable was still perfectly valid and pointing to the exact start of the block the millisecond the program terminated, Valgrind classifies it differently. It is basically saying, "You didn't actually lose this memory while the program was running; you were just too lazy to clean it up before shutting down." As we discussed earlier, the OS will bulldoze it anyway, so this isn't as catastrophic as a "definitely lost" leak, but it is still considered sloppy engineering

*/