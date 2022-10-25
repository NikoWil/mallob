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
    // destroy as many old reductions as possible, yee-haw
    bool changed{false};
    do {
        changed = false;
        for (size_t idx{0}; idx < _old_reductions.size(); ++idx) {
            /*if (!_old_reductions.at(idx)) {
                std::swap(_old_reductions.at(idx), _old_reductions.back());
                _old_reductions.pop_back();
            } else */if (_old_reductions.at(idx)->isDestructible()) {
                std::swap(_old_reductions.at(idx), _old_reductions.back());
                _old_reductions.pop_back();

                // avoid underflow case
                if (idx != 0) {
                    idx--;
                } else {
                    changed = true;
                }
            }
        }
    } while (changed);

    if (!_reduction && job_tree.isRoot()) {
        float curr_time{Timer::elapsedSeconds()};
        if (curr_time - _last_reduction_start >= 1.f) {
            //LOG(V2_INFO, "Kick off init reduction\n");
            _last_reduction_start = curr_time;
            JobMessage init_msg{};
            init_msg.jobId = job_id;
            init_msg.revision = revision;
            init_msg.tag = TOHTN_TAGS::INIT_REDUCTION;
            init_msg.payload = {};
            init_msg.epoch = _epoch + 1;

            MyMpi::isend(job_tree.getRank(), MSG_SEND_APPLICATION_MESSAGE, init_msg);
        }
    }

    if (_reduction) {
        _reduction->advance();

        // happens like this on root only, rest needs a message for it
        if (_reduction->hasResult()) {
            //LOG(V2_INFO, "Reduction complete (update)\n");
            _result = _reduction->extractResult();
            _reduction = {};
            _last_reduction_start = Timer::elapsedSeconds();
        }
    }
}

void
GlobalSyncer::receive_message(int source, int mpi_tag, JobMessage &msg, JobTree &job_tree, int revision,
                              int job_id, const JobDescription &desc, std::atomic<bool> &need_data) {
    if (mpi_tag == MSG_JOB_TREE_BROADCAST && msg.returnedToSender) {
        //LOG(V2_INFO, "recv message, Bcast came back\n");
        // Can happen if our children went invalid during the thing
        return;
    }

    if (msg.tag == TOHTN_TAGS::INIT_REDUCTION && msg.returnedToSender) {
        //LOG(V2_INFO, "recv message, Init came back\n");
        JobMessage fake_msg{get_base_message(desc, _epoch)};
        fake_msg.payload = empty_loopdetector_message();
        _reduction->receive(source, MSG_JOB_TREE_REDUCTION, fake_msg);
        return;
    }

    if (msg.tag == TOHTN_TAGS::INIT_REDUCTION && !msg.returnedToSender) {
        //LOG(V2_INFO, "recv message, Init reduction message\n");
        init_reduction(job_tree, revision, msg.epoch, job_id, desc, need_data);
        return;
    }

    if (_reduction && (mpi_tag == MSG_JOB_TREE_REDUCTION || mpi_tag == MSG_JOB_TREE_BROADCAST)) {
        //LOG(V2_INFO, "recv message, Loop data\n");
        // got some data
        _reduction->receive(source, mpi_tag, msg);

        if (_reduction->hasResult()) {
            //LOG(V2_INFO, "Reduction complete (message)\n");
            _result = _reduction->extractResult();
            _reduction = {};
            _last_reduction_start = Timer::elapsedSeconds();
        }
        return;
    }
    //LOG(V2_INFO, "recv message, didn't take it\n");
}

void GlobalSyncer::suspend() {
    _reduction->cancel();
    if (_reduction) {
        _old_reductions.push_back(std::move(_reduction));
        _reduction = {};
    }
}

void GlobalSyncer::reset() {
    if (_reduction) {
        _old_reductions.push_back(std::move(_reduction));
        _reduction = {};
    }
    _last_reduction_start = Timer::elapsedSeconds();
}

void GlobalSyncer::produce(std::function<std::vector<int>()> producer) {
    if (_reduction) {
        _reduction->produce(producer);
    }
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
                                  std::atomic<bool> &need_data) {
    /*LOG(V2_INFO, "Init reduction %d, is root: %s, left: %s, right: %s\n", epoch, job_tree.isRoot() ? "true" : "false",
        job_tree.hasLeftChild() ? "true" : "false", job_tree.hasRightChild() ? "true" : "false");//*/
    // Replace with new reduction if needed
    if (epoch > _epoch) {
        if (_reduction) {
            _old_reductions.push_back(std::move(_reduction));
            _reduction = {};
        }
    } else {
        return;
    }
    _epoch = epoch;

    // Announce the whole thing to the world
    JobMessage bcast_msg;
    bcast_msg.revision = revision;
    bcast_msg.jobId = job_id;
    bcast_msg.tag = TOHTN_TAGS::INIT_REDUCTION;
    bcast_msg.epoch = _epoch;
    if (job_tree.hasLeftChild()) {
        //LOG(V2_INFO, "Left child: %d\n", job_tree.getLeftChildNodeRank());
        MyMpi::isend(job_tree.getLeftChildNodeRank(), MSG_SEND_APPLICATION_MESSAGE, bcast_msg);
    }
    if (job_tree.hasRightChild()) {
        //LOG(V2_INFO, "Right child: %d\n", job_tree.getRightChildNodeRank());
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

    need_data.store(true);
}