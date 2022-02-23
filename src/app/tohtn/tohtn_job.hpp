//
// Created by khondar on 14.02.22.
//

#ifndef MALLOB_TOHTN_JOB_HPP
#define MALLOB_TOHTN_JOB_HPP


#include "app/job.hpp"

class TohtnJob : public Job {
public:
    TohtnJob(const Parameters& params, int commSize, int worldRank, int jobId);

    void appl_start() override;

    void appl_suspend() override;

    void appl_resume() override;

    void appl_terminate() override;

    int appl_solved() override;

    JobResult&& appl_getResult() override;

    bool appl_wantsToBeginCommunication() override;

    void appl_beginCommunication() override;

    void appl_communicate(int source, JobMessage& msg) override;

    void appl_dumpStats() override;

    bool appl_isDestructible() override;

private:
    JobResult _result;
};


#endif //MALLOB_TOHTN_JOB_HPP
