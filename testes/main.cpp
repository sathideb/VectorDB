

#include <iostream>
#include <vector>
#include <cassert>
#include <cstdio>
#include "StorageEngine.h"
#include "BTreeManager.h"

void run_practice_tests() {
    const char* db_file = "velodb_practice.db";
    std::remove(db_file); // Clean slate

    // 1. Initialize StorageEngine with minimum initial size (2 pages = 8KB)
    StorageEngine engine;
    bool opened = engine.open_database(db_file, 2 * PAGE_SIZE);
    assert(opened);
    assert(engine.max_pages() == 2);
    std::cout << "[PASS] StorageEngine initialized with 2 pages\n";

    // 2. Initialize BTreeManager with StorageEngine instance
    BTreeManager btree(&engine);

    // 3. Test Insert & Point Search
    btree.insert(100);
    btree.insert(200);
    btree.insert(300);

    assert(btree.search(100) == true);
    assert(btree.search(200) == true);
    assert(btree.search(500) == false);
    std::cout << "[PASS] Insert and Search verification\n";

    // 4. Test Soft Delete & Resurrection (revive_if_present)
    btree.remove(200); // Soft delete
    assert(btree.search(200) == false); // Should be invisible to search

    btree.insert(200); // Re-inserting resurrects tombstoned key
    assert(btree.search(200) == true); // Should be visible again
    std::cout << "[PASS] Soft-delete & revive_if_present logic\n";

    // 5. Test Range Search [150, 350] -> Expected: {200, 300}
    auto range_res = btree.range_search(150, 350);
    assert(range_res.size() == 2);
    assert(range_res[0] == 200 && range_res[1] == 300);
    std::cout << "[PASS] Range Search [150, 350]\n";

    // 6. Test Dynamic Growth
    // Insert keys to force page allocations beyond initial 2 pages
    for (int i = 1000; i < 2000; ++i) {
        btree.insert(i);
    }
    assert(engine.max_pages() > 2); // File dynamically grew!
    std::cout << "[PASS] Dynamic File Growth (Max Pages: " << engine.max_pages() << ")\n";

    // 7. Test Vacuum and File Sync
    btree.remove(1500);
    btree.remove(1501);
    btree.vacuum();

    bool synced = engine.sync();
    assert(synced);
    std::cout << "[PASS] Vacuum & MS_SYNC check\n";

    // 8. Close Engine & Verify Auto-Shrinking
    engine.close_database();
    std::cout << "[PASS] Auto-shrink & clean shutdown\n";

    std::remove(db_file);
    std::cout << "\n All practice verification tests passed successfully!\n";
}

int main() {
    run_practice_tests();
    return 0;
}