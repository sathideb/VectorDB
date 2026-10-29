
#include "BTreeManager.h"
#include <algorithm>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
 
// NOTES:
//  LOCKING MAP (read this first)
// for concurrency we r using total three layer Architecture as follows:
//
//  Layer 1  BTreeNoderaw bytes in the mmap'd page(never holds a lock)
//  Layer 2  node_locks[id] one std::shared_mutex per page(only in RAM)
//  Layer 3  map_guard :protects node_locks itself (held only for the lookup)
//
//  ROOT_LOCK_ID (= 0) is a lock on "the root pointer" (header->root_page_id). <-- here we Actually locking the header metadata for safety purpose and to avoid misleading metadata changes
//  Page 0 is the DB header, never a tree node, so id 0 is free to use as that lock.
//  Think of it as the "parent of the root".
//
//  Rules used everywhere below:
//   * Lock order is always TOP -> DOWN (root latch, root, child, grandchild...).
//     Nobody ever waits upward, so there is no cycle, so no deadlock.
//   * Hand-over-hand: lock the child FIRST, only then unlock the parent.
//   * A page's is_leaf never changes after alloc_node(), and a child id is only
//     published into its parent while the parent is exclusively locked(back to pessimist crabbing). So while
//     we hold the parent we may peek child->is_leaf to pick the lock mode.
// 
 
BTreeNode* BTreeManager::fetch_node(uint32_t page_id) {    //It's coverting the raw pointer from the get_page() to the BTreeNode* type.
    void* page = storage->get_page(page_id);
    if (!page) throw std::runtime_error("BTreeManager: page id out of range");
    return static_cast<BTreeNode*>(page);
}
 
uint32_t BTreeManager::alloc_node(bool is_leaf) {
    uint32_t page_id = storage->allocate_page();
    if (page_id == INVALID_PAGE) {
        throw std::runtime_error("BTreeManager: storage exhausted");
    }
    BTreeNode* node = fetch_node(page_id);
    node->is_leaf   = is_leaf;
    node->num_keys  = 0;
    return page_id;
}
 
// First index i such that node->keys[i] >= key. example : key: 20   array=[10,30,50] will return 1
int BTreeManager::lower_bound_index(const BTreeNode* node, int32_t key) {
    const int32_t* begin = node->keys;
    const int32_t* end   = node->keys + node->num_keys;
    return static_cast<int>(std::lower_bound(begin, end, key) - begin);
}
 
// Insert into a leaf that is guaranteed to have room. Caller holds it exclusively.
static void leaf_insert(BTreeNode* node, int32_t key) {
    int i = node->num_keys - 1;
    while (i >= 0 && node->keys[i] > key) {
        node->keys[i + 1]=node->keys[i];
        node->tombstone[i + 1]= node->tombstone[i];
        --i;
    }
    node->keys[i + 1]      = key;
    node->tombstone[i + 1] = false;
    node->num_keys++;
}
 

//  SEARCH  : shared locks, hand-over-hand, never restarts
bool BTreeManager::search(int32_t key) {
    lock_page(ROOT_LOCK_ID, false);
    uint32_t curr_id = storage->get_header()->root_page_id;
    if (curr_id == INVALID_PAGE) {
        unlock_page(ROOT_LOCK_ID, false);
        return false;
    }
    lock_page(curr_id, false);              // child first...
    unlock_page(ROOT_LOCK_ID, false);       // ...then let go of the parent
 
    while (true) {
        BTreeNode* node = fetch_node(curr_id);
        int i = lower_bound_index(node, key);
 
        if (i < node->num_keys && node->keys[i] == key) {
            bool live = !node->tombstone[i];
            unlock_page(curr_id, false);
            return live;
        }
        if (node->is_leaf) {
            unlock_page(curr_id, false);
            return false;
        }
        uint32_t child_id = node->children[i];
        lock_page(child_id, false);         // grab child
        unlock_page(curr_id, false);        // release parent
        curr_id = child_id;
    }
}
 
//  REMOVE  : tombstone flip only. No ancestor can ever change, so every
//  parent may be released the moment its child is locked (no "is child
//  safe?" check needed). Exclusive because we write a flag.

