
#include "job_reader.hpp"

#include "app/dummy/dummy_reader.hpp"
#include "app/tohtn/tohtn_reader.hpp"
#include "util/logger.hpp"

bool JobReader::read(const std::vector<std::string>& files, SatReader::ContentMode contentMode, JobDescription& desc) {
    Logger::getMainInstance().log(V0_CRIT, "Entered JobReader::read\n");

    switch (desc.getApplication()) {
    case JobDescription::DUMMY:
        return DummyReader::read(files, desc);
    case JobDescription::ONESHOT_SAT:
    case JobDescription::INCREMENTAL_SAT:
        return SatReader(files.front(), contentMode).read(desc);
    case JobDescription::TOHTN:
        return TohtnReader::read(files, desc);
    default:
        return false;
    }
}
