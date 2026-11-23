

#pragma once 
#include<cstdint>
#include <vector>

struct SearchResult{

    float score; // holds the score 
    uint32_t position; // holds it’s the vector’s index (0-based)

};

std::vector<SearchResult> flat_search(const float* data, const float* query, int dim, uint32_t count, uint32_t k);





/*

std::vector<SearchResult> is the return value: a list of slips.
flat_search is the function name.
const float* data points to the big block of all vectors, which is read-only.
const float* query points to the query vector’s 384 floats.
int dim is the number of floats per vector (384). It’s an int to match distance.h.
uint32_t count is the number of vectors in the block (1000).
uint32_t k is how many results to return (3).

*/
