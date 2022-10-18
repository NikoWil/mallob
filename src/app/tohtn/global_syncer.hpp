//
// Created by khondar on 17.10.22.
//

#ifndef MALLOB_GLOBAL_SYNCHER_HPP
#define MALLOB_GLOBAL_SYNCHER_HPP

#include "crowd/crowd_worker.hpp"

#include "comm/job_tree_all_reduction.hpp"

#include "memory"

class GlobalSyncer {
public:
    void update(JobTree &job_tree, int job_id, int revision);

    void receive_message(int source, int mpi_tag, JobMessage &msg, JobTree &job_tree, int revision, int job_id,
                         const JobDescription &desc, std::atomic<bool> &need_data, std::atomic<bool> &has_data,
                         std::vector<int> &data);

    /**
     * Activate on suspend
     * Cancels the reduction, leading to an empty message being sent onwards and will ignore anything that comes
     * next
     */
    void suspend();

    /**
     * Activate on version increase.
     * Interrupts the reduction and won't send any messages, makes place for a new one
     */
    void reset();

    std::optional<std::vector<int>> get_data();

private:
    void
    init_reduction(JobTree &job_tree, int revision, int epoch, int job_id, const JobDescription &desc,
                   std::atomic<bool> &need_data, std::atomic<bool> &has_data, std::vector<int> &data);

    std::unique_ptr<JobTreeAllReduction> _reduction;
    float _last_reduction_start{Timer::elapsedSeconds()};

    std::vector<int> _result{};

    std::vector<std::unique_ptr<JobTreeAllReduction>> _old_reductions{};

    int _epoch{0};
};

#endif //MALLOB_GLOBAL_SYNCHER_HPP
