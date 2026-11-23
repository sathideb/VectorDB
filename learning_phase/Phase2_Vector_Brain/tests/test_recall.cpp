#include <gtest/gtest.h>
#include "recall.h"

TEST(RecallTest, ExactMatch) {
    std::vector<SearchResult> gt = {
        {0.95f, 12}, {0.91f, 45}, {0.88f, 7}, {0.85f, 89}, {0.80f, 3}
    };
    std::vector<SearchResult> pred = {
        {0.95f, 12}, {0.91f, 45}, {0.88f, 7}, {0.85f, 89}, {0.80f, 3}
    };

    EXPECT_FLOAT_EQ(compute_recall_at_k(gt, pred, 5), 1.0f);
}

TEST(RecallTest, OrderIndependent) {
    std::vector<SearchResult> gt = {
        {0.95f, 12}, {0.91f, 45}, {0.88f, 7}, {0.85f, 89}, {0.80f, 3}
    };
    std::vector<SearchResult> pred = {
        {0.80f, 3}, {0.85f, 89}, {0.88f, 7}, {0.91f, 45}, {0.95f, 12}
    };

    EXPECT_FLOAT_EQ(compute_recall_at_k(gt, pred, 5), 1.0f);
}

TEST(RecallTest, PartialOverlap) {
    std::vector<SearchResult> gt = {
        {0.95f, 12}, {0.91f, 45}, {0.88f, 7}, {0.85f, 89}, {0.80f, 3}
    };
    std::vector<SearchResult> pred = {
        {0.95f, 12}, {0.88f, 7}, {0.84f, 99}, {0.91f, 45}, {0.79f, 102}
    };

    EXPECT_NEAR(compute_recall_at_k(gt, pred, 5), 0.6f, 1e-6f);
}

TEST(RecallTest, DuplicateInPredictedDoesNotInflate) {
    std::vector<SearchResult> gt = {
        {0.95f, 12}, {0.91f, 45}, {0.88f, 7}
    };
    std::vector<SearchResult> pred = {
        {0.95f, 12}, {0.95f, 12}, {0.95f, 12}
    };

    EXPECT_NEAR(compute_recall_at_k(gt, pred, 3), 1.0f / 3.0f, 1e-6f);
}

TEST(RecallTest, EdgeCasesEmptyAndKBiggerThanSize) {
    std::vector<SearchResult> empty;
    std::vector<SearchResult> small = {{0.9f, 1}, {0.8f, 2}};

    EXPECT_FLOAT_EQ(compute_recall_at_k(empty, small, 5), 0.0f);
    EXPECT_FLOAT_EQ(compute_recall_at_k(small, small, 0), 0.0f);
    EXPECT_FLOAT_EQ(compute_recall_at_k(small, small, 10), 1.0f);
}