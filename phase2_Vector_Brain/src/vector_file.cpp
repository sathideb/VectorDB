#include "vector_file.h"
#include <cstdint>
#include <fstream>
#include <stdexcept>

LoadedVectors load_vectors(const std::string& path) {

    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("can't open " + path);

    uint32_t dim = 0;
    in.read(reinterpret_cast<char*>(&dim), sizeof(dim));

    uint32_t count = 0;
    in.read(reinterpret_cast<char*>(&count), sizeof(count));

    uint32_t dtype = 0;
    in.read(reinterpret_cast<char*>(&dtype), sizeof(dtype));

    if (!in) throw std::runtime_error("truncated header in " + path);
    if (dtype != 0) throw std::runtime_error("unsupported dtype in " + path);

    LoadedVectors result;
    result.dim = dim;
    result.count = count;

    in.seekg(4096);

    size_t total=static_cast<size_t>(count)*dim;
    result.data.resize(total);

    in.read(reinterpret_cast<char*>(result.data.data()), total * sizeof(float));
    if (!in) throw std::runtime_error("truncated data in " + path);
    return result;
}
