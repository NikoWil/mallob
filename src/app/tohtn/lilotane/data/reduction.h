
#ifndef DOMPASCH_TREE_REXX_REDUCTION_H
#define DOMPASCH_TREE_REXX_REDUCTION_H

#include <vector>
#include <map>

#include "../data/htn_op.h"
#include "../data/signature.h"

class Reduction : public HtnOp {

private:

    // Coding of the methods' AT's name.
    int _task_name_id = -1;
    // The method's AT's arguments.
    std::vector<int> _task_args;

    // The ordered list of subtasks.
    std::vector<USignature> _subtasks;

public:
    Reduction();
    Reduction(HtnOp& op);
    Reduction(const Reduction& r);
    Reduction(int nameId, const std::vector<int>& args, const USignature& task);
    Reduction(int nameId, const std::vector<int>& args, USignature&& task);

    void orderSubtasks(const std::map<int, std::vector<int>>& orderingNodelist);

    Reduction substituteRed(const Substitution& s) const;

    void addSubtask(const USignature& subtask);
    void addSubtask(USignature&& subtask);
    void setSubtasks(std::vector<USignature>&& subtasks);

    USignature getTaskSignature() const;    // The signature of the task this method resolves
    const std::vector<int>& getTaskArguments() const;   // seems to be parameter names?
    const std::vector<USignature>& getSubtasks() const;

    Reduction& operator=(const Reduction& other);
};

#endif