bool BTreeManager::remove(int32_t key) {
    lock_page(ROOT_LOCK_ID, false);         // root pointer is only READ here
    uint32_t curr_id = storage->get_header()->root_page_id;
    if (curr_id == INVALID_PAGE) {
        unlock_page(ROOT_LOCK_ID, false);
        return false;
    }
    lock_page(curr_id, true);
    unlock_page(ROOT_LOCK_ID, false);
 
    while (true) {
        BTreeNode* node = fetch_node(curr_id);
        int i = lower_bound_index(node, key);
 
        if (i < node->num_keys && node->keys[i] == key) {
            bool was_live = !node->tombstone[i];
            node->tombstone[i] = true;
            unlock_page(curr_id, true);
            return was_live;                // false = it was already deleted
        }
        if (node->is_leaf) {
            unlock_page(curr_id, true);
            return false;
        }
        uint32_t child_id = node->children[i];
        lock_page(child_id, true);
        unlock_page(curr_id, true);
        curr_id = child_id;
    }
}
 //this is where optimistic and pessimist crabbing comes first
//  INSERT  : optimistic first, pessimistic only if a split (or a revive
//  of a tombstoned separator) is needed.
void BTreeManager::insert(int32_t key) {
    if (insert_optimistic(key)) return;     // fast path: shared locks on the way down
    insert_pessimistic(key);                // rare path: exclusive crabbing + splits
}
 
// Returns true if the insert finished. Returns false = "restart me pessimistically".
// Nothing has been modified when it returns false, so restarting is free.
bool BTreeManager::insert_optimistic(int32_t key) {
    lock_page(ROOT_LOCK_ID, false);
    uint32_t curr_id = storage->get_header()->root_page_id;
    if (curr_id == INVALID_PAGE) {          // empty tree: need to create a root
        unlock_page(ROOT_LOCK_ID, false);
        return false;
    }
 
    // Shared on internal nodes (we only look), exclusive on a leaf (we may write).
    bool curr_leaf = fetch_node(curr_id)->is_leaf;
    lock_page(curr_id, curr_leaf);
    unlock_page(ROOT_LOCK_ID, false);
 
    while (true) {
        BTreeNode* node = fetch_node(curr_id);
        int i = lower_bound_index(node, key);
        bool found = (i < node->num_keys && node->keys[i] == key);
 
        if (node->is_leaf) {                // we hold this one EXCLUSIVELY
            if (found) {
                node->tombstone[i] = false; // revive, or leave a live key alone
                unlock_page(curr_id, true);
                return true;
            }
            if (node->num_keys == MAX_KEYS) {   // would need a split
                unlock_page(curr_id, true);
                return false;
            }
            leaf_insert(node, key);
            unlock_page(curr_id, true);
            return true;
        }
 
        // internal node, held SHARED
        if (found) {
            bool live = !node->tombstone[i];
            unlock_page(curr_id, false);
            return live;                    // live -> nothing to do. dead -> needs a write -> pessimistic
        }
 
        uint32_t child_id   = node->children[i];
        bool     child_leaf = fetch_node(child_id)->is_leaf;
        lock_page(child_id, child_leaf);    // child first
        unlock_page(curr_id, false);        // then parent (always safe: we are only reading)
        curr_id = child_id;
    }
}
 
