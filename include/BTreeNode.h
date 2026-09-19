#pragma once
#include <cstdint>

#include "StorageEngine.h"

/*
 0 is occupied by is_lasf( now we have : 1 size)
we take 1 as padding (size:2)
from 2,3 <--num_keys(2+2=4)
from 4, 4*545=1816 total :1816+4=1820
tombstone:454 
1820+454=2274+2 offset =2276
2276+1820=4096

*/
constexpr int MAX_KEYS = 454;

struct BTreeNode {
    bool     is_leaf;
    uint16_t num_keys;
    int32_t  keys[MAX_KEYS];
    bool     tombstone[MAX_KEYS];
    uint32_t children[MAX_KEYS + 1];
};

static_assert(sizeof(BTreeNode) <= PAGE_SIZE,
              "BTreeNode must fit inside a single page"); //If the struct's size ever exceeds 4096 byte(typed later) a single node would spill into the next page a This assert stops that at compile time instead of letting it happen silently at runtime.
static_assert(MAX_KEYS >= 3, "A B-Tree node needs room to split"); 

/*
NOTES:
THINKING BEHIND THE HEADER:
    Structure padding is the process where a compiler adds empty (unused) bytes inside a data structure to align each member to a memory address that is a multiple of its size, allignment matters bcause CPUs read memory in chunks like 4 or 8 byte at a time.
    HOW PADDING WORK WITH EXAMPLE:
        struct Example {
        char c; //1 byte
        int i;   //4byte
        char d; //1 byte};
    here the total size of the struct is 12 not 6 , because of padding,
        offset:0  size:1 Byte <--char c
        offser:1,2,3  padding(3 byte)Empty bytes added so the next member (int i) can start at Offset 4 (divisible by 4).
        int i->4byte(4,5,6,7)
        1byte (at 8) <--chat d
        9,10,11<--Trailing bytes added to bring the total size up to 12, which is a multiple of the largest member (4 bytes).
        total size:12

        


*/