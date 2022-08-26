//
// Created by khondar on 25.08.22.
//

#ifndef MALLOB_REDUCTION_COMMUNICATOR_HPP
#define MALLOB_REDUCTION_COMMUNICATOR_HPP

#include <memory>

#include "comm/job_tree_all_reduction.hpp"
#include "crowd/crowd_worker.hpp"

class ReductionCommunicator {
public:
    ReductionCommunicator();

    std::optional<std::vector<int>> update(JobTree &job_tree, int job_id, int revision);

    /**
     * Receives a message if it is meant for the ReductionCommunicator. Whether it is meant for the
     * ReductionCommunicator is determined by msg.tag
     * Messages that are accepted are using the tags MPI_TAGS::LOOP_DETECTION_REDUCTION_ANNOUNCE and
     * MPI_TAGS::LOOP_DETECTION_REDUCTION_DATA
     * @param msg
     * @return
     */
    void receive_message(int source, int mpi_tag, JobMessage &msg, JobTree &job_tree, int revision, int job_id,
                         const JobDescription &description, std::mutex &worker_mutex, CooperativeCrowdWorker &worker);

private:
    std::unique_ptr<JobTreeAllReduction> _reduction;
    // Data to manage the communication of loop detector data
    float _last_aggregation_start;
    // initialize as none object, to avoid a weird initial state
    enum class ReductionState {
        // used while no reduction is going on at all
        INACTIVE,
        // used while
        ACTIVE,
        DONE,
    };
    ReductionState _reduction_state;

    void update_inactive(JobTree &job_tree, int job_id, int revision);

    std::optional<std::vector<int>> update_active();

    void update_done();

    void receive_inactive(JobMessage &msg, JobTree &job_tree, int revision, int job_id,
                          const JobDescription &description, std::mutex &worker_mutex, CooperativeCrowdWorker &worker);

    void receive_active(int source, int mpi_tag, JobMessage &msg, const JobDescription &description);

    void receive_done(JobMessage &msg, JobTree &job_tree, int revision, int job_id,
                      const JobDescription &description, std::mutex &worker_mutex, CooperativeCrowdWorker &worker);

    /**
     * Initializes the encapsulated JobTreeAllReduction both locally and globally
     * Globally, the message with MPI_TAGS::LOOP_DETECTION_REDUCTION_ANNOUNCE is forwarded to all children to activate
     * them as well.
     * Locally, a new JobTreeAllReduction is initialized and the local value is set via the produce() function of the
     * JobTreeAllReduction
     */
    void init_reduction(JobTree &job_tree, int revision, int job_id, const JobDescription &description,
                        std::mutex &worker_mutex, CooperativeCrowdWorker &worker);
};


#endif //MALLOB_REDUCTION_COMMUNICATOR_HPP
