//
// Created by khondar on 25.08.22.
//

#include "reduction_communicator.hpp"
#include "tohtn_msg_tags.hpp"

JobMessage get_loop_detection_base_message(const JobDescription &desc) {
    return {desc.getId(), desc.getRevision(), 0, MPI_TAGS::LOOP_DETECTION_REDUCTION_DATA, {}};
}

ReductionCommunicator::ReductionCommunicator()
        : _reduction{}, _last_aggregation_start{Timer::elapsedSeconds()}, _reduction_state{ReductionState::INACTIVE} {
}

std::optional<std::vector<int>>
ReductionCommunicator::update(JobTree &job_tree, int job_id, int revision) {
    switch (_reduction_state) {
        case ReductionState::INACTIVE:
            update_inactive(job_tree, job_id, revision);
            return {};
        case ReductionState::ACTIVE:
            return update_active();
        case ReductionState::DONE:
            update_done();
            return {};
    }
}

void ReductionCommunicator::receive_message(int source, int mpi_tag, JobMessage &msg, JobTree &job_tree, int revision,
                                            int job_id,
                                            const JobDescription &description, std::mutex &worker_mutex,
                                            CooperativeCrowdWorker &worker) {
    switch (_reduction_state) {
        case ReductionState::INACTIVE:
            receive_inactive(msg, job_tree, revision, job_id, description, worker_mutex, worker);
            break;
        case ReductionState::ACTIVE:
            receive_active(source, mpi_tag, msg, description);
            break;
        case ReductionState::DONE:
            receive_done(msg, job_tree, revision, job_id, description, worker_mutex, worker);
            break;
    }
}

void ReductionCommunicator::update_inactive(JobTree &job_tree, int job_id, int revision) {
    assert(!_reduction);
    if (job_tree.isRoot()) {
        if (Timer::elapsedSeconds() - _last_aggregation_start >= 1.) {
            {
                // Send message to self to use the common init code
                _last_aggregation_start = Timer::elapsedSeconds();

                JobMessage init_msg{};
                init_msg.jobId = job_id;
                init_msg.revision = revision;
                init_msg.tag = MPI_TAGS::LOOP_DETECTION_REDUCTION_ANNOUNCE;
                init_msg.payload = {};

                MyMpi::isend(job_tree.getRank(), MSG_SEND_APPLICATION_MESSAGE, init_msg);
            }
        }
    }
}

std::optional<std::vector<int>> ReductionCommunicator::update_active() {
    assert(_reduction);

    _reduction->advance();
    if (_reduction->hasResult()) {
        _reduction_state = ReductionState::DONE;
        return _reduction->extractResult();
    } else {
        return {};
    }
}

void ReductionCommunicator::update_done() {
    assert(_reduction->hasResult());

    if (_reduction->isDestructible()) {
        _reduction = {};
        _reduction_state = ReductionState::INACTIVE;
    }
}

void ReductionCommunicator::receive_inactive(JobMessage &msg, JobTree &job_tree, int revision, int job_id,
                                             const JobDescription &description, std::mutex &worker_mutex,
                                             CooperativeCrowdWorker &worker) {
    assert(msg.tag != MPI_TAGS::LOOP_DETECTION_REDUCTION_DATA);
    if (msg.tag == MPI_TAGS::LOOP_DETECTION_REDUCTION_ANNOUNCE) {
        init_reduction(job_tree, revision, job_id, description, worker_mutex, worker);
        _reduction_state = ReductionState::ACTIVE;
    }
}

void
ReductionCommunicator::receive_active(int source, int mpi_tag, JobMessage &msg, const JobDescription &description) {
    if (msg.tag == MPI_TAGS::LOOP_DETECTION_REDUCTION_DATA) {
        _reduction->receive(source, mpi_tag, msg);
    } else if (msg.tag == MPI_TAGS::LOOP_DETECTION_REDUCTION_ANNOUNCE && msg.returnedToSender) {
        // A child died before they could send us loop detector data. Just pretend it sent back an empty message
        // to make the JobTreeAllReduction happy (i.e, pretend all children replied)
        JobMessage fake_msg{get_loop_detection_base_message(description)};
        fake_msg.payload = empty_loopdetector_message();
        _reduction->receive(source, MSG_JOB_TREE_REDUCTION, fake_msg);
    }
}

/**
 * If the message is a broadcast announcement, destroy the old worker, skip ReductionState::INACTIVE, directly enter
 * ReductionState::ACTIVE
 * @param msg
 * @return
 */
void ReductionCommunicator::receive_done(JobMessage &msg, JobTree &job_tree, int revision, int job_id,
                                         const JobDescription &description, std::mutex &worker_mutex,
                                         CooperativeCrowdWorker &worker) {
    assert(msg.tag != MPI_TAGS::LOOP_DETECTION_REDUCTION_DATA);

    // Skip the inactive stage
    if (msg.tag == MPI_TAGS::LOOP_DETECTION_REDUCTION_ANNOUNCE) {
        // may lead to a more expensive destruction
        // Should be unlikely to impossible (as we always use the result), but is done as I rather code defensively
        // than try to prove the properties of a parallel state machine (and future code changes etc.)
        init_reduction(job_tree, revision, job_id, description, worker_mutex, worker);
        _reduction_state = ReductionState::ACTIVE;
    }
}

void
ReductionCommunicator::init_reduction(JobTree &job_tree, int revision, int job_id, const JobDescription &description,
                                      std::mutex &worker_mutex, CooperativeCrowdWorker &worker) {
    // Announce the whole thing to the world
    JobMessage bcast_msg;
    bcast_msg.revision = revision;
    bcast_msg.jobId = job_id;
    bcast_msg.tag = MPI_TAGS::LOOP_DETECTION_REDUCTION_ANNOUNCE;
    if (job_tree.hasLeftChild()) {
        MyMpi::isend(job_tree.getLeftChildNodeRank(), MSG_SEND_APPLICATION_MESSAGE, bcast_msg);
    }
    if (job_tree.hasRightChild()) {
        MyMpi::isend(job_tree.getRightChildNodeRank(), MSG_SEND_APPLICATION_MESSAGE, bcast_msg);
    }

    // Create the own reduction
    assert(!_reduction);
    _reduction =
            std::make_unique<JobTreeAllReduction>(job_tree, get_loop_detection_base_message(description),
                                                  empty_loopdetector_message(),
                                                  [](std::list<std::vector<int>> &elems) -> std::vector<int> {
                                                      if (elems.empty()) {
                                                          return empty_loopdetector_message();
                                                      }
                                                      std::vector<int> new_elem{
                                                              elems.front()};
                                                      for (auto &elem: elems) {
                                                          new_elem = combine_encoded_messages(
                                                                  new_elem,
                                                                  elem);
                                                      }
                                                      return new_elem;
                                                  }
            );

    // Get the data in there
    _reduction->produce([&worker_mutex, &worker]() {
        std::unique_lock worker_lock{worker_mutex};
        return worker.get_loop_detector_data();
    });
}
