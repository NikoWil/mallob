//
// Created by khondar on 21.06.22.
//

#ifndef MALLOB_TOHTN_MULTI_JOB_HPP
#define MALLOB_TOHTN_MULTI_JOB_HPP


#include "app/job.hpp"
#include "crowd/crowd_worker.hpp"

class TohtnMultiJob : public Job {
public:
    TohtnMultiJob(const Parameters &params, int commSize, int worldRank, int jobId, JobDescription::Application appl);

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
    std::string _domain_file_name{};
    std::string _problem_file_name{};
    std::shared_ptr<HtnInstance> _htn{};
    std::unique_ptr<CooperativeCrowdWorker> _worker{};
    std::thread _work_thread{};

    bool _suspend_flag{false};
    std::mutex _suspend_mutex{};
    std::condition_variable _suspend_cvar{};

    std::mutex _worker_mutex{};

    // Flag & mutex to signal the _work_thread when it should abort planning and terminate
    bool _termiante_flag{false};
    std::mutex _terminate_mutex{};

    // Flag to show that termination request hast been received
    // isDestructible only returns true once this flag is true as it guarantees that the thread will join fast and can
    // thus be joined without violating the mallob preconditions (i.e. the thread has seen the set _termiante_flag and
    // is not currently in a long planning episode)
    bool _has_terminated{false};
    std::mutex _has_terminated_mutex{};

    // Only exists as appl_getResult returns a reference and we need the JobResult to survive beyond the function call
    JobResult _result{};

    void init_job();
};


#endif //MALLOB_TOHTN_MULTI_JOB_HPP
