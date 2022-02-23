//
// Created by khondar on 14.02.22.
//

#include <iostream>
#include "tohtn_job.hpp"

std::pair<std::string, std::string> extract_files(const JobDescription& description) {
    const int revision{description.getRevision()};
    const std::size_t payload_size{description.getFormulaPayloadSize(revision)};
    const int* payload{description.getFormulaPayload(revision)};
    const int num_domain_chars{payload[payload_size - 2]};
    const int num_problem_chars{payload[payload_size - 1]};

    const int* domain_payload{payload};
    const int* problem_payload{payload + num_domain_chars};

    std::string domain;
    std::string problem;
    for (int i{0}; i < num_domain_chars; ++i) {
        domain.push_back(static_cast<char>(domain_payload[i]));
    }
    for (int i{0}; i < num_problem_chars; ++i) {
        problem.push_back(static_cast<char>(problem_payload[i]));
    }

    LOG(V2_INFO, "problem init:\n%s\n", problem.c_str());

    return {domain, problem};
}

TohtnJob::TohtnJob(const Parameters &params, int commSize, int worldRank, int jobId)
    : Job(params, commSize, worldRank, jobId,JobDescription::Application::TOHTN) {}

// TODO:
// - parse domain & problem
// - start planner
void TohtnJob::appl_start() {
    const auto [domain, problem] = extract_files(getDescription());

    LOG(V2_INFO, "domain:\n%s\n\n", domain.c_str());
    LOG(V2_INFO, "problem:\n%s\n", problem.c_str());
}

// TODO
void TohtnJob::appl_suspend() {

}

// TODO
void TohtnJob::appl_resume() {

}

// TODO
void TohtnJob::appl_terminate() {

}

// TODO
int TohtnJob::appl_solved() {
    return -1;
}

// TODO
JobResult &&TohtnJob::appl_getResult() {
    return std::move(_result);
}

// TODO
bool TohtnJob::appl_wantsToBeginCommunication() {
    return false;
}

// TODO
void TohtnJob::appl_beginCommunication() {

}

// TODO
void TohtnJob::appl_communicate(int source, JobMessage &msg) {

}

// TODO
void TohtnJob::appl_dumpStats() {

}

// TODO
bool TohtnJob::appl_isDestructible() {
    return true;
}
