#include "cube_root.hpp"

#include <cassert>
#include <cmath>

CubeRoot::CubeRoot(CubeSetup &setup)
    : _formula(setup.formula), _cube_comm(setup.cube_comm), _logger(setup.logger), _result(setup.result), _terminator(_isInterrupted) {

    _depth = setup.params.getIntParam("cube-depth");
    _cubes_per_worker = setup.params.getIntParam("cubes-per-worker");

    _solver.connect_terminator(&_terminator);
}

CubeRoot::~CubeRoot() { _logger.log(0, "Enter destructor of CubeRoot.\n"); }

bool CubeRoot::generateCubes() {
    // Read formula
    for (auto lit : *_formula.get()) {
        _solver.add(lit);
    }

    // Create cubes
    auto cubesWithStatus = _solver.generate_cubes(_depth);
    auto cubes = cubesWithStatus.cubes;
    auto status = cubesWithStatus.status;

    // Check if formula was already solved
    if (status) {
        parseStatus(status);
        return false;
    }

    // Check if generate_cubes finished because of an interrupt
    if (_isInterrupted.load()) {
        return false;
    }

    // For some reason cadical may return 0 on a call to lookahead (used in generate_cubes) signaling a solved formula but does not change its state.
    // In this case the returnted cubes vector either is empty or contains zeros.
    //
    // It is empty for the formula Problem14_label20_true-unreach-call.c.cnf
    // It contains zeros for the formula satcoin-genesis-SAT-3.cnf.
    //
    // Because of this we check for a too small cube array or zeros in the cubes and in this case we start to solve here, expecting it to return instantaneously.

    // TODO Check if this assumption holds

    // Check if cubes vector is too small
    if (cubes.size() != pow(2, _depth)) {
        auto result = _solver.solve();
        parseStatus(result);
        return false;
    }

    // Check if there are zeros in the cubes
    // We only check the first cube, because the cubes consist of the permutations of the negations of the same literals.
    for (auto lit : cubes.at(0)) {
        if (lit == 0) {
            auto result = _solver.solve();
            parseStatus(result);
            return false;
        }
    }

    // Insert cubes into _root_cubes
    for (auto cube_vec : cubes) {
        _root_cubes.emplace_back(cube_vec);
    }

    return true;
}

void CubeRoot::interrupt() {
    // This should interrupt CaDiCaL::Solver::generate_cubes() since the terminator is referenced in the lookahead.cpp in the function terminating_asked()
    _isInterrupted.store(true);
}

void CubeRoot::parseStatus(int status) {
    if (status == 10) {
        _result = SatResult::SAT;
    } else if (status == 20) {
        _result = SatResult::UNSAT;
    }
}

void CubeRoot::handleMessage(int source, JobMessage &msg) {
    const std::lock_guard<Mutex> lock(_root_cubes_lock);

    if (_root_cubes.empty()) {
        return;
    }

    if (msg.tag == MSG_REQUEST_CUBES) {
        auto prepared_cubes = prepareCubes(source);
        auto serialized_cubes = serializeCubes(prepared_cubes);
        _cube_comm.sendCubes(source, serialized_cubes);

    } else if (msg.tag == MSG_RETURN_FAILED_CUBES) {
        auto serialized_failed_cubes = msg.payload;
        auto failed_cubes = unserializeCubes(serialized_failed_cubes);
        digestFailedCubes(failed_cubes);

        // Signal failed cubes were digested
        _cube_comm.receivedFailedCubes(source);
    }
}

std::vector<Cube> CubeRoot::prepareCubes(int target) {
    assert(_root_cubes.size() > 0);

    // Guarantees that we do not use an iterator past end()
    size_t distance_to_end = std::distance(_root_cubes.begin(), _root_cubes.end());
    size_t distance = std::min(distance_to_end, _cubes_per_worker);

    std::vector<Cube>::iterator begin = _root_cubes.begin();
    std::vector<Cube>::iterator end = _root_cubes.begin() + distance;

    // Assign all cubes to target
    for (auto it = begin; it != end; it++) {
        it->assign(target);
    }

    std::vector<Cube> prepared_cubes(begin, end);

    // Move used cubes to back in root cubes
    std::rotate(begin, end, _root_cubes.end());

    return prepared_cubes;
}

void CubeRoot::digestFailedCubes(std::vector<Cube> &failed_cubes) {
    for (auto failed_cube : failed_cubes) {
        // Erases all occurences of given cube
        // Behavior is defined if there are no occurences
        // https://stackoverflow.com/questions/24011627/erasing-using-iterator-from-find-or-remove
        _root_cubes.erase(std::remove(_root_cubes.begin(), _root_cubes.end(), failed_cube), _root_cubes.end());
    }

    // Check for UNSAT
    if (_root_cubes.empty()) {
        _result = UNSAT;
    }
}