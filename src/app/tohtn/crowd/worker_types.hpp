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

[[nodiscard]] std::vector<int> combine_encoded_messages(std::vector<int> &lhs, std::vector<int> &rhs);

[[nodiscard]] std::vector<int> empty_loopdetector_message();

#endif //CROWDHTN_WORKER_TYPES_HPP
