//
// Created by khondar on 21.06.22.
//

#include <fstream>
#include "tohtn_multi_job.hpp"
#include "tohtn_utils.hpp"

TohtnMultiJob::TohtnMultiJob(const Parameters &params, int commSize, int worldRank, int jobId,
                             JobDescription::Application appl) : Job(params, commSize, worldRank, jobId, appl) {}

void TohtnMultiJob::init_job() {
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
    LOG(V2_INFO, "HtnInstance created\n");

    LOG(V2_INFO, "Worker is root: %s\n", getJobTree().isRoot() ? "true" : "false");
    _worker = create_cooperative_worker(_htn, getJobTree().isRoot());
    LOG(V2_INFO, "Worker created\n");
}

void TohtnMultiJob::appl_start() {
    init_job();

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
                if (_worker->plan_step() == WorkerPlanState::PLAN) {
                    return;
                }
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

void TohtnMultiJob::appl_suspend() {
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

void TohtnMultiJob::appl_resume() {
    // resume thread
    {
        std::unique_lock suspend_lock{_suspend_mutex};
        _suspend_flag = false;
    }
    // Cppreference says that holding the lock while calling notify_all is a pessimization
    // https://en.cppreference.com/w/cpp/thread/condition_variable/notify_all
    _suspend_cvar.notify_all();
}

void TohtnMultiJob::appl_terminate() {
    // Separate flag such that the lock gets destroyed, the _worker_thread can actually read the _terminate_flag and
    // then terminate accordingly
    {
        std::unique_lock terminate_lock{_terminate_mutex};
        _termiante_flag = true;
    }
    // only a resumed job will actually see the _terminate_flag and act accordingly
    appl_resume();
}

int TohtnMultiJob::appl_solved() {
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

JobResult &&TohtnMultiJob::appl_getResult() {
    LOG(V2_INFO, "TohtnSimpleJob::appl_getResult()\n");
    std::optional<std::string> plan_opt;
    {
        std::unique_lock worker_lock{_worker_mutex};
        if (_worker) {
            LOG(V2_INFO, "_worker->get_plan_string()\n");
            plan_opt = _worker->get_plan_string();
            LOG(V2_INFO, "worker got plan string optional\n");
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

void TohtnMultiJob::appl_communicate() {
    std::vector<OutWorkerMessage> worker_messages{};
    {
        std::unique_lock worker_lock{_worker_mutex};
        worker_messages = _worker->get_messages(getJobComm().getRanklist());
    }

    for (auto &msg: worker_messages) {
        JobMessage job_msg;
        job_msg.jobId = getId();
        job_msg.revision = getRevision();
        job_msg.tag = msg.tag;
        // TODO: job_msg.epoch = ???
        // TODO: job_msg.checksum = ???
        job_msg.payload = std::move(msg.data);

        MyMpi::isend(msg.dest, MSG_SEND_APPLICATION_MESSAGE, job_msg);
    }
}

void TohtnMultiJob::appl_communicate(int source, int mpiTag, JobMessage& msg) {
    InWorkerMessage worker_msg;
    worker_msg.tag = msg.tag;
    worker_msg.source = source;
    worker_msg.data = std::move(msg.payload);

    std::unique_lock worker_lock{_worker_mutex};
    _worker->add_message(worker_msg);
}

void TohtnMultiJob::appl_dumpStats() {

}

bool TohtnMultiJob::appl_isDestructible() {
    std::unique_lock worker_lock{_worker_mutex};
    std::unique_lock has_terminated_lock{_has_terminated_mutex};
    return _work_thread.joinable() && _has_terminated && _worker->is_destructible();
}

void TohtnMultiJob::appl_memoryPanic() {
    // TODO: clean up memory used for loop detection
}

TohtnMultiJob::~TohtnMultiJob() {
    _work_thread.join();
}
