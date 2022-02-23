//
// Created by khondar on 14.02.22.
//

#ifndef MALLOB_TOHTN_READER_HPP
#define MALLOB_TOHTN_READER_HPP

#include "data/job_description.hpp"

#include <string>
#include <vector>

namespace TohtnReader {
    [[nodiscard]] bool read(const std::vector<std::string>& files, JobDescription& desc);
}

#endif //MALLOB_TOHTN_READER_HPP