// Exclusive crabbing with proactive splitting (your original insert_non_full idea):
// every node we step INTO is guaranteed non-full, so a split below it can only
// touch it, never its parent. So as soon as the child is settled, the parent
// (and everything above) is released. We never hold more than parent + child.
void BTreeManager::insert_pessimistic(int32_t key) {
    lock_page(ROOT_LOCK_ID, true);          // we might change root_page_id
 
    // Re-check under the lock: another thread may have created the root meanwhile.
    if (storage->get_header()->root_page_id == INVALID_PAGE) {
        uint32_t root_id = alloc_node(/*is_leaf=*/true);
        storage->get_header()->root_page_id = root_id;   // re-fetch header after alloc
        lock_page(root_id, true);
        unlock_page(ROOT_LOCK_ID, true);
        leaf_insert(fetch_node(root_id), key);
        unlock_page(root_id, true);
        return;
    }
 
    uint32_t curr_id = storage->get_header()->root_page_id;
    lock_page(curr_id, true);
 
    if (fetch_node(curr_id)->num_keys == MAX_KEYS) {     // root full -> tree grows a level
        uint32_t new_root_id = alloc_node(/*is_leaf=*/false);
        {   // The new root is brand new and unreachable (we hold the root latch), so nobody
            // can possibly hold its lock: try_lock() always succeeds. We don't use
            // lock_page() here because that would lock "child (old root) then parent
            // (new root)", the reverse of the normal top-down order.
            std::lock_guard<std::mutex> g(map_guard);
            bool ok = node_locks[new_root_id].try_lock();
            if (!ok) throw std::logic_error("BTreeManager: fresh root page was already locked");
        }
        BTreeNode* new_root = fetch_node(new_root_id);
        new_root->children[0] = curr_id;
        storage->get_header()->root_page_id = new_root_id;
        split_child(new_root, 0, fetch_node(curr_id));
        unlock_page(curr_id, true);          // old root is now just a half-full child
        curr_id = new_root_id;
    }
    // curr is non-full now, so the root pointer can never change under us again.
    unlock_page(ROOT_LOCK_ID, true);
 
    while (true) {
        BTreeNode* node = fetch_node(curr_id);
        int i = lower_bound_index(node, key);
 
        if (i < node->num_keys && node->keys[i] == key) {
            node->tombstone[i] = false;      // revive (or leave a live key alone)
            unlock_page(curr_id, true);
            return;
        }
        if (node->is_leaf) {                 // non-full guaranteed, we split on the way down
            leaf_insert(node, key);
            unlock_page(curr_id, true);
            return;
        }
 
        uint32_t child_id = node->children[i];
        lock_page(child_id, true);
        BTreeNode* child = fetch_node(child_id);
 
        if (child->num_keys == MAX_KEYS) {
            split_child(node, i, child);     // node has room (invariant), so only node changes
            node = fetch_node(curr_id);      // re-fetch after alloc inside split
 
            if (key == node->keys[i]) {      // the median that just moved up IS our key
                node->tombstone[i] = false;
                unlock_page(child_id, true);
                unlock_page(curr_id, true);
                return;
            }
            if (key > node->keys[i]) {       // go to the new right sibling instead
                uint32_t right_id = node->children[i + 1];
                lock_page(right_id, true);   // uncontended: only reachable through node
                unlock_page(child_id, true);
                child_id = right_id;
            }
        }
        unlock_page(curr_id, true);          // child is settled & non-full -> release parent
        curr_id = child_id;
    }
}
 
// Splits a full "child" sitting at parent->children[idx]. The median key moves
// up into "parent"; the upper half moves into a freshly allocated sibling.
// Caller holds parent AND child exclusively.
void BTreeManager::split_child(BTreeNode* parent, int idx, BTreeNode* child) {
    const int total = child->num_keys;
    const int mid   = total / 2;  // index of the key promoted to the parent
 
    uint32_t   new_node_id = alloc_node(child->is_leaf);
    BTreeNode* new_node    = fetch_node(new_node_id);
 
    new_node->num_keys = static_cast<uint16_t>(total - mid - 1);
 
    for (int j = 0; j < new_node->num_keys; ++j) {
        new_node->keys[j]      = child->keys[mid + 1 + j];
        new_node->tombstone[j] = child->tombstone[mid + 1 + j];
    }
    if (!child->is_leaf) {
        for (int j = 0; j <= new_node->num_keys; ++j) {
            new_node->children[j] = child->children[mid + 1 + j];
        }
    }
 
    const int32_t promoted_key  = child->keys[mid];
    const bool    promoted_dead = child->tombstone[mid];
    child->num_keys = static_cast<uint16_t>(mid);
 
    // Make room in the parent: child pointers first, then keys.
    for (int j = parent->num_keys; j >= idx + 1; --j) {
        parent->children[j + 1] = parent->children[j];
    }
    parent->children[idx + 1] = new_node_id;
 
    for (int j = parent->num_keys - 1; j >= idx; --j) {
        parent->keys[j + 1]      = parent->keys[j];
        parent->tombstone[j + 1] = parent->tombstone[j];
    }
    parent->keys[idx]      = promoted_key;
    parent->tombstone[idx] = promoted_dead;
    parent->num_keys++;
}
 
// ---------------------------------------------------------------------
//  RANGE SEARCH : shared lock on each node, held while we walk its subtree
//  (we need the node's keys[] stable between recursive calls). Top-down
//  order only, same as everyone else -> no deadlock.
// ---------------------------------------------------------------------
std::vector<int32_t> BTreeManager::range_search(int32_t min_key, int32_t max_key) {
    std::vector<int32_t> result;
    if (min_key > max_key) return result;
 
    lock_page(ROOT_LOCK_ID, false);
    uint32_t root_id = storage->get_header()->root_page_id;
    if (root_id == INVALID_PAGE) {
        unlock_page(ROOT_LOCK_ID, false);
        return result;
    }
    lock_page(root_id, false);
    unlock_page(ROOT_LOCK_ID, false);
 
    range_search_helper(root_id, min_key, max_key, result);   // unlocks root_id itself
    return result;
}
 
