//
// Created by khondar on 14.02.22.
//

#include "tohtn_reader.hpp"

#include "util/logger.hpp"
#include "util/sys/process.hpp"

#include <fstream>
#include <random>

std::size_t read_file(const std::string &file_name, JobDescription &desc) {
    std::ifstream input{file_name};

    std::size_t num_chars{0};
    for (std::string line; std::getline(input, line);) {
        for (const char c: line) {
            desc.addPermanentData(static_cast<int>(c));
            ++num_chars;
        }
        desc.addPermanentData(static_cast<int>('\n'));
        ++num_chars;
    }

    return num_chars;
}

bool TohtnReader::read(const std::vector<std::string> &files, JobDescription &desc) {
    if (files.size() != 2) {
        LOG(V0_CRIT, "Invalid number of input files for TOHTN planning. Needs exactly 2 inputs <domain> <problem>\n");
        return false;
    }

    LOG(V2_INFO, "TohtnReader::read, domain file: %s, problem file: %s\n", files[0].data(), files[1].data());

    desc.beginInitialization(desc.getRevision());

    const auto &domain_file_name{files[0]};
    const auto &problem_file_name{files[1]};

    const std::size_t num_domain_chars{read_file(domain_file_name, desc)};
    const std::size_t num_problem_chars{read_file(problem_file_name, desc)};

    if (num_domain_chars > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        num_problem_chars > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        LOG(V0_CRIT, "Either domain or problem file too large - more characters than int max");
        Process::doExit(1);
    }

    desc.addPermanentData(static_cast<int>(num_domain_chars));
    desc.addPermanentData(static_cast<int>(num_problem_chars));

    const size_t seed{std::random_device{}()};
    static_assert(sizeof(size_t) % sizeof(int) == 0);
    constexpr size_t ints_in_size_t{sizeof(size_t) / sizeof(int)};
    std::array<int, ints_in_size_t> seed_as_ints{};
    memcpy(seed_as_ints.data(), &seed, sizeof(size_t));
    for (const auto& item : seed_as_ints) {
        desc.addPermanentData(item);
    }
    TODO
    //std::for_each(seed_as_ints.crbegin(), seed_as_ints.crend(), [&desc](int item) { desc.addPermanentData(item); });

    desc.endInitialization();
    return true;
}