//
// Created by khondar on 17.05.22.
//

#include <memory>
#include <optional>
#include <vector>

#ifndef MALLOB_CROWD_WORKER_HPP
#define MALLOB_CROWD_WORKER_HPP

class HtnInstance;

// TODO: params?
std::shared_ptr<HtnInstance>
get_htn_instance(const std::string &domain_file_name, const std::string &problem_file_name);

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
};

std::unique_ptr<SingleThreadedCrowdWorker> create_crowd_worker(std::shared_ptr<HtnInstance> htn);

#endif //MALLOB_CROWD_WORKER_HPP
