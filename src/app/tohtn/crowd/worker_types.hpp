//
// Created by khondar on 04.08.22.
//

#ifndef CROWDHTN_WORKER_TYPES_HPP
#define CROWDHTN_WORKER_TYPES_HPP

#include <vector>

enum class SearchAlgorithm {
    A_STAR,
    BFS,
    DFS,
    GBFS,
    RANDOM_GBFS,
};

enum class LoopDetectionMode {
    NONE,
    HASHMAP,
    GLOBAL_BLOOM,
    LOCAL_BLOOM,
};

struct InWorkerMessage {
    int tag;
    int source;
    std::vector<int> data;
};

struct OutWorkerMessage {
    int tag;
    int dest;
    std::vector<int> data;
};

struct LoopDetectorMessage {
    size_t version;
    size_t num_nodes;
    // The number of bytes in data which are significant
    size_t num_bytes;
    std::vector<int> data;
};

[[nodiscard]] LoopDetectorMessage combine_messages(const LoopDetectorMessage& lhs, const LoopDetectorMessage& rhs);

#endif //CROWDHTN_WORKER_TYPES_HPP
