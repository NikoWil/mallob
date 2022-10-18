//
// Created by khondar on 17.10.22.
//

#include "crowd/worker_types.hpp"

#include "tohtn_msg_tags.hpp"
#include "global_syncer.hpp"

JobMessage get_base_message(const JobDescription &desc, int epoch) {
    return {desc.getId(), desc.getRevision(), epoch, TOHTN_TAGS::REDUCTION_DATA, {}};
}

void GlobalSyncer::update(JobTree &job_tree, int job_id, int revision) {
    for (size_t idx{0}; idx < _old_reductions.size(); ++idx) {
        if (_old_reductions.at(idx)->isDestructible()) {
            std::swap(_old_reductions.at(idx), _old_reductions.back());
            _old_reductions.resize(_old_reductions.size() - 1);
            // we need to check the current position again!
            idx--;
        }
    }

    if (!_reduction && job_tree.isRoot()) {
        float curr_time{Timer::elapsedSeconds()};
        if (curr_time - _last_reduction_start >= 1.f) {
            _last_reduction_start = curr_time;
            JobMessage init_msg{};
            init_msg.jobId = job_id;
            init_msg.revision = revision;
            init_msg.tag = TOHTN_TAGS::INIT_REDUCTION;
            init_msg.payload = {};
            init_msg.epoch = ++_epoch;

            MyMpi::isend(job_tree.getRank(), MSG_SEND_APPLICATION_MESSAGE, init_msg);
        }
    }

    if (_reduction) {
        _reduction->advance();

        // happens like this on root only, rest needs a message for it
        if (_reduction->hasResult()) {
            _result = _reduction->extractResult();
            _reduction = {};
        }
    }
}

void
GlobalSyncer::receive_message(int source, int mpi_tag, JobMessage &msg, JobTree &job_tree, int revision,
                              int job_id, const JobDescription &desc, std::atomic<bool> &need_data,
                              std::atomic<bool> &has_data, std::vector<int> &data) {
    if (mpi_tag == MSG_JOB_TREE_BROADCAST && msg.returnedToSender) {
        // Can happen if our children went invalid during the thing
        return;
    }

    if (msg.tag == TOHTN_TAGS::INIT_REDUCTION && msg.returnedToSender) {
        JobMessage fake_msg{get_base_message(desc, _epoch)};
        fake_msg.payload = empty_loopdetector_message();
        _reduction->receive(source, MSG_JOB_TREE_REDUCTION, fake_msg);
        return;
    }

    if (msg.tag == TOHTN_TAGS::INIT_REDUCTION && !msg.returnedToSender) {
        init_reduction(job_tree, revision, msg.epoch, job_id, desc, need_data, has_data, data);
        return;
    }

    if (_reduction && mpi_tag == MSG_JOB_TREE_REDUCTION || mpi_tag == MSG_JOB_TREE_BROADCAST) {
        // got some data
        _reduction->receive(source, mpi_tag, msg);

        if (_reduction->hasResult()) {
            _result = _reduction->extractResult();
            _reduction = {};
            _last_reduction_start = Timer::elapsedSeconds();
        }
    }
}

void GlobalSyncer::suspend() {
    _reduction->cancel();
    _old_reductions.push_back(std::move(_reduction));
    _reduction = {};
}

void GlobalSyncer::reset() {
    _old_reductions.push_back(std::move(_reduction));
    _reduction = {};
    _last_reduction_start = Timer::elapsedSeconds();
}

std::optional<std::vector<int>> GlobalSyncer::get_data() {
    if (_result.empty()) {
        return std::optional<std::vector<int>>{};
    } else {
        std::vector<int> new_result{};
        std::swap(_result, new_result);
        return new_result;
    }
}

void GlobalSyncer::init_reduction(JobTree &job_tree, int revision, int epoch, int job_id, const JobDescription &desc,
                                  std::atomic<bool> &need_data, std::atomic<bool> &has_data, std::vector<int> &data) {
    if (epoch > _epoch && _reduction) {
        _old_reductions.push_back(std::move(_reduction));
        _reduction = {};
    }

    _epoch = epoch;
    // Announce the whole thing to the world
    JobMessage bcast_msg;
    bcast_msg.revision = revision;
    bcast_msg.jobId = job_id;
    bcast_msg.tag = TOHTN_TAGS::INIT_REDUCTION;
    if (job_tree.hasLeftChild()) {
        MyMpi::isend(job_tree.getLeftChildNodeRank(), MSG_SEND_APPLICATION_MESSAGE, bcast_msg);
    }
    if (job_tree.hasRightChild()) {
        MyMpi::isend(job_tree.getRightChildNodeRank(), MSG_SEND_APPLICATION_MESSAGE, bcast_msg);
    }
    // Create the own reduction
    assert(!_reduction);
    _reduction =
            std::make_unique<JobTreeAllReduction>(job_tree, get_base_message(desc, _epoch),
                                                  empty_loopdetector_message(),
                                                  [](std::list<std::vector<int>> &elems) -> std::vector<int> {
                                                      if (elems.empty()) {
                                                          return empty_loopdetector_message();
                                                      }
                                                      std::vector<int> new_elem{elems.front()};
                                                      for (auto &elem: elems) {
                                                          new_elem = combine_encoded_messages(new_elem, elem);
                                                      }
                                                      return new_elem;
                                                  }
            );
    // Get the data in there
    _reduction->produce([&need_data, &has_data, &data]() {
        need_data.store(true);
        while (!has_data.load()) {

        }
        std::vector<int> new_data{std::move(data)};
        need_data.store(false);
        has_data.store(false);
        return new_data;
    });

    _reduction->advance();
}