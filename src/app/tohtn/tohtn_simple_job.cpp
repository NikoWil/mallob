//
// Created by khondar on 17.05.22.
//

#include <fstream>
#include "tohtn_simple_job.hpp"

#include "util/logger.hpp"

#include "tohtn_utils.hpp"

TohtnSimpleJob::TohtnSimpleJob(const Parameters &params, const JobSetup &setup) : Job(params, setup) {}

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

    const auto[seed, domain, problem] = extract_files(getDescription());

    std::ofstream domain_file{_domain_file_name};
    domain_file << domain;
    domain_file.close();

    std::ofstream problem_file{_problem_file_name};
    problem_file << problem;
    problem_file.close();

    _htn = get_htn_instance(_domain_file_name, _problem_file_name);
    LOG(V2_INFO, "HtnInstance created\n");

    _worker = create_crowd_worker(_htn, SearchAlgorithm::DFS, LoopDetectionMode::NONE, seed);
    LOG(V2_INFO, "Worker created\n");

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
    LOG(V2_INFO, "Work Thread started\n");
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
    // only a resumed job will actually see the _terminate_flag and act accordingly
    appl_resume();
}

int TohtnSimpleJob::appl_solved() {
    std::unique_lock worker_lock{_worker_mutex};
    if (_worker) {
        LOG(V2_INFO, "Worker %d asked whether plan exists, reports _worker->has_plan = %s\n", this->getId(),
            _worker->has_plan() ? "true" : "false");
        return _worker->has_plan() ? 1 : -1;
    } else {
        LOG(V2_INFO, "Worker %d asked whether plan exists, _worker is nullptr\n", this->getId());
        return -1;
    }
}

JobResult &&TohtnSimpleJob::appl_getResult() {
    LOG(V2_INFO, "TohtnSimpleJob::appl_getResult()\n");
    std::optional<std::string> plan_opt;
    {
        std::unique_lock worker_lock{_worker_mutex};
        if (_worker) {
            LOG(V2_INFO, "_worker->get_plan_string()\n");
            plan_opt = _worker->get_plan_string();
            LOG(V2_INFO, "worker got plan string\n");
        }
    }

    _result = JobResult{};
    _result.id = this->getId();
    _result.revision = this->getRevision();
    if (plan_opt.has_value()) {
        LOG(V2_INFO, "result exists!\n");
        _result.result = 10;

        const std::string plan_string{std::move(plan_opt.value())};
        std::vector<int> plan_str_as_ints;
        plan_str_as_ints.reserve(plan_string.size());
        for (const auto c: plan_string) {
            plan_str_as_ints.push_back(c);
        }
        _result.setSolutionToSerialize(plan_str_as_ints.data(), plan_str_as_ints.size());
        _result.setSolution(std::move(plan_str_as_ints));

        LOG(V2_INFO, "plan_str:\n%s\n", plan_string.c_str());
    } else {
        _result.result = 0;
        std::vector<int> empty_solution{};
        _result.setSolutionToSerialize(empty_solution.data(), empty_solution.size());
        _result.setSolution(std::move(empty_solution));

        // TODO: what if we are called while no plan exists? Crash the world?
        LOG(V1_WARN, "Worker %d asked for plan while none exists (yet)!\n", getId());
    }

    // Don't tell me, the signature asks for it
    return std::move(_result);
}

void TohtnSimpleJob::appl_communicate() {
    // We do not communicate, ever
}

void TohtnSimpleJob::appl_communicate(int source, int mpiTag, JobMessage &msg) {
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
    // TODO: is this correct? Yup.
    return 1;
}

bool TohtnSimpleJob::appl_isDestructible() {
    // A thread is safe to destruct if it is no longer running
    // A thread which has terminated is no longer joinable (hooo-ray!)
    std::unique_lock has_terminated_lock{_has_terminated_mutex};
    return _work_thread.joinable() && _has_terminated;
}

void TohtnSimpleJob::appl_memoryPanic() {
    // TODO: if loop detection is performed, clean up the filters here
}

TohtnSimpleJob::~TohtnSimpleJob() {
    _work_thread.join();
}