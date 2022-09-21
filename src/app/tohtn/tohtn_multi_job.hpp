//
// Created by khondar on 21.06.22.
//

#ifndef MALLOB_TOHTN_MULTI_JOB_HPP
#define MALLOB_TOHTN_MULTI_JOB_HPP


#include "app/job.hpp"
#include "crowd/crowd_worker.hpp"

class TohtnMultiJob : public Job {
public:
    TohtnMultiJob(const Parameters& params, const JobSetup& setup);

    void appl_start() override;

    void appl_suspend() override;

    void appl_resume() override;

    void appl_terminate() override;

    int appl_solved() override;

    JobResult&& appl_getResult() override;

    void appl_communicate() override;

    void appl_communicate(int source, int mpiTag, JobMessage& msg) override;

    void appl_dumpStats() override;

    // Consciously do not override getDemans and instead use the default strategy implemented in Job
    // int getDemand() const override;

    bool appl_isDestructible() override;

    void appl_memoryPanic() override;

    ~TohtnMultiJob() override;

private:
    // parallelize parsing, construction of our parallel worker etc
    std::thread _init_thread{};

    std::string _domain_file_name{};
    std::string _problem_file_name{};
    std::shared_ptr<HtnInstance> _htn{};
    std::unique_ptr<CooperativeCrowdWorker> _worker{};
    std::thread _work_thread{};

    std::atomic<bool> _has_plan{false};
    std::string _plan{};

    std::atomic<bool> _should_suspend{false};
    std::condition_variable _suspend_cvar{};

    std::mutex _out_msg_mutex{};
    std::vector<OutWorkerMessage> _out_msgs{};
    std::mutex _in_msg_mutex{};
    std::vector<InWorkerMessage> _in_msgs{};
    std::mutex _return_msg_mutex{};
    std::vector<InWorkerMessage> _return_msgs{};

    std::atomic<bool> _should_terminate{false};

    // Flag to show that termination request hast been received
    // isDestructible only returns true once this flag is true as it guarantees that the thread will join fast and can
    // thus be joined without violating the mallob preconditions (i.e. the thread has seen the set _termiante_flag and
    // is not currently in a long planning episode)
    std::atomic<bool> _did_terminate{false};

    // Only exists as appl_getResult returns a reference and we need the JobResult to survive beyond the function call
    JobResult _result{};
    // Bool to ensure that appl_solved() returns true only once per job
    bool _returned_solved{false};

    std::mutex _ranklist_mutex{};
    std::vector<int> _ranklist{};

    // Better logging
    size_t _worker_id{0};

    void init_job();
};


#endif //MALLOB_TOHTN_MULTI_JOB_HPP
