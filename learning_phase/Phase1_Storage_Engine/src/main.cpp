#include <iostream>
#include "StorageEngine.h"
#include "BTreeManager.h"

int main() {
    StorageEngine engine;
    if (!engine.open_database("velodb.db")) {
        std::cerr << "could not open database\n";
        return 1;
    }

    BTreeManager btree(&engine);
    for (int k : {10, 20, 30}) btree.insert(k);

    std::cout << "search(20): " << btree.search(20) << "\n";   // 1 = found
    std::cout << "search(99): " << btree.search(99) << "\n";   // 0 = not found

    engine.close_database();
    return 0;
}