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

class HtnInstance;

// TODO: params?
std::shared_ptr<HtnInstance>
get_htn_instance(std::string &domain_file_name, std::string &problem_file_name);

enum class WorkerPlanState {
    PLANNING,
    OUT_OF_WORK,
    PLAN,
};

struct WorkerMessage {
    int tag;
    int source;
    int dest;
    std::vector<uint8_t> data;
};

class SingleThreadedCrowdWorker {
public:
    virtual WorkerPlanState plan_step() = 0;

    [[nodiscard]] virtual std::optional<std::string> get_plan_string() const = 0;

    [[nodiscard]] virtual bool has_plan() const = 0;

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
    virtual void add_message(WorkerMessage& message) = 0;

    /**
     * Get any messages that the CooperativeCrowdWorker wants to send
     * @param worker_ids The worker ids to which a message may be sent
     * @return
     */
    [[nodiscard]] virtual std::vector<WorkerMessage> get_messages(const std::vector<int>& worker_ids) = 0;

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

    ~CooperativeCrowdWorker() override = default;
};

std::unique_ptr<SingleThreadedCrowdWorker> create_crowd_worker(std::shared_ptr<HtnInstance> htn);

#endif //CROWDHTN_CROWD_WORKER_HPP
