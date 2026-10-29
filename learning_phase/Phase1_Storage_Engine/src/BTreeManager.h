#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <mutex>

#include "BTreeNode.h"
#include "StorageEngine.h"

class BTreeManager {
public:
    explicit BTreeManager(StorageEngine* engine) : storage(engine) {}

    void insert(int32_t key);   //for inserting an element
    bool search(int32_t key);
    bool remove(int32_t key);  // soft delete (tombstone)

    // Inclusive [min_key, max_key], returned in ascending order.
    std::vector<int32_t> range_search(int32_t min_key, int32_t max_key);

    void vacuum();  // Permanently purges tombstoned keys from leaf pages and re-packs them.

private:
    StorageEngine* storage;

    /*
    id 0 has two purpose:
    Storage Role: Page 0 is reserved exclusively for DatabaseHeader metadata on disk/RAM (it is never a regular B-Tree node).  
Locking Role:ID 0 (ROOT_LOCK_ID) is reused inside node_locks[0] to lock and protect root_page_id changes during tree updates.
    
    
    */

    static constexpr uint32_t ROOT_LOCK_ID = 0;

    std::unordered_map<uint32_t, std::shared_mutex> node_locks;  
    // Layer 2: one lock per page (RAM only)
    std::mutex map_guard;                                        // Layer 3: protects node_locks itself

    void lock_page(uint32_t page_id, bool exclusive);
    void unlock_page(uint32_t page_id, bool exclusive);

    // ree helpers 
    BTreeNode* fetch_node(uint32_t page_id);
    uint32_t   alloc_node(bool is_leaf);

    /*insert() function uses optimistic first (shared locks down, exclusive on the leaf).if it Returns false if it must be redone with exclusive crabbing + splits.*/
    
    bool insert_optimistic(int32_t key);
    void insert_pessimistic(int32_t key);

    void split_child(BTreeNode* parent, int idx, BTreeNode* child);

    // Caller has already shared-locked page_id; the helper releases it.
    void range_search_helper(uint32_t page_id, int32_t min_key, int32_t max_key,
                             std::vector<int32_t>& result);
    // Caller has already locked page_id (exclusive if leaf, else shared); the helper releases it.
    void vacuum_helper(uint32_t page_id);

    static int lower_bound_index(const BTreeNode* node, int32_t key);
};