
#include <algorithm>

#include "app/sat/sharing/import_buffer.hpp"

#include "util/sys/process.hpp"
#include "util/sys/thread_pool.hpp"
#include "util/random.hpp"
#include "util/logger.hpp"
#include "util/sys/timer.hpp"
#include "app/sat/sharing/buffer/priority_clause_buffer.hpp"
#include "app/sat/sharing/buffer/buffer_merger.hpp"
#include "app/sat/sharing/buffer/buffer_reducer.hpp"
#include "util/sys/terminator.hpp"
#include "util/assert.hpp"

Mallob::Clause generateClause(int minLength, int maxLength) {
    int length = minLength + (int) (Random::rand() * (maxLength-minLength));
    assert(length >= minLength);
    assert(length <= maxLength);
    int lbd = 2 + (int) (Random::rand() * (length-2));
    if (length == 1) lbd = 1;
    assert(lbd >= 2 || (length == 1 && lbd == 1));
    assert(lbd <= length);
    Mallob::Clause c((int*)malloc(length*sizeof(int)), length, lbd);
    for (size_t i = 0; i < length; ++i) {
        c.begin[i] = -100000 + (int) (Random::rand() * 200001);
        assert(c.begin[i] >= -100000);
        assert(c.begin[i] <= 100000);
    }
    std::sort(c.begin, c.begin+length);
    return c;
}

int getProducer(const Mallob::Clause& clause, int nbProducers) {
    int sum = 0;
    for (int i = 0; i < clause.size; i++) sum += std::abs(clause.begin[i]);
    return sum % nbProducers;
}

void testBasic() {
    PriorityClauseBuffer::Setup setup;
    setup.maxClauseLength = 20;
    setup.slotsForSumOfLengthAndLbd = false;
    setup.maxLbdPartitionedSize = 2;
    setup.numLiterals = 10;
    PriorityClauseBuffer pcb(setup);

    Mallob::Clause c;
    int nbExportedCls;

    for (int rep = 0; rep <= 10; rep++) {

        for (int size = 10; size >= 2; size--) {
            c = generateClause(size, size);
            LOG(V2_INFO, "INSERT %s\n", c.toStr().c_str());
            bool success = pcb.addClause(c);
            assert(success);
            assert(pcb.checkTotalLiterals());
        }
        
        assert(pcb.getNumLiterals(2, 2) == 2);
        assert(pcb.getNumLiterals(3, 2) == 3);
        assert(pcb.getNumLiterals(4, 2) == 4);
        assert(pcb.getNumLiterals(5, 2) == 0);
        assert(pcb.getCurrentlyUsedLiterals() == 9);

        pcb.exportBuffer(999, nbExportedCls);
        assert(nbExportedCls == 3);
        assert(pcb.checkTotalLiterals());
        assert(pcb.getCurrentlyUsedLiterals() == 0);

        for (int size = 1; size <= 10; size++) {
            c = generateClause(size, size);
            LOG(V2_INFO, "INSERT %s\n", c.toStr().c_str());
            bool success = pcb.addClause(c);
            assert(success == (size <= 4));
            assert(pcb.checkTotalLiterals());
        }

        assert(pcb.getNumLiterals(1, 1) == 1);
        assert(pcb.getNumLiterals(2, 2) == 2);
        assert(pcb.getNumLiterals(3, 2) == 3);
        assert(pcb.getNumLiterals(4, 2) == 4);
        assert(pcb.getNumLiterals(5, 2) == 0);
        assert(pcb.getCurrentlyUsedLiterals() == 10);

        pcb.exportBuffer(999, nbExportedCls);
        assert(nbExportedCls == 4);
        assert(pcb.checkTotalLiterals());
        assert(pcb.getCurrentlyUsedLiterals() == 0);
    }
}

int main() {
    Timer::init();
    Random::init(rand(), rand());
    Logger::init(0, V5_DEBG);
    Process::init(0);
    ProcessWideThreadPool::init(4);
    
    testBasic();
}
