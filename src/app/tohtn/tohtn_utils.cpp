//
// Created by khondar on 17.05.22.
//

#include "tohtn_utils.hpp"
#include "util/logger.hpp"

TohtnJobInfo extract_files(const JobDescription &description) {

    const int revision{description.getRevision()};
    const std::size_t payload_size{description.getFormulaPayloadSize(revision)};
    const int *payload{description.getFormulaPayload(revision)};

    // extract seed
    static_assert(sizeof(size_t) % sizeof(int) == 0);
    constexpr size_t ints_in_size_t{sizeof(size_t) / sizeof(int)};
    std::array<size_t, 4> seeds{};
    memcpy(seeds.data(), payload + payload_size - 4 * ints_in_size_t, 4 * sizeof(size_t));

    const int num_domain_chars{payload[payload_size - 2 - (4 * ints_in_size_t)]};
    const int num_problem_chars{payload[payload_size - 1 - (4 * ints_in_size_t)]};

    const int *domain_payload{payload};
    const int *problem_payload{payload + num_domain_chars};

    std::string domain;
    std::string problem;
    for (int i{0}; i < num_domain_chars; ++i) {
        domain.push_back(static_cast<char>(domain_payload[i]));
    }
    for (int i{0}; i < num_problem_chars; ++i) {
        problem.push_back(static_cast<char>(problem_payload[i]));
    }

    return {seeds, domain, problem};
}