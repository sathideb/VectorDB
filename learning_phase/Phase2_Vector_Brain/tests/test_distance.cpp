


#include "distance.h"
#include <gtest/gtest.h>

//TEST(suitename,testname)
TEST( Distance, DotproductBasic){

    float a1[3]={1,2,3};
    float b1[3]={4,5,6};
    int dim=3;
    
    float result=dot_product(a1,b1,dim);
    EXPECT_FLOAT_EQ(32,result);

}

TEST( Distance, L2_distance_basic){

    float a1[2]={1,2};
    float b1[2]={4,6};

    float result=l2_distance(a1,b1,2);
    EXPECT_NEAR(5,result,1e-5);

}

TEST(Distance, L2_distance_irrational) {

    float a1[2] = {1, 2};
    float b1[2] = {2, 3};

    float result = l2_distance(a1, b1, 2);
    EXPECT_NEAR(1.41421, result, 1e-5);
}

TEST(Distance, CosineSameDirectionDifferentSize) {
    float a[3] = {1, 2, 3};
    float b[3] = {2, 4, 6};
    EXPECT_NEAR(1.0f, cosine_similarity(a, b, 3), 1e-5f);
}

TEST(Distance, CosinePerpendicular) {
    float a[2] = {1, 0};
    float b[2] = {0, 1};
    EXPECT_NEAR(0.0f, cosine_similarity(a, b, 2), 1e-5f);
}

TEST(Distance, CosineOpposite) {
    float a[2] = {1, 1};
    float b[2] = {-1, -1};
    EXPECT_NEAR(-1.0f, cosine_similarity(a, b, 2), 1e-5f);
}

TEST(Distance, CosineNoticeExample) {
    float q[3]  = {1, 1, 0};
    float d2[3] = {9, 0, 5};
    EXPECT_NEAR(0.61812f, cosine_similarity(q, d2, 3), 1e-4f);
}

TEST(Distance, CosineIdentical) {
    float q[3] = {1, 1, 0};
    EXPECT_NEAR(1.0f, cosine_similarity(q, q, 3), 1e-5f);
}

TEST(Distance, CosineZeroVector) {
    float z[2] = {0, 0};
    float a[2] = {1, 2};
    EXPECT_FLOAT_EQ(0.0f, cosine_similarity(z, a, 2));
}

TEST(Distance, TenThousandDistancesUnder10ms) {
    const int dim = 384;
    std::vector<float> a(dim, 0.5f);
    std::vector<float> b(dim, 0.25f);

    float total = 0.0f;

    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 10000; i++) {
        total += dot_product(a.data(), b.data(), dim);
    }
    auto end = std::chrono::steady_clock::now();

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "10,000 distances took " << ms << " ms\n";

    EXPECT_GT(total, 0.0f);   // uses the result so the compiler can't delete the loop
    EXPECT_LT(ms, 10);
}