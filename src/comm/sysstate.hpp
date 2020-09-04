
#ifndef DOMPASCH_MALLOB_SYSSTATE_HPP
#define DOMPASCH_MALLOB_SYSSTATE_HPP

#include "comm/mympi.hpp"

template <int N>
class SysState {

private:
    MPI_Comm& _comm;
    float _local_state[N];
    float _global_state[N];
    MPI_Request _request;
    bool _aggregating = false;
    float _last_aggregation = 0;
    float _last_check = 0;

public:
    SysState(MPI_Comm& comm);
    void setLocal(int pos, float val);
    void setLocal(std::initializer_list<float> elems);
    bool aggregate(float elapsedTime = Timer::elapsedSeconds());
    float* getGlobal();
};

#include "sysstate_impl.hpp"

#endif