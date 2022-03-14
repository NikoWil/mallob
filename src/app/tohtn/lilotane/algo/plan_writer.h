
#ifndef DOMPASCH_LILOTANE_PLAN_WRITER_H
#define DOMPASCH_LILOTANE_PLAN_WRITER_H

#include <iostream>
#include <sstream>

#include "../data/htn_instance.h"
#include "../data/plan.h"

class PlanWriter {

private:
    HtnInstance& _htn;
    ParametersLilotane& _params;

public:
    PlanWriter(HtnInstance& htn, ParametersLilotane& params) : _htn(htn), _params(params) {}
    void outputPlan(Plan& _plan);
};

#endif