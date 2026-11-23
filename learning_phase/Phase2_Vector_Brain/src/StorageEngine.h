


#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include<shared_mutex>
#include<mutex>
 
constexpr size_t   PAGE_SIZE    = 4096;
constexpr uint32_t VELO_MAGIC   = 0x56454C4F;  // 'VELO' it's used to check if the file is database file or not
constexpr uint32_t VELO_VERSION = 1;
 
// Address space reserved ONCE when the database opens (1 GiB = 262,144 pages).
// The file can grow up to this size without the mapping ever moving, so a
// BTreeNode* handed out by get_page() never becomes a dangling pointer.
constexpr size_t   MAX_DB_BYTES = size_t(1) << 30;
/*
constexpr: a constant whose value is fixed at compile time.
size_t: an unsigned number type, 64-bit on your machine, used for sizes.
size_t(1) << 30: take 1 and shift its bits left 30 places. That is 2³⁰ = 1 GiB. Starting from size_t(1) keeps the math 64-bit, so a larger shift can’t overflow like a plain int.

*/
 
// Page 0 always holds the header, actually hold the page metadata
constexpr uint32_t INVALID_PAGE = 0;
 
struct DatabaseHeader {
    uint32_t magic_number; // for now magic number is set to velomagic
    uint32_t db_version;
    uint32_t page_size;
    uint32_t page_count;    // total no of pages we have 
    uint32_t root_page_id;  // hold the id of present root id
};
 
static_assert(sizeof(DatabaseHeader) <= PAGE_SIZE,  "DatabaseHeader must fit inside page 0");
 
class StorageEngine {
private:
    int    fd =-1;
    void*  mmap_ptr  = nullptr;
    size_t file_size = 0;   // bytes of the file that really exist
    size_t map_size  = 0;   // bytes of address space mapped (>= file_size, fixed while open)
    mutable std::shared_mutex engine_mutex;
 
    bool grow_file(size_t new_size);
 
    // Caller must ALREADY hold engine_mutex. Used inside functions that lock it themselves,
    // because calling the public get_header() there would lock it a second time (deadlock).
    DatabaseHeader* get_header_nolock();
 
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
 