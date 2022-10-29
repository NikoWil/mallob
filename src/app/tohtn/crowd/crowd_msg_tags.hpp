//
// Created by khondar on 23.08.22.
// General message tags as they are used by messages sent by crowd. These tags are not directly needed by any users of
// crowd. Knowing them might still be helpful to avoid collisions with own, internal tags.
//

#ifndef CROWDHTN_CROWD_MSG_TAGS_HPP
#define CROWDHTN_CROWD_MSG_TAGS_HPP

constexpr int CROWD_TAG_WORK_REQUEST{10};
constexpr int CROWD_TAG_POSITIVE_WORK_RESPONSE{20};
constexpr int CROWD_TAG_NEGATIVE_WORK_RESPONSE{30};
constexpr int CROWD_TAG_LOCAL_ROOT{40};

#endif //CROWDHTN_CROWD_MSG_TAGS_HPP
