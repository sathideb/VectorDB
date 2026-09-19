#include "BTreeManager.h"

#include <algorithm>
#include <stdexcept>

BTreeNode* BTreeManager::fetch_node(uint32_t page_id) {
    void* page = storage->get_page(page_id);
    if (!page) throw std::runtime_error("BTreeManager: page id out of range");
    return static_cast<BTreeNode*>(page);
}

uint32_t BTreeManager::alloc_node(bool is_leaf) {
    uint32_t page_id =storage->allocate_page();
    if (page_id == INVALID_PAGE) {
        throw std::runtime_error("BTreeManager: storage exhausted");
    }
    BTreeNode* node = fetch_node(page_id);
    node->is_leaf   = is_leaf;
    node->num_keys  = 0;
    return page_id;
}

// First index i such that node->keys[i] >= key.
int BTreeManager::lower_bound_index(const BTreeNode* node, int32_t key) {
    const int32_t* begin = node->keys;//0
    const int32_t* end   = node->keys + node->num_keys;//3
    return static_cast<int>(std::lower_bound(begin, end, key) - begin);
}

bool BTreeManager::search(int32_t key) {
    uint32_t curr_id = storage->get_header()->root_page_id;

    while (curr_id != INVALID_PAGE) {
        BTreeNode* node = fetch_node(curr_id);
        int i= lower_bound_index(node, key);

        if (i < node->num_keys && node->keys[i] == key) {
            return !node->tombstone[i];
        }
        if (node->is_leaf) return false;
        curr_id = node->children[i];
    }
    return false;
}

bool BTreeManager::remove(int32_t key) {
    uint32_t curr_id = storage->get_header()->root_page_id;

    while (curr_id != INVALID_PAGE) {
        BTreeNode* node = fetch_node(curr_id);
        int i= lower_bound_index(node, key);

        if (i < node->num_keys && node->keys[i] == key) {
            if (node->tombstone[i]) return false;  // already deleted
            node->tombstone[i] = true;
            return true;
        }
        if (node->is_leaf) return false;
        curr_id = node->children[i];
    }
    return false;
}

bool BTreeManager::revive_if_present(int32_t key) {
    uint32_t curr_id = storage->get_header()->root_page_id; //0,1

    while (curr_id != INVALID_PAGE) {
        BTreeNode* node = fetch_node(curr_id);
        int i= lower_bound_index(node, key);

        if (i < node->num_keys && node->keys[i] == key) {
            node->tombstone[i] = false;  // resurrect, or leave a live key alone
            return true;
        }
        if (node->is_leaf) return false;
        curr_id = node->children[i];
    }
    return false;
}

void BTreeManager::insert(int32_t key) {
    // Keys are unique: a re-insert of a tombstoned key resurrects it instead of
    // creating a second copy the search path would never reach.
    if (revive_if_present(key)) return; // if true skip further insertion if false then insert

    DatabaseHeader* header = storage->get_header(); 

    if (header->root_page_id == INVALID_PAGE) {
        uint32_t root_id= alloc_node(/*is_leaf=*/true); //1
        header= storage->get_header();
        header->root_page_id = root_id;
        insert_non_full(fetch_node(root_id), root_id, key); //node, id, key
        return;
    }

    uint32_t   root_id = header->root_page_id; //1
    BTreeNode* root= fetch_node(root_id);

    if (root->num_keys == MAX_KEYS) {
        uint32_t   new_root_id = alloc_node(/*is_leaf=*/false); //page2
        BTreeNode* new_root = fetch_node(new_root_id);
        new_root->children[0]= root_id;//page2-->child[0]=page1

        header  = storage->get_header();
        header->root_page_id = new_root_id;

        split_child(new_root, 0, fetch_node(root_id));
        insert_non_full(new_root, new_root_id, key);
    } else {
        insert_non_full(root, root_id, key);
    }
}

// Splits a full "child" sitting at parent->children[idx]. The median key moves
// up into "parent"; the upper half moves into a freshly allocated sibling.
void BTreeManager::split_child(BTreeNode* parent, int idx, BTreeNode* child) {
    const int total = child->num_keys; //3
    const int mid   = total / 2;  // index of the key promoted to the parent
    //1
    uint32_t   new_node_id = alloc_node(child->is_leaf); //3
    BTreeNode* new_node    = fetch_node(new_node_id);//page3

    new_node->num_keys = static_cast<uint16_t>(total - mid - 1); //3-1-1=1 page3 hold 1 data

    for (int j = 0; j < new_node->num_keys; ++j) {
        //Keys strictly after mid are moved into Page 3.
        new_node->keys[j]      = child->keys[mid + 1 + j]; //page3[0]->page1[2]
        new_node->tombstone[j] = child->tombstone[mid + 1 + j];
    }
    if (!child->is_leaf) {
        for (int j = 0; j <= new_node->num_keys; ++j) {
            new_node->children[j] = child->children[mid + 1 + j];
        }
    }

    const int32_t promoted_key  = child->keys[mid];
    const bool    promoted_dead = child->tombstone[mid];
    child->num_keys= static_cast<uint16_t>(mid);

    // Make room in the parent: child pointers first, then keys.
    for (int j = parent->num_keys; j >= idx + 1; --j) {
        parent->children[j + 1] = parent->children[j];
    }
    parent->children[idx + 1] = new_node_id;

    for (int j = parent->num_keys - 1; j >= idx; --j) {
        parent->keys[j + 1]= parent->keys[j];
        parent->tombstone[j + 1]= parent->tombstone[j];
    }
    parent->keys[idx]= promoted_key;
    parent->tombstone[idx] = promoted_dead;
    parent->num_keys++;
}

void BTreeManager::insert_non_full(BTreeNode* node, uint32_t node_page_id, int32_t key) {
    (void)node_page_id;  // kept for symmetry with the recursive descent

    if (node->is_leaf) {
        int i = node->num_keys - 1; //0-1=-1, 1-1=0,2-1=1
        while (i >= 0 && node->keys[i] > key) { //while is not excuted for i=-1
            node->keys[i + 1]      = node->keys[i];
            node->tombstone[i + 1] = node->tombstone[i];
            --i; //-1
        }
        node->keys[i + 1]= key; //0
        node->tombstone[i + 1] = false;
        node->num_keys++;
        return;
    }

    int i= lower_bound_index(node, key);
    uint32_t child_id = node->children[i];

    if (fetch_node(child_id)->num_keys == MAX_KEYS) {
        split_child(node, i, fetch_node(child_id));
        // The median just landed at node->keys[i]; go right if the key is larger.
        if (key > node->keys[i]) ++i;
        child_id = node->children[i];
    }
    insert_non_full(fetch_node(child_id), child_id, key);
}

std::vector<int32_t> BTreeManager::range_search(int32_t min_key, int32_t max_key) {
    std::vector<int32_t> result;
    if (min_key > max_key) return result;

    uint32_t root_id = storage->get_header()->root_page_id;
    if (root_id == INVALID_PAGE) return result;

    range_search_helper(root_id, min_key, max_key, result);
    return result;
}

void BTreeManager::range_search_helper(uint32_t page_id, int32_t min_key, int32_t max_key,
                                       std::vector<int32_t>& result) {
    if (page_id == INVALID_PAGE) return;

    BTreeNode* node = fetch_node(page_id);

    for (int i = 0; i < node->num_keys; ++i) {
        // children[i] holds keys strictly below keys[i]; skip it only when the
        // whole subtree is below the window.
        if (!node->is_leaf && node->keys[i] >= min_key) {
            range_search_helper(node->children[i], min_key, max_key, result);
        }
        if (node->keys[i] > max_key) return;  // everything further right is larger

        if (node->keys[i] >= min_key && !node->tombstone[i]) {
            result.push_back(node->keys[i]);
        }
    }

    if (!node->is_leaf) {
        range_search_helper(node->children[node->num_keys], min_key, max_key, result);
    }
}

void BTreeManager::vacuum() {
    uint32_t root_id = storage->get_header()->root_page_id;
    if (root_id == INVALID_PAGE) return;
    vacuum_helper(root_id);
}

// Only leaves are compacted. A key stored in an internal node is also a
// separator: dropping it would orphan one of its two child pointers and lose an
// entire subtree. Tombstoned separators stay in place and keep routing traffic,
// while search() and range_search() already skip them.
void BTreeManager::vacuum_helper(uint32_t page_id) {
    if (page_id == INVALID_PAGE) return;

    BTreeNode* node = fetch_node(page_id);

    if (!node->is_leaf) {
        for (int i = 0; i <= node->num_keys; ++i) {
            vacuum_helper(node->children[i]);
        }
        return;
    }

    int write_idx = 0;
    for (int read_idx = 0; read_idx < node->num_keys; ++read_idx) {
        if (node->tombstone[read_idx]) continue;
        if (write_idx != read_idx) {
            node->keys[write_idx]      = node->keys[read_idx];
            node->tombstone[write_idx] = false;
        }
        ++write_idx;
    }

    for (int i = write_idx; i < node->num_keys; ++i) {
        node->tombstone[i] = false;
        node->keys[i]      = 0;
    }
    node->num_keys = static_cast<uint16_t>(write_idx);
}
