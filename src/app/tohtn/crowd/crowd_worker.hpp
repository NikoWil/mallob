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

class SingleThreadedCrowdWorker {
public:
    virtual WorkerPlanState plan_step() = 0;

    [[nodiscard]] virtual std::optional<std::string> get_plan_string() const = 0;

    [[nodiscard]] virtual bool has_plan() const = 0;
};

std::unique_ptr<SingleThreadedCrowdWorker> create_crowd_worker(std::shared_ptr<HtnInstance> htn);

#endif //CROWDHTN_CROWD_WORKER_HPP
