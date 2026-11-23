#include <gtest/gtest.h>
#include "flat_search.h"
#include "vector_file.h"

TEST(FlatSearch, TopTwo) {
    float data[8]  = {1,0,  0,1,  1,1,  -1,0};
    float query[2] = {1, 0};

    std::vector<SearchResult> results = flat_search(data, query, 2, 4, 2);

    EXPECT_EQ(results.size(), 2u);

    EXPECT_EQ(results[0].position, 0u);
    EXPECT_NEAR(results[0].score, 1.0f, 1e-5f);

    EXPECT_EQ(results[1].position, 2u);
    EXPECT_NEAR(results[1].score, 0.70711f, 1e-4f);
}

TEST(FlatSearch, KBiggerThanCount) {
    float data[8]  = {1,0,  0,1,  1,1,  -1,0};
    float query[2] = {1, 0};

    std::vector<SearchResult> results = flat_search(data, query, 2, 4, 10);

    EXPECT_EQ(results.size(), 4u);

    EXPECT_EQ(results[0].position, 0u);
    EXPECT_EQ(results[1].position, 2u);
    EXPECT_EQ(results[2].position, 1u);
    EXPECT_EQ(results[3].position, 3u);
}

TEST(FlatSearch, OnlyTheBest) {
    float data[8]  = {1,0,  0,1,  1,1,  -1,0};
    float query[2] = {1, 0};

    std::vector<SearchResult> results = flat_search(data, query, 2, 4, 1);

    EXPECT_EQ(results.size(), 1u);

    EXPECT_EQ(results[0].position, 0u);
}
TEST(VectorFile, HeaderAndSize) {
    LoadedVectors v = load_vectors("data/vectors.dat");

    EXPECT_EQ(v.dim, 384);
    EXPECT_EQ(v.count, 1000u);
    EXPECT_EQ(v.data.size(), 384000u);
}

TEST(VectorFile, FirstValuesMatchPython) {
    LoadedVectors v = load_vectors("data/vectors.dat");

    EXPECT_NEAR(v.data[0], -0.05485826f, 1e-6f);
    EXPECT_NEAR(v.data[1], -0.04781568f, 1e-6f);
    EXPECT_NEAR(v.data[2], -0.00951484f, 1e-6f);
    EXPECT_NEAR(v.data[3], -0.04020191f, 1e-6f);
    EXPECT_NEAR(v.data[4],  0.0146623f,  1e-6f);
}

TEST(VectorFile, StoredVectorFindsItself) {
    LoadedVectors v = load_vectors("data/vectors.dat");

    const float* query = v.data.data() + 7 * v.dim;
    std::vector<SearchResult> results =
        flat_search(v.data.data(), query, v.dim, v.count, 3);

    ASSERT_EQ(results.size(), 3u);
    EXPECT_EQ(results[0].position, 7u);
    EXPECT_NEAR(results[0].score, 1.0f, 1e-4f);
}