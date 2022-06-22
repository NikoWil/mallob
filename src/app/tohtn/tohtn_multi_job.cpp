//
// Created by khondar on 21.06.22.
//

#include "tohtn_multi_job.hpp"

void TohtnMultiJob::appl_start() {

}

void TohtnMultiJob::appl_suspend() {

}

void TohtnMultiJob::appl_resume() {

}

void TohtnMultiJob::appl_terminate() {

}

int TohtnMultiJob::appl_solved() {

}

JobResult&& TohtnMultiJob::appl_getResult() {

}

bool TohtnMultiJob::appl_wantsToBeginCommunication() {

}

void TohtnMultiJob::appl_beginCommunication() {
    auto get_worker_messages = [this](){
        std::unique_lock worker_lock{_worker_mutex};
        return _worker->get_messages(getJobComm().getRanklist());
    };
    for (auto& msg : get_worker_messages()) {
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

void TohtnMultiJob::appl_communicate(int source, JobMessage& msg) {
    WorkerMessage worker_msg;
    worker_msg.tag = msg.tag;
    worker_msg.source = source;
    // ignore, the receiver hopefully knows who he is: worker_msg.dest
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

TohtnMultiJob::~TohtnMultiJob() {

}
