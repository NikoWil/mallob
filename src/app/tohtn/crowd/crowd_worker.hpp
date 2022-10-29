//
// Created by khondar on 23.05.22.
//

#ifndef CROWDHTN_CROWD_WORKER_HPP
#define CROWDHTN_CROWD_WORKER_HPP

//
// Created by khondar on 17.05.22.
//

#include <memory>
#include <optional>
#include <vector>
#include "worker_types.hpp"

class HtnInstance;

// TODO: params?
std::shared_ptr<HtnInstance>
get_htn_instance(std::string &domain_file_name, std::string &problem_file_name);

enum class WorkerPlanState {
    PLANNING,
    OUT_OF_WORK,
    PLAN,
};

class SingleThreadedCrowdWorker {
public:
    virtual WorkerPlanState plan_step() = 0;

    [[nodiscard]] virtual std::optional<std::string> get_plan_string() const = 0;

    [[nodiscard]] virtual bool has_plan() const = 0;

    [[nodiscard]] virtual bool has_work() const = 0;

    virtual void dump_stats() const = 0;

    virtual ~SingleThreadedCrowdWorker() = 0;
};

class CooperativeCrowdWorker : public SingleThreadedCrowdWorker {
public:
    /**
     * Deliver a single message to the CooperativeWorker. Such messages can be but are not limited to
     * - work requests
     * - positive work responses
     * - negative work responses
     * - positive work response acknowledgements
     * @param message
     */
    virtual void add_message(InWorkerMessage &message) = 0;

    /**
     * In some cases a message cannot be delivered to the worker it was intended for, e.g. due to the other worker
     * shutting down in between sending the message and it being delivered. In such a case, the message should be
     * returned to the original node for handling.
     * @param message
     */
    virtual void return_message(InWorkerMessage &message) = 0;

    /**
     * Get any messages that the CooperativeCrowdWorker wants to send
     * @param worker_ids The worker ids to which a message may be sent
     * @return
     */
    [[nodiscard]] virtual std::vector<OutWorkerMessage> get_messages(const std::vector<int> &worker_ids) = 0;

    /**
     * Get the data representing the state of an internal LoopDetector if it exists.
     * @return
     */
    [[nodiscard]] virtual std::vector<int> get_loop_detector_data() = 0;

    /**
     * Add the data encoded in message to the local LoopDetector for future checks against it. Returns true if the
     * internal version was increased as a result of the new data. Version increases can happen due to two reasons:
     * - the version of message is higher than the internal version
     * - the worker is the root worker and with the new data the internal loop detector filled up completely. As a
     *   result, the internal version was increased and the search restarted
     * @param message The LoopDetector to be added
     * @return True if the internal version was increased as a result of the data, false otherwise.
     */
    virtual bool add_loop_detector_data(std::vector<int> &message) = 0;

    /**
     * Gets the internal version of the worker.
     * @return
     */
    [[nodiscard]] virtual size_t get_version() const = 0;

     /**
      * Sets the internal version the max of the old value and the parameter version. Else does nothing. The internal
      * version affects things like loop detectors etc that will be reset. Each version increase corresponds to a global
      * restart.
      * @param version The new lower limit for the internal version.
      * @return True if the internal version increased, false otherwise.
      */
    virtual bool set_version(size_t version) = 0;

    /**
     * Tells a CooperativeCrowdWorker to stop any work. This will mean that
     * - plan_step will no longer perform any work
     * - no new work requests are sent out
     * - incoming work requests will be answered negatively, independent of available work packages
     * Any ongoing communication sequences will however be finished normally.
     */
    virtual void stop() = 0;

    /**
     * Marks whether the worker can be safely destroyed, i.e. whether any ongoing communication sequences have been
     * finished at this point.
     * @return
     */
    [[nodiscard]] virtual bool is_destructible() = 0;

    virtual void clear() = 0;

    /**
     * If memory might run out, delete some data to avoid crashing.
     */
    virtual void reduce_memory() = 0;

    /*
     * Is empty if there is no local root
     * @return
     */
    [[nodiscard]] virtual std::optional<OutWorkerMessage> get_local_root_message(const std::vector<int>& worker_ids) = 0;

    ~CooperativeCrowdWorker() override = default;
};

std::unique_ptr<SingleThreadedCrowdWorker>
create_crowd_worker(std::shared_ptr<HtnInstance> htn, SearchAlgorithm algorithm, LoopDetectionMode mode, const std::array<size_t, 4> &seeds);

std::unique_ptr<CooperativeCrowdWorker>
create_cooperative_worker(std::shared_ptr<HtnInstance> htn, const std::array<size_t, 4> &seeds, SearchAlgorithm algorithm,
                          LoopDetectionMode mode, bool is_root);

#endif //CROWDHTN_CROWD_WORKER_HPP
