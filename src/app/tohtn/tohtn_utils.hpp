//
// Created by khondar on 17.05.22.
//

#ifndef MALLOB_TOHTN_UTILS_HPP
#define MALLOB_TOHTN_UTILS_HPP

#include "data/job_description.hpp"

#include <string>
#include <utility>

std::pair<std::string, std::string> extract_files(const JobDescription &description);

#endif //MALLOB_TOHTN_UTILS_HPP
