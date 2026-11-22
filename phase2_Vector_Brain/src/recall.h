
#pragma once

#include "flat_search.h"
#include <vector>
#include <cstdint>


float compute_recall_at_k( const std::vector<SearchResult>& ground_truth,
    const std::vector<SearchResult>& predicted, uint32_t k);


/*
ground_truth is top-k list retuned by flat_search
and predicted is the top-k list returned by algorithm that we are testing , and k is top no of top scores(matching score)


recall small mental model :
ground_truth: [12, 45, 7, 89, 3]
predicted: [12, 7, 99, 45, 102]
compute_recall_at_k returns 3/5=0.6 because out of 5 3 scores have matched.
*/