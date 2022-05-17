//
// Created by khondar on 15.03.22.
//

#ifndef MALLOB_TOHTN_SYSSTATE_HPP
#define MALLOB_TOHTN_SYSSTATE_HPP

#include "tohtn_msg_tags.hpp"

#include "app/job_tree.hpp"
#include "util/sys/timer.hpp"

#include <array>

template<int N>
class TohtnSysstate {
private:
    void handle_bcast_up(const JobMessage &msg) {

    }

    void handle_bcast_down(const JobMessage &msg) {

    }

public:
    TohtnSysstate(float aggregation_interval, const JobTree &job_tree) : _recv_left{false}, _recv_right{false},
                                                                         _aggregation_interval{aggregation_interval},
                                                                         _last_bcast{Timer::elapsedSeconds()},
                                                                         _job_tree{job_tree} {
        std::fill(_local.begin(), _local.end(), 0.);
        std::fill(_global.begin(), _global.end(), 0.);
        std::fill(_upstream.begin(), _upstream.end(), 0.);
        std::fill(_downstream.begin(), _downstream.end(), 0.);
    }

    void set_local(std::size_t idx, float value) {
        _local[idx] = value;
    }

    [[nodiscard]] float get_global(std::size_t idx) const {
        return _global[idx];
    }


    /**
     * Regularly call to initiate a broadcast from the leaf
     */
    void poll() {
        if (_job_tree.isLeaf() && (Timer::elapsedSeconds() - _last_bcast) >= _aggregation_interval) {
            const std::size_t num_bytes{N * sizeof(int)};
            std::vector<std::uint8_t> data_obj(num_bytes);

            memcpy(data_obj.data(), _upstream.data(), num_bytes);

            MyMpi::isend(_job_tree.getParentIndex(), 1, std::move(data_obj));
        }
    }

    void handle_message(const JobMessage &msg) {
        if (msg.tag == MPI_TAGS::TOHTN_SYSSTATE_BCAST_UP) {
            handle_bcast_up(msg);
        } else if (msg.tag == MPI_TAGS::TOHTN_SYSSTATE_BCAST_DOWN) {
            handle_bcast_up(msg);
        } else {
            LOG(V0_CRIT, "TohtnSysstate::handle_message - Invalid MPI message tag %d\n", msg.tag);
            Process::doExit(1);
        }
    }

private:
    std::array<float, N> _local;
    std::array<float, N> _global;

    std::array<float, N> _upstream;
    bool _recv_left;
    bool _recv_right;

    std::array<float, N> _downstream;
    float _aggregation_interval;
    float _last_bcast;

    const JobTree &_job_tree;
};

#endif //MALLOB_TOHTN_SYSSTATE_HPP
