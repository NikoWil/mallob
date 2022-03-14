//
// Created by khondar on 14.02.22.
//

#include <iostream>
#include <fstream>
#include "tohtn_job.hpp"
#include "app/tohtn/lilotane/data/htn_instance.h"

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

void TohtnJob::appl_start() {
    const auto [domain, problem] = extract_files(getDescription());

    std::stringstream id_stream;
    id_stream << "job-" << std::to_string(getDescription().getId()) << "_";
    id_stream << "rank-" << std::to_string(getDescription().getClientRank());

    std::stringstream domain_file_name;
    domain_file_name << "domain_";
    domain_file_name << id_stream.str();
    domain_file_name << ".hddl";

    std::stringstream problem_file_name;
    problem_file_name << "problem_";
    problem_file_name << id_stream.str();
    problem_file_name << ".hddl";

    std::ofstream domain_file{domain_file_name.str()};
    domain_file << domain;
    domain_file.close();

    std::ofstream problem_file{problem_file_name.str()};
    problem_file << problem;
    problem_file.close();

    _htn = std::make_shared<HtnInstance>(HtnInstance{domain_file_name.str(), problem_file_name.str()});


}

// TODO: make the planner thread fall asleep and wait for some condition variable
void TohtnJob::appl_suspend() {

}

// TODO: signal the planner thread to resume on the condition variable
void TohtnJob::appl_resume() {

}

// TODO: kill everything? (ze thread)
void TohtnJob::appl_terminate() {

}

// TODO: ask planner whether done
int TohtnJob::appl_solved() {
    return -1;
}

// TODO: somehow encode a plan as a SAT solution?!
JobResult &&TohtnJob::appl_getResult() {
    return std::move(_result);
}

// TODO:
bool TohtnJob::appl_wantsToBeginCommunication() {
    if (_send_buf._mutex.try_lock()) {
        // The main thread is the only one to ever shrink the underlying vector, as a result we can rely on this being okay
        const auto wants_comm{!_send_buf._vec.empty()};
        _send_buf._mutex.unlock();
        return wants_comm;
    } else {
        return false;
    }
}

// TODO
void TohtnJob::appl_beginCommunication() {
    if (!_send_buf._mutex.try_lock()) {
        return;
    }

    for (auto& msg : _send_buf._vec) {
        MyMpi::isend(msg._dest, msg._mpi_tag, std::move(msg._payload));
    }

    _send_buf._mutex.unlock();
}

// TODO: write message into the message received queue of the thread, always called when a message exists
void TohtnJob::appl_communicate(int source, JobMessage &msg) {
    if (_recv_buf._mutex.try_lock()) {
        _recv_buf._vec.push_back(msg);

        _recv_buf._mutex.unlock();
    }
}

// TODO: dump what?
void TohtnJob::appl_dumpStats() {

}

// TODO: Find out what this is for
bool TohtnJob::appl_isDestructible() {
    return true;
}
