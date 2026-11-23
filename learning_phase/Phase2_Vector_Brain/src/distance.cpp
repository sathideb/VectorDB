

#include "distance.h"
#include <cmath>
#include <chrono>
#include<vector>
#include<iostream>

using namespace std;

float dot_product(const float* vecA, const float* vecB, int dim){

    float sum=0;
    float mul =1;

    for ( int i=0; i<dim; i++){
        mul= vecA[i]*vecB[i];
        sum+=mul;
        mul=1;
  
    }

    return sum;
}


float l2_distance(const float* vecA, const float* vecB, int dim){

    float l2=0;

    for ( int i=0; i<dim;i++){
        float diff=vecA[i] - vecB[i];
        l2+=(diff*diff);
        
    }

    return sqrt(l2);

}

float cosine_similarity(const float* vecA, const float* vecB, int dim) {
    float dot  = dot_product(vecA, vecB, dim);
    float lenA = std::sqrt(dot_product(vecA, vecA, dim));
    float lenB = std::sqrt(dot_product(vecB, vecB, dim));

    if (lenA == 0.0f || lenB == 0.0f) return 0.0f;

    return dot / (lenA * lenB);
}

