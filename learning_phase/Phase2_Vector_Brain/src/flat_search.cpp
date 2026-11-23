

#include "flat_search.h"
#include "distance.h"
#include <queue>
#include <algorithm>

struct WorseScoreOnTop {
    bool operator()(const SearchResult& a, const SearchResult& b) const {
        return a.score > b.score;
    }
};

std::vector<SearchResult> flat_search(const float* data, const float* query,
                                      int dim, uint32_t count, uint32_t k) {

    std::priority_queue<SearchResult, std::vector<SearchResult>, WorseScoreOnTop> heap;

    for (uint32_t i=0; i<count ; i++){

        const float* ptr= data + static_cast<size_t>(i) * dim;
        float score=cosine_similarity(query, ptr, dim);

        heap.push(SearchResult{score, i});

        if (heap.size() >k ){
            heap.pop();

        }


    }

    std::vector<SearchResult> results;

    while (!heap.empty()){
        results.push_back(heap.top());
        heap.pop();

    }
    std::reverse(results.begin(), results.end());
    return results;
}