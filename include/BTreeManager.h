#pragma once
#include <cstdint>
#include <vector>

#include "BTreeNode.h"
#include "StorageEngine.h"

class BTreeManager {
public:
    explicit BTreeManager(StorageEngine* engine) : storage(engine) {}

    // Unique-key semantics: inserting a key that already exists is a no-op,
    // and inserting a tombstoned key resurrects it.
    void insert(int32_t key);

    bool search(int32_t key);
    bool remove(int32_t key);  // soft delete (tombstone)

    // Inclusive [min_key, max_key], returned in ascending order.
    std::vector<int32_t> range_search(int32_t min_key, int32_t max_key);

    // Permanently purges tombstoned keys from leaf pages and re-packs them.
    void vacuum();

private:
    StorageEngine* storage;

    BTreeNode* fetch_node(uint32_t page_id);
    uint32_t   alloc_node(bool is_leaf);

    // Returns true if the key was already present; resurrects it if tombstoned.
    bool revive_if_present(int32_t key);

    void split_child(BTreeNode* parent, int idx, BTreeNode* child);
    void insert_non_full(BTreeNode* node, uint32_t node_page_id, int32_t key);

    void range_search_helper(uint32_t page_id, int32_t min_key, int32_t max_key,
                             std::vector<int32_t>& result);
    void vacuum_helper(uint32_t page_id);

    static int lower_bound_index(const BTreeNode* node, int32_t key);
};
