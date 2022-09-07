//
// Created by khondar on 17.05.22.
//

#ifndef MALLOB_TOHTN_UTILS_HPP
#define MALLOB_TOHTN_UTILS_HPP

#include "data/job_description.hpp"

#include <string>
#include <utility>

struct TohtnJobInfo {
    std::array<size_t, 4> seed;
    std::string domain;
    std::string problem;
};

TohtnJobInfo extract_files(const JobDescription &description);

#endif //MALLOB_TOHTN_UTILS_HPP
