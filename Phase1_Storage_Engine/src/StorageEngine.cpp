#include "StorageEngine.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>

StorageEngine::~StorageEngine() {
    close_database();
}

bool StorageEngine::grow_file(size_t new_size) {
    if (!is_open() || new_size <= file_size) return false;

    if (new_size % PAGE_SIZE != 0) {
        new_size += PAGE_SIZE -(new_size % PAGE_SIZE);
    }

    if (mmap_ptr) {
        ::msync(mmap_ptr, file_size, MS_SYNC);
        ::munmap(mmap_ptr, file_size);
        mmap_ptr= nullptr;
    }

    if (::ftruncate(fd, static_cast<off_t>(new_size)) == -1) {
        return false;
    }

    file_size =new_size;
    mmap_ptr = ::mmap(nullptr, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mmap_ptr == MAP_FAILED) {
        mmap_ptr =nullptr;
        return false;
    }
    return true;
}

bool StorageEngine::shrink_to_fit() {
    if (!is_open()) return false;

    DatabaseHeader* header = get_header();
    size_t needed_size = static_cast<size_t>(header->page_count) * PAGE_SIZE;
    if (needed_size < 2 * PAGE_SIZE) {
        needed_size = 2 * PAGE_SIZE;
    }

    if (needed_size >= file_size) return true;

    if (mmap_ptr) {
        ::msync(mmap_ptr, file_size, MS_SYNC);
        ::munmap(mmap_ptr, file_size);
        mmap_ptr = nullptr;
    }

    if (::ftruncate(fd, static_cast<off_t>(needed_size)) == -1) {
        return false;
    }

    file_size = needed_size;
    mmap_ptr = ::mmap(nullptr, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mmap_ptr == MAP_FAILED) {
        mmap_ptr = nullptr;
        return false;
    }
    return true;
}

bool StorageEngine::open_database(const std::string& filename, size_t initial_size) {
    if (is_open())  // already mapped
        return false;                 
    if (initial_size < 2 * PAGE_SIZE)   //given size is not sufficient for creating two pages (header+ page1)
         return false;

    // Round the mapping down to a whole number of pages.
    initial_size -= initial_size % PAGE_SIZE; 
    /*
    Excluding the leftover byte, for say initial size:100000 two page we need 
    4096*2=8192 , remaining: 10000-8192=1808 byte <-- we r just extracting this leftover 
    
    */

    fd = ::open(filename.c_str(), O_RDWR | O_CREAT, 0666);
    if (fd == -1) return false;

    struct stat st {};
    if (::fstat(fd, &st) == -1) {
        ::close(fd);
        fd = -1;
        return false;
    }

    // Never shrink an existing file: that would truncate live pages.
    file_size = (static_cast<size_t>(st.st_size) > initial_size)
                    ? static_cast<size_t>(st.st_size)
                    : initial_size;

    if (::ftruncate(fd, static_cast<off_t>(file_size)) == -1) {
        ::close(fd);
        fd= -1;
        file_size = 0;
        return false;
    }

    mmap_ptr = ::mmap(nullptr, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mmap_ptr == MAP_FAILED) {
        mmap_ptr = nullptr;
        ::close(fd);
        fd= -1;
        file_size =0;
        return false;
    }

    DatabaseHeader* header = get_header();
    if (header->magic_number != VELO_MAGIC) {
        // Fresh database: page 0 is the header, and there is no root yet.
        std::memset(mmap_ptr, 0, PAGE_SIZE);
        header->magic_number = VELO_MAGIC;
        header->db_version   = VELO_VERSION;
        header->page_size    = static_cast<uint32_t>(PAGE_SIZE);
        header->page_count   = 1;
        header->root_page_id = INVALID_PAGE;
    } else if (header->page_size != PAGE_SIZE || header->db_version != VELO_VERSION) {
        close_database();
        return false;
    }
    return true;
}

void StorageEngine::close_database() {
    if (mmap_ptr) {
        shrink_to_fit();
        ::msync(mmap_ptr, file_size, MS_SYNC);
        ::munmap(mmap_ptr, file_size);
        mmap_ptr = nullptr;
    }
    if (fd != -1) {
        ::close(fd);
        fd = -1;
    }
    file_size = 0;
}

bool StorageEngine::sync() {
    if (!is_open()) return false;
    return ::msync(mmap_ptr, file_size, MS_SYNC) == 0;
}

uint32_t StorageEngine::allocate_page() {
    DatabaseHeader* header = get_header();
    if (header->page_count >= max_pages()) {
        size_t growth_step = 16 * PAGE_SIZE; // grows dynamically in 64KB increments
        if (!grow_file(file_size + growth_step)) {
            return INVALID_PAGE;
        }
        header = get_header();
    }

    uint32_t page_id = header->page_count++;
    // Pages are never recycled yet, but zeroing keeps allocation deterministic
    // even if the file is reopened or a free list is added later.
    std::memset(get_page(page_id), 0, PAGE_SIZE);
    return page_id;
}

DatabaseHeader* StorageEngine::get_header() {
    return static_cast<DatabaseHeader*>(mmap_ptr);
}

void* StorageEngine::get_page(uint32_t page_id) {
    if (!is_open() || page_id >= max_pages()) return nullptr;
    return static_cast<char*>(mmap_ptr) + (static_cast<size_t>(page_id) * PAGE_SIZE);
}