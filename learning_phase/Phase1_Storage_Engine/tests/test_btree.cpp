
//
// Build target: btree_tests (links GTest::gtest_main, so there is NO main() here).
//
// How to read this file:
//   * Every TEST_F is one scenario: ARRANGE (fixture does it) -> ACT -> ASSERT.
//   * EXPECT_* records a failure and keeps going. ASSERT_* stops the test.
//   * The fixture gives every test its OWN fresh database file, so tests can't
//     poison each other.
#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <functional>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "BTreeManager.h"
#include "StorageEngine.h"

namespace {

// How many pages the db file starts with. Starting small makes the file GROW
// during the test. If a test only fails with 2 here but passes with 4000, the
// bug is in how the file grows (remap), not in the B-Tree locking.
constexpr size_t kInitialPages = 2;

// Runs fn(thread_id) on n threads at once and waits for all of them.
void run_threads(int n, const std::function<void(int)>& fn) {
    std::vector<std::thread> threads;
    threads.reserve(n);
    for (int t = 0; t < n; ++t) threads.emplace_back(fn, t);
    for (auto& th : threads) th.join();
}

// ARRANGE step shared by every test: a fresh db file, an engine, a tree.
class BTreeTest : public ::testing::Test {
protected:
    void SetUp() override {
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        db_file_ = std::string("velodb_") + info->test_suite_name() + "_" + info->name() + ".db";
        std::remove(db_file_.c_str());                       // clean slate
        ASSERT_TRUE(engine_.open_database(db_file_.c_str(), kInitialPages * PAGE_SIZE));
        btree_ = std::make_unique<BTreeManager>(&engine_);
    }
    void TearDown() override {
        btree_.reset();                                      // tree first, then engine
        engine_.close_database();
        std::remove(db_file_.c_str());
    }

    StorageEngine engine_;
    std::unique_ptr<BTreeManager> btree_;
    std::string db_file_;
};

// 
//  PART 1: single thread. If these fail, don't even look at Part 2.

TEST_F(BTreeTest, InsertThenSearchFindsKeys) {
    btree_->insert(10);
    btree_->insert(20);
    btree_->insert(30);

    EXPECT_TRUE(btree_->search(10));
    EXPECT_TRUE(btree_->search(20));
    EXPECT_TRUE(btree_->search(30));
    EXPECT_FALSE(btree_->search(99));
}

TEST_F(BTreeTest, SearchOnEmptyTreeIsFalse) {
    EXPECT_FALSE(btree_->search(1));
    EXPECT_TRUE(btree_->range_search(0, 100).empty());
}

TEST_F(BTreeTest, ManyKeysForceSplitsAndStayFindable) {
    std::vector<int> keys(5000);
    for (int i = 0; i < 5000; ++i) keys[i] = i;
    std::mt19937 rng(42);
    std::shuffle(keys.begin(), keys.end(), rng);             // random order = many different split shapes

    for (int k : keys) btree_->insert(k);

    for (int k = 0; k < 5000; ++k) EXPECT_TRUE(btree_->search(k)) << "missing key " << k;
    EXPECT_FALSE(btree_->search(5000));
    EXPECT_FALSE(btree_->search(-1));

    auto r = btree_->range_search(100, 199);
    EXPECT_EQ(r.size(), 100u);
    EXPECT_TRUE(std::is_sorted(r.begin(), r.end()));
}

TEST_F(BTreeTest, DuplicateInsertDoesNotCreateSecondCopy) {
    for (int round = 0; round < 3; ++round)
        for (int k = 0; k < 200; ++k) btree_->insert(k);

    auto all = btree_->range_search(0, 1000);
    EXPECT_EQ(all.size(), 200u);                             // still exactly 200
}

TEST_F(BTreeTest, RemoveReviveAndVacuum) {
    for (int k = 0; k < 1000; ++k) btree_->insert(k);

    for (int k = 0; k < 1000; k += 2) EXPECT_TRUE(btree_->remove(k));   // true = it was live
    EXPECT_FALSE(btree_->remove(0));                          // already deleted -> false

    for (int k = 0; k < 1000; ++k) EXPECT_EQ(btree_->search(k), k % 2 == 1) << "key " << k;

    btree_->insert(4);                                        // revive a tombstoned key
    EXPECT_TRUE(btree_->search(4));

    btree_->vacuum();                                         // must not lose any LIVE key
    for (int k = 0; k < 1000; ++k) {
        bool expected_live = (k % 2 == 1) || k == 4;
        EXPECT_EQ(btree_->search(k), expected_live) << "after vacuum, key " << k;
    }
}


//  PART 2: many threads. The same checks, but threads race each other.
// Your original test: each thread owns a contiguous range of keys.
TEST_F(BTreeTest, TenThreadsInsertDisjointRanges) {
    const int num_threads = 10;
    const int keys_per_thread = 100;

    run_threads(num_threads, [&](int t) {
        for (int k = t * keys_per_thread; k < (t + 1) * keys_per_thread; ++k) {
            btree_->insert(k);
            EXPECT_TRUE(btree_->search(k));                  // read-your-own-write
        }
    });

    for (int k = 0; k < num_threads * keys_per_thread; ++k)
        EXPECT_TRUE(btree_->search(k)) << "lost key " << k;
}

// Harder: thread t inserts t, t+T, t+2T ... so EVERY thread keeps hitting the
// SAME leaves and the same splits. This is where lost updates would show up.
TEST_F(BTreeTest, InterleavedKeysFightOverSameLeaves) {
    const int num_threads = 8;
    const int keys_per_thread = 2000;

    run_threads(num_threads, [&](int t) {
        for (int i = 0; i < keys_per_thread; ++i) btree_->insert(i * num_threads + t);
    });

    auto all = btree_->range_search(0, num_threads * keys_per_thread);
    ASSERT_EQ(all.size(), static_cast<size_t>(num_threads * keys_per_thread));   // none lost, none doubled
    EXPECT_TRUE(std::is_sorted(all.begin(), all.end()));
    for (int k = 0; k < num_threads * keys_per_thread; ++k)
        ASSERT_TRUE(btree_->search(k)) << "lost key " << k;
}

// Writers keep inserting NEW keys while readers look up OLD keys. An old key
// must be found EVERY time, even in the middle of someone else's split.
TEST_F(BTreeTest, ReadersAlwaysSeeOldKeysWhileWritersSplit) {
    const int old_keys = 1000;
    for (int k = 0; k < old_keys; ++k) btree_->insert(k);

    std::atomic<bool> writers_done{false};
    std::atomic<int> misses{0};
    std::atomic<int> unsorted_ranges{0};

    std::vector<std::thread> readers;
    for (int r = 0; r < 4; ++r) {
        readers.emplace_back([&, r] {
            std::mt19937 rng(100 + r);
            while (!writers_done) {
                if (!btree_->search(static_cast<int>(rng() % old_keys))) ++misses;
                if (rng() % 50 == 0) {
                    auto v = btree_->range_search(0, 500);
                    if (!std::is_sorted(v.begin(), v.end())) ++unsorted_ranges;
                }
            }
        });
    }

    run_threads(4, [&](int t) {                              // writers: new keys only (>= 1000)
        for (int i = 0; i < 2000; ++i) btree_->insert(old_keys + i * 4 + t);
    });
    writers_done = true;
    for (auto& th : readers) th.join();

    EXPECT_EQ(misses.load(), 0) << "a reader missed a key that was always present";
    EXPECT_EQ(unsorted_ranges.load(), 0);
    for (int k = 0; k < old_keys + 8000; ++k) ASSERT_TRUE(btree_->search(k)) << "lost key " << k;
}

// Everything at once on overlapping keys. We can't predict WHICH keys end up
// alive, so we only check invariants: sorted, no duplicates, and every key
// that range_search returns is also found by search.
TEST_F(BTreeTest, MixedInsertRemoveSearchVacuumKeepsInvariants) {
    run_threads(6, [&](int t) {
        std::mt19937 rng(7 + t);
        for (int i = 0; i < 4000; ++i) {
            int k = static_cast<int>(rng() % 3000);
            switch (rng() % 10) {
                case 0: case 1: case 2: case 3: case 4: btree_->insert(k); break;
                case 5: case 6:                         btree_->remove(k); break;
                case 7: case 8:                         btree_->search(k); break;
                default:
                    if (t == 0 && i % 200 == 0) btree_->vacuum();
                    else                         btree_->range_search(k, k + 50);
            }
        }
    });

    auto live = btree_->range_search(0, 100000);
    EXPECT_TRUE(std::is_sorted(live.begin(), live.end()));
    EXPECT_EQ(std::adjacent_find(live.begin(), live.end()), live.end()) << "duplicate key in tree";
    for (int k : live) EXPECT_TRUE(btree_->search(k)) << "range_search returned " << k << " but search can't find it";
}

}  // namespace