

#pragma once
#include<cstdint>
#include<string>
#include<vector>

struct LoadedVectors{

    int dim;
    uint32_t count;
    std::vector<float>data;
};

LoadedVectors load_vectors(const std::string& path);