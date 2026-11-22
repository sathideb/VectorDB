

#include "recall.h"
#include<algorithm>
#include <unordered_set>


/*
If k is bigger than the number of ground-truth results (say k = 10 but only 4 vectors
exist), flat_search returns only 4. So the limit is min(k, ground_truth.size()):
 1. the loop never reads past the end of the list, and
 2. we divide by 4, not 10, so a perfect search still scores 1.0.

If k == 0 or ground_truth is empty, the limit is 0: return 0.0f to avoid dividing by zero.
*/

float compute_recall_at_k( const std::vector<SearchResult>& ground_truth,
    const std::vector<SearchResult>& predicted, uint32_t k){

    size_t gt_limit = std::min<size_t>(k,ground_truth.size());

    if (gt_limit == 0) return 0.0f;
    std::unordered_set<uint32_t>gt_ids;
    gt_ids.reserve(gt_limit);

    for (size_t i=0; i<gt_limit; ++i){

        gt_ids.insert(ground_truth[i].position);

    }
    size_t pred_limit = std::min<size_t>(k,predicted.size());
    uint32_t matches=0;
     for (size_t i=0; i<pred_limit;++i){

       if (gt_ids.erase(predicted[i].position) > 0){
            ++matches;}

    }

    return static_cast<float>(matches) / static_cast<float>(gt_limit);
}
