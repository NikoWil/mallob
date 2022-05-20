//
// Created by khondar on 17.05.22.
//

#include <fstream>
#include "tohtn_simple_job.hpp"

#include "tohtn_utils.hpp"
#include "app/tohtn/lilotane/util/log.h"

void TohtnSimpleJob::appl_start() {
    {
        // Create filenames for domain and problem, unique to each worker
        // Uniqueness of filename is important in the case where the workers run on the same machine

        std::stringstream id_stream;
        id_stream << "job-" << std::to_string(getDescription().getId()) << "_";
        id_stream << "rank-" << std::to_string(getDescription().getClientRank());

        std::stringstream domain_file_name;
        domain_file_name << "domain_";
        domain_file_name << id_stream.str();
        domain_file_name << ".hddl";
        _domain_file_name = domain_file_name.str();

        std::stringstream problem_file_name;
        problem_file_name << "problem_";
        problem_file_name << id_stream.str();
        problem_file_name << ".hddl";
        _problem_file_name = problem_file_name.str();
    }

    const auto[domain, problem] = extract_files(getDescription());

    std::ofstream domain_file{_domain_file_name};
    domain_file << domain;
    domain_file.close();

    std::ofstream problem_file{_problem_file_name};
    problem_file << problem;
    problem_file.close();


    _htn = get_htn_instance(_domain_file_name, _problem_file_name);
    _worker = create_crowd_worker(_htn);

    // TODO: create thread
    _work_thread = std::thread{[this]() {
        while (true) {
            // The three sections are in different blocks to make it clear to the compiler that mutexes can be released
            // again

            // acquire mutex for working & wait flag
            // Suspend via condition variable, if it's needed
            {
                std::unique_lock suspend_lock{_suspend_mutex};
                if (_suspend_flag) {
                    // re-check _suspend_flag to deal with spurious wakeups
                    _suspend_cvar.wait(suspend_lock, [this]() { return !_suspend_flag; });
                }
            }

            {
                std::unique_lock worker_lock{_worker_mutex};
                switch (_worker->plan_step()) {
                    case WorkerPlanState::PLANNING:
                        break;
                    case WorkerPlanState::OUT_OF_WORK:
                        return;
                    case WorkerPlanState::PLAN:
                        return;
                };
            }

            // check for termination
            {
                std::unique_lock terminate_lock{_terminate_mutex};
                if (_termiante_flag) {
                    std::unique_lock has_terminated_lock{_has_terminated_mutex};
                    _has_terminated = true;
                    return;
                }
            }
        }
    }};
}

void TohtnSimpleJob::appl_suspend() {
    {
        // don't suspend if the thread is already busy terminating!
        std::unique_lock terminate_lock{_terminate_mutex};
        if (_termiante_flag) {
            return;
        }
    }
    {
        // suspend thread
        std::unique_lock suspend_lock{_suspend_mutex};
        _suspend_flag = true;
    }
}

void TohtnSimpleJob::appl_resume() {
    // resume thread
    {
        std::unique_lock suspend_lock{_suspend_mutex};
        _suspend_flag = false;
    }
    // Cppreference says that holding the lock while calling notify_all is a pessimization
    // https://en.cppreference.com/w/cpp/thread/condition_variable/notify_all
    _suspend_cvar.notify_all();
}

void TohtnSimpleJob::appl_terminate() {
    // Separate flag such that the lock gets destroyed, the _worker_thread can actually read the _terminate_flag and
    // then terminate accordingly
    {
        std::unique_lock terminate_lock{_terminate_mutex};
        _termiante_flag = true;
    }
    appl_resume();
}

int TohtnSimpleJob::appl_solved() {
    std::unique_lock worker_lock{_worker_mutex};
    return _worker->has_plan();
}

JobResult &&TohtnSimpleJob::appl_getResult() {
    // TODO
    std::unique_lock worker_lock{_worker_mutex};
    auto plan_opt{_worker->get_plan_string()};
    if (plan_opt.has_value()) {
        // Round up the number of chars in the string to a multiple of sizeof(int)
        // This is done to avoid confusion by the JobResult class which might copy between the uint8_t and the int
        // representation at will
        const auto round_up = [](size_t val, size_t base) {
            if (val % base == 0) {
                return val;
            } else {
                return val + base - (val % base);
            }
        };

        std::string plan_string{std::move(plan_opt.value())};
        // make space for the null terminator
        const size_t nulled_string_size{plan_string.size() + 1};
        const size_t rounded_string_size{round_up(nulled_string_size, sizeof(int))};

        std::vector<uint8_t> plan_bytes(rounded_string_size);
        memcpy(plan_bytes.data(), plan_string.data(), plan_string.length());
        // Set null terminator just behing the plan string encoding
        // We do not care about the values of any bytes beyond that, they are just there to avoid invalid accesses
        plan_bytes.at(plan_string.length()) = '\0';
        _result = JobResult{std::move(plan_bytes)};
    } else {
        // TODO: what if we are called while no plan exists? Crash the world?
        Log::e("Worker %d asked for plan while none exists!\n", getId());
    }

    return std::move(_result);
}

bool TohtnSimpleJob::appl_wantsToBeginCommunication() {
    return false;
}

void TohtnSimpleJob::appl_beginCommunication() {
    // We do not communicate, ever
}

void TohtnSimpleJob::appl_communicate(int source, JobMessage &msg) {
    // We do not expect any messages, ever
}

void TohtnSimpleJob::appl_dumpStats() {
    // TODO: do we actually want to dump something?
    // Stats to dump:
    // - # elements in the bloom filter (local vs global!)
    // - # hits for the bloom filter
    // - current iteration (especially with bloom filter restarts)
    // - current fringe size
}

int TohtnSimpleJob::getDemand() const {
    // TODO: is this correct?
    return 1;
}

bool TohtnSimpleJob::appl_isDestructible() {
    // A thread is safe to destruct if it is no longer running
    // A thread which has terminated is no longer joinable (hooo-ray!)
    std::unique_lock has_terminated_lock{_has_terminated_mutex};
    return _work_thread.joinable() && _has_terminated;
}

TohtnSimpleJob::~TohtnSimpleJob() noexcept {
    _work_thread.join();
}