
#pragma once

#include "app/app_registry.hpp"
#include "tohtn_multi_job.hpp"
#include "tohtn_reader.hpp"


void register_mallob_app_tohtn() {
    app_registry::registerApplication("TOHTN",
        // Job reader
        [](const std::vector<std::string>& files, JobDescription& desc) {
            return TohtnReader::read(files, desc);
        },
        // Job creator
        [](const Parameters& params, const Job::JobSetup& setup) -> Job* {
            return new TohtnMultiJob(params, setup);
        },
        // Job solution formatter
        [](const JobResult& result) {
            // An actual application would nicely format the result here ...
            std::string plan_str{};
            for (size_t idx{0}; idx < result.getSolutionSize(); ++idx) {
                plan_str.push_back(static_cast<char>(result.getSolution(idx)));
            }

            auto json = nlohmann::json{};
            json["plan"] = plan_str;
            return json;
        }
    );
}
