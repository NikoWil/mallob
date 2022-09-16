//
// Created by khondar on 21.06.22.
//

#include <fstream>
#include "tohtn_multi_job.hpp"
#include "tohtn_utils.hpp"

TohtnMultiJob::TohtnMultiJob(const Parameters &params, const JobSetup &setup) : Job(params, setup) {}

void TohtnMultiJob::init_job() {
    {
        // Create filenames for domain and problem, unique to each worker
        // Uniqueness of filename is important in the case where the workers run on the same machine
        std::stringstream id_stream;
        id_stream << "job-" << std::to_string(getDescription().getId()) << "_";
        id_stream << "rank-" << std::to_string(getJobTree().getIndex());

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

    const auto[seeds, domain, problem] = extract_files(getDescription());

    std::ofstream domain_file{_domain_file_name};
    domain_file << domain;
    domain_file.close();

    std::ofstream problem_file{_problem_file_name};
    problem_file << problem;
    problem_file.close();

    _htn = get_htn_instance(_domain_file_name, _problem_file_name);

    _worker = create_cooperative_worker(_htn, seeds, SearchAlgorithm::DFS, LoopDetectionMode::GLOBAL_BLOOM,
                                        getJobTree().isRoot());
}

void TohtnMultiJob::appl_start() {
    _init_thread = std::thread([this]() {
        init_job();

        _work_thread = std::thread{[this]() {
            while (true) {

                if (_worker->plan_step() == WorkerPlanState::PLAN) {
                    _has_plan.store(true);
                    auto plan_opt{_worker->get_plan_string()};
                    assert(plan_opt.has_value());
                    _plan = std::string{plan_opt.value()};
                    return;
                }

                // The three sections are in different blocks to make it clear to the compiler that mutexes can be released
                // again
                std::vector<InWorkerMessage> new_in_msgs{};
                {
                    std::unique_lock in_msg_lock{_in_msg_mutex};
                    std::swap(_in_msgs, new_in_msgs);
                }
                for (auto &in_msg: new_in_msgs) {
                    _worker->add_message(in_msg);
                }

                std::vector<InWorkerMessage> new_return_msgs{};
                {
                    std::unique_lock return_msg_lock{_return_msg_mutex};
                    std::swap(_return_msgs, new_return_msgs);
                }
                for (auto &return_msg: new_return_msgs) {
                    _worker->return_message(return_msg);
                }

                std::vector<OutWorkerMessage> new_out_msgs{_worker->get_messages(getJobComm().getRanklist())};
                if (!new_out_msgs.empty()) {
                    std::unique_lock out_msg_lock{_out_msg_mutex};
                    _out_msgs.insert(_out_msgs.end(), new_out_msgs.begin(), new_out_msgs.end());
                }

                // suspension and termination are checked after the plan step and messages are handled
                // i.e., all messages from crowd's side are handled once we reach this point
                // this assures us that we are kinda fine

                if (_should_suspend.load()) {
                    std::mutex token_mutex{};
                    std::unique_lock token_lock{token_mutex};
                    _suspend_cvar.wait(token_lock, [this]() -> bool { return !_should_suspend.load(); });
                }

                // check for termination
                if (_should_terminate.load()) {
                    _did_terminate.store(true);
                    return;
                }
            }
        }};
        LOG(V2_INFO, "Work Thread started\n");
    });
}

void TohtnMultiJob::appl_suspend() {
    if (_should_terminate.load()) {
        return;
    }
    // get rid of any leftover messages!
    this->communicate();
    _should_suspend.store(true);
}

void TohtnMultiJob::appl_resume() {
    // resume thread
    _should_suspend.store(false);

    // Cppreference says that holding the lock while calling notify_all is a pessimization
    // https://en.cppreference.com/w/cpp/thread/condition_variable/notify_all
    _suspend_cvar.notify_all();
}

void TohtnMultiJob::appl_terminate() {
    _should_terminate.store(true);
    // get rid of any leftover messages!
    this->communicate();

    // only a resumed job will actually see the _terminate_flag and act accordingly
    appl_resume();
}

int TohtnMultiJob::appl_solved() {
    if (_has_plan.load() && !_returned_solved) {
        _returned_solved = true;
        return 1;
    } else {
        return -1;
    }
}

JobResult &&TohtnMultiJob::appl_getResult() {
    LOG(V2_INFO, "TohtnMultiJob::appl_getResult()\n");

    _result = JobResult{};
    _result.id = this->getId();
    _result.revision = this->getRevision();
    if (_has_plan.load()) {
        LOG(V2_INFO, "result exists!\n");
        _result.result = 10;

        std::vector<int> plan_str_as_ints;
        plan_str_as_ints.reserve(_plan.size());
        for (const auto c: _plan) {
            plan_str_as_ints.push_back(c);
        }
        _result.setSolutionToSerialize(plan_str_as_ints.data(), plan_str_as_ints.size());
        _result.setSolution(std::move(plan_str_as_ints));
    } else {
        _result.result = 0;
        std::vector<int> empty_solution{};
        _result.setSolutionToSerialize(empty_solution.data(), empty_solution.size());
        _result.setSolution(std::move(empty_solution));
    }

    // Don't tell me, the signature asks for it
    return std::move(_result);
}

void TohtnMultiJob::appl_communicate() {
    std::vector<OutWorkerMessage> new_out_msgs{};
    {
        std::unique_lock out_msg_lock{_out_msg_mutex};
        std::swap(new_out_msgs, _out_msgs);
    }

    for (auto &msg: new_out_msgs) {
        JobMessage job_msg;
        job_msg.jobId = getId();
        job_msg.revision = getRevision();
        job_msg.tag = msg.tag;
        // job_msg.epoch and job_msg.checksum can be ignored so far.
        job_msg.payload = std::move(msg.data);

        MyMpi::isend(msg.dest, MSG_SEND_APPLICATION_MESSAGE, job_msg);
    }
}

void TohtnMultiJob::appl_communicate(int source, int mpiTag, JobMessage &msg) {
    InWorkerMessage worker_msg;
    worker_msg.tag = msg.tag;
    worker_msg.source = source;
    worker_msg.data = std::move(msg.payload);

    if (msg.returnedToSender) {
        std::unique_lock return_msg_lock{_return_msg_mutex};
        _return_msgs.push_back(std::move(worker_msg));
    } else {
        std::unique_lock in_msg_lock{_in_msg_mutex};
        _in_msgs.push_back(std::move(worker_msg));
    }
}

void TohtnMultiJob::appl_dumpStats() {

}

bool TohtnMultiJob::appl_isDestructible() {
    // TODO: protect call to _worker->is_destructible()
    return _init_thread.joinable() && _work_thread.joinable() && _did_terminate.load();
}

void TohtnMultiJob::appl_memoryPanic() {
    // TODO: clean up memory used for loop detection
}

TohtnMultiJob::~TohtnMultiJob() {
    _init_thread.join();
    _work_thread.join();
}
