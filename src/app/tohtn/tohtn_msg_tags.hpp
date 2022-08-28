//
// Created by khondar on 16.03.22.
//

#ifndef MALLOB_TOHTN_MSG_TAGS_HPP
#define MALLOB_TOHTN_MSG_TAGS_HPP
namespace MPI_TAGS {
    constexpr int LOOP_DETECTION_REDUCTION_DATA{100};
    constexpr int LOOP_DETECTION_REDUCTION_ANNOUNCE{200};
    constexpr int WORKER_VERSION_BCAST{300};
}

#endif //MALLOB_TOHTN_MSG_TAGS_HPP
