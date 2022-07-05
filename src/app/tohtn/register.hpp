
#pragma once

#include "app/app_registry.hpp"
#include "tohtn_multi_job.hpp"
#include "tohtn_reader.hpp"


void register_mallob_app_tohtn() {
    app_registry::registerApplication("DUMMY",
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
            return nlohmann::json();
        }
    );
}
