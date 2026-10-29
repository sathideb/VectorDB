#include "StorageEngine.h"
 
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <shared_mutex>
#include <mutex>
#include <cstring>
 
//  TWO RULES THIS FILE FOLLOWS
//
//  1) A function that already holds engine_mutex must NEVER call a public
//     function that locks it again (get_header, get_page, sync...). A thread
//     cannot take a lock it already holds -> "Resource deadlock avoided".
//     So those functions use the *_nolock helpers instead.
//
//  2) mmap_ptr NEVER MOVES while the database is open. open_database()
//     reserves MAX_DB_BYTES of address space once; growing the file is just
//     ftruncate() (bytes become valid inside the SAME mapping). So a
//     BTreeNode* that a thread got from get_page() stays valid even while
//     another thread allocates pages. (Before: grow_file did munmap+mmap, the
//     mapping moved, and other threads were left holding dangling pointers.)

StorageEngine::~StorageEngine() {
    close_database();
}
 
// PRIVATE HELPER: caller must hold engine_mutex (shared or unique)
DatabaseHeader* StorageEngine::get_header_nolock() {
    return static_cast<DatabaseHeader*>(mmap_ptr);
}
 
// PRIVATE HELPER: caller must already hold unique_lock on engine_mutex.
// Grows the file INSIDE the existing mapping; mmap_ptr does not change.
bool StorageEngine::grow_file(size_t new_size) {
    if (!is_open() || new_size <= file_size) return false;
 
    if (new_size % PAGE_SIZE != 0) {
        new_size += PAGE_SIZE - (new_size % PAGE_SIZE);
    }
    if (new_size > map_size) return false;       // would run past the reserved address range
 
    if (::ftruncate(fd, static_cast<off_t>(new_size)) == -1) {
        return false;
    }
    file_size = new_size;
    return true;
}
 
bool StorageEngine::shrink_to_fit() {
    std::unique_lock<std::shared_mutex> lock(engine_mutex);
    if (!is_open()) return false;
 
    DatabaseHeader* header = get_header_nolock();
    size_t needed_size = static_cast<size_t>(header->page_count) * PAGE_SIZE;
    if (needed_size < 2 * PAGE_SIZE) {
        needed_size = 2 * PAGE_SIZE;
    }
 
    if (needed_size >= file_size) return true;
 
    // Only the file gets shorter; the mapping stays where it is.
    if (::ftruncate(fd, static_cast<off_t>(needed_size)) == -1) {
        return false;
    }
    file_size = needed_size;
    return true;
}
 
bool StorageEngine::open_database(const std::string& filename, size_t initial_size) {
    std::unique_lock<std::shared_mutex> lock(engine_mutex);
    if (is_open()) return false;
    if (initial_size < 2 * PAGE_SIZE) return false;
 
    initial_size -= initial_size % PAGE_SIZE;
 
    fd = ::open(filename.c_str(), O_RDWR | O_CREAT, 0666);
    if (fd == -1) return false;
 
    struct stat st {};
    if (::fstat(fd, &st) == -1) {
        ::close(fd);
        fd = -1;
        return false;
    }
 
    file_size = (static_cast<size_t>(st.st_size) > initial_size)
                    ? static_cast<size_t>(st.st_size)
                    : initial_size;
 
    if (::ftruncate(fd, static_cast<off_t>(file_size)) == -1) {
        ::close(fd);
        fd = -1;
        file_size = 0;
        return false;
    }
 
    // Reserve the whole address range now. Bytes past the end of the file are
    // not usable yet (touching them would SIGBUS) but become valid the moment
    // grow_file() extends the file, at the same addresses.
    map_size = std::max(file_size, MAX_DB_BYTES);
    mmap_ptr = ::mmap(nullptr, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mmap_ptr == MAP_FAILED) {
        mmap_ptr = nullptr;
        ::close(fd);
        fd = -1;
        file_size = 0;
        map_size = 0;
        return false;
    }
 
    DatabaseHeader* header = get_header_nolock();
    if (header->magic_number != VELO_MAGIC) {
        std::memset(mmap_ptr, 0, PAGE_SIZE);
        header->magic_number = VELO_MAGIC;
        header->db_version   = VELO_VERSION;
        header->page_size    = static_cast<uint32_t>(PAGE_SIZE);
        header->page_count   = 1;
        header->root_page_id = INVALID_PAGE;
    } else if (header->page_size != PAGE_SIZE || header->db_version != VELO_VERSION) {
        ::msync(mmap_ptr, file_size, MS_SYNC);
        ::munmap(mmap_ptr, map_size);
        mmap_ptr = nullptr;
        if (fd != -1) {
            ::close(fd);
            fd = -1;
        }
        file_size = 0;
        map_size = 0;
        return false;
    }
    return true;
}
 
void StorageEngine::close_database() {
    std::unique_lock<std::shared_mutex> lock(engine_mutex);
    if (mmap_ptr) {
        // Work out the final size BEFORE unmapping (header lives in the mapping).
        DatabaseHeader* header = get_header_nolock();
        size_t needed_size = static_cast<size_t>(header->page_count) * PAGE_SIZE;
        if (needed_size < 2 * PAGE_SIZE) needed_size = 2 * PAGE_SIZE;
 
        ::msync(mmap_ptr, file_size, MS_SYNC);
        ::munmap(mmap_ptr, map_size);
        mmap_ptr = nullptr;
 
        // Shrink-to-fit inline (no call to shrink_to_fit(): that would lock twice).
        if (needed_size < file_size && fd != -1) {
            if (::ftruncate(fd, static_cast<off_t>(needed_size)) != -1) {
                file_size = needed_size;
            }
        }
    }
    if (fd != -1) {
        ::close(fd);
        fd = -1;
    }
    file_size = 0;
    map_size = 0;
}
 
bool StorageEngine::sync() {
    std::shared_lock<std::shared_mutex> lock(engine_mutex);
    if (!is_open()) return false;
    return ::msync(mmap_ptr, file_size, MS_SYNC) == 0;
}
 
uint32_t StorageEngine::allocate_page() {
    std::unique_lock<std::shared_mutex> lock(engine_mutex);
    if (!is_open()) return INVALID_PAGE;
 
    DatabaseHeader* header = get_header_nolock();
    if (header->page_count >= max_pages()) {
        size_t growth_step = 16 * PAGE_SIZE;
        if (!grow_file(file_size + growth_step)) {
            return INVALID_PAGE;
        }
        // mmap_ptr did not move, so `header` is still valid.
    }
 
    uint32_t page_id = header->page_count++;
 
    // Direct offset math: get_page() would try to lock engine_mutex again.
    void* page_ptr = static_cast<char*>(mmap_ptr) + (static_cast<size_t>(page_id) * PAGE_SIZE);
    std::memset(page_ptr, 0, PAGE_SIZE);
    return page_id;
}
 
DatabaseHeader* StorageEngine::get_header() {
    std::shared_lock<std::shared_mutex> lock(engine_mutex);
    return get_header_nolock();
}
 
void* StorageEngine::get_page(uint32_t page_id) {
    std::shared_lock<std::shared_mutex> lock(engine_mutex);
    if (!is_open() || page_id >= max_pages()) return nullptr;
    return static_cast<char*>(mmap_ptr) + (static_cast<size_t>(page_id) * PAGE_SIZE);
}
 
