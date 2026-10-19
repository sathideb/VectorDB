
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

constexpr size_t   PAGE_SIZE    = 4096;
constexpr uint32_t VELO_MAGIC   = 0x56454C4F;  // 'VELO' it's used to check if the file is database file or not
constexpr uint32_t VELO_VERSION = 1;

// Page 0 always holds the header, actually hold the page metadata
constexpr uint32_t INVALID_PAGE = 0;

struct DatabaseHeader {
    uint32_t magic_number; // for now magic number is set to velomagic
    uint32_t db_version;
    uint32_t page_size;
    uint32_t page_count;    // total no of pages we have 
    uint32_t root_page_id;  // hold the id of present root id
};

static_assert(sizeof(DatabaseHeader) <= PAGE_SIZE, 
                                        "DatabaseHeader must fit inside page 0");

class StorageEngine {
private:
    int    fd =-1;
    void*  mmap_ptr  = nullptr;
    size_t file_size = 0;

    bool grow_file(size_t new_size);

public:
    StorageEngine() = default;
    ~StorageEngine();

    // The engine owns an fd and a mapping; copying it would double-free both.
    StorageEngine(const StorageEngine&)            = delete; 
    StorageEngine& operator=(const StorageEngine&) = delete;  // to avoid the double destruction mechanism

    bool open_database(const std::string& filename, size_t initial_size = 2 * PAGE_SIZE); //ull->unsigned long long
    void close_database();
    bool sync();  // flush dirty pages to disk

    bool shrink_to_fit();  // auto-shrinks file to match exact page_count size

    // Returns INVALID_PAGE when the mapping is exhausted.
    uint32_t allocate_page();

    DatabaseHeader* get_header();
    void* get_page(uint32_t page_id);

    bool is_open()   const { return mmap_ptr != nullptr; }
    uint32_t max_pages() const { return static_cast<uint32_t>(file_size / PAGE_SIZE); }
};

/*
NOTE1: static_assert is a check that runs at compile time , not at the run tim, if somehow the condition is false , the program
refuses to compile and the compiler prints the given msg.
here it's simple task is to check if the DatabaseHeader actually fit in the 4096 byte i have reserved for page0?(right now it's20 byte so passes, in future if anyhow the metadata exceeds 4096 this assart alarts and give us that msg.

WHY I HAVE WRITEN SYNC() EVEN I HAVE DONE MSSYNC()?
If msync only ran there, your data would only be durable on a clean shutdown. But what if the process crashes, or the machine loses power, mid-session.. after I've inserted a million keys but before you've called close_database? Everything since the last flush is gone.

sync() gives the caller a checkpoint they control: "I've just finished a batch of important writes, force them to disk now, before continuing." Real databases call this after committing a transaction — that's literally what "durability" in ACID means.

*/