// Contract: the CALLER has already shared-locked page_id. This function
// releases it before returning (every return path below does).
void BTreeManager::range_search_helper(uint32_t page_id, int32_t min_key, int32_t max_key,
                                       std::vector<int32_t>& result) {
    BTreeNode* node = fetch_node(page_id);
 
    for (int i = 0; i < node->num_keys; ++i) {
        // children[i] holds keys strictly below keys[i]; skip it only when the
        // whole subtree is below the window.
        if (!node->is_leaf && node->keys[i] >= min_key) {
            uint32_t child_id = node->children[i];
            lock_page(child_id, false);
            range_search_helper(child_id, min_key, max_key, result);
        }
        if (node->keys[i] > max_key) {        // everything further right is larger
            unlock_page(page_id, false);
            return;
        }
        if (node->keys[i] >= min_key && !node->tombstone[i]) {
            result.push_back(node->keys[i]);
        }
    }
 
    if (!node->is_leaf) {
        uint32_t child_id = node->children[node->num_keys];
        lock_page(child_id, false);
        range_search_helper(child_id, min_key, max_key, result);
    }
    unlock_page(page_id, false);
}
 
//  VACUUM : compacts leaves. Internal nodes are only read (shared),
//  leaves are written (exclusive). Mode is picked by peeking is_leaf.

void BTreeManager::vacuum() {
    lock_page(ROOT_LOCK_ID, false);
    uint32_t root_id = storage->get_header()->root_page_id;
    if (root_id == INVALID_PAGE) {
        unlock_page(ROOT_LOCK_ID, false);
        return;
    }
    bool root_leaf = fetch_node(root_id)->is_leaf;
    lock_page(root_id, root_leaf);
    unlock_page(ROOT_LOCK_ID, false);
 
    vacuum_helper(root_id);                   // unlocks root_id itself
}
 
// Only leaves are compacted. A key stored in an internal node is also a
// separator: dropping it would orphan one of its two child pointers and lose an
// entire subtree. Tombstoned separators stay in place and keep routing traffic,
// while search() and range_search() already skip them.
//
// Contract: caller already holds page_id (exclusive if leaf, shared otherwise).
// This function releases it before returning.
void BTreeManager::vacuum_helper(uint32_t page_id) {
    BTreeNode* node = fetch_node(page_id);
 
    if (!node->is_leaf) {
        for (int i = 0; i <= node->num_keys; ++i) {
            uint32_t child_id   = node->children[i];
            bool     child_leaf = fetch_node(child_id)->is_leaf;
            lock_page(child_id, child_leaf);
            vacuum_helper(child_id);
        }
        unlock_page(page_id, false);
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
    unlock_page(page_id, true);
}
 
// lock mechanisms (my notes: mapguard is locking the hash map itself because it's not thread safe.
 
void BTreeManager::lock_page(uint32_t page_id, bool exclusive) {

    std::shared_mutex* padlock = nullptr;

    {   // STEP 1: Lock the hash map JUST long enough to fetch the page lock address.
        std::lock_guard<std::mutex> g(map_guard);
        padlock = &node_locks[page_id];
    }   // STEP 2: map_guard is ALWAYS destroyed/unlocked here at the scope exit!
        // The hash table is now completely free for other threads.

    // STEP 3: Wait on the individual page lock outside of map_guard.
    if (exclusive)
        padlock->lock();         // Writer mode (Exclusive)
    else
        padlock->lock_shared();  // Reader mode (Shared)
}
 
void BTreeManager::unlock_page(uint32_t page_id, bool exclusive) {
 
    std::shared_mutex* padlock = nullptr;
    {
        std::lock_guard<std::mutex> g(map_guard);
        padlock = &node_locks[page_id];
    }
 
    if (exclusive)
        padlock->unlock();
    else
        padlock->unlock_shared();
}
 
/*
personal notes:
We unlock map_guard before waiting on the node's padlock for two major reasons: preventing a global system bottleneck and avoiding deadlocks.






*/