
#ifndef DOMPASCH_MALLOB_SOLVER_STATISTICS_HPP
#define DOMPASCH_MALLOB_SOLVER_STATISTICS_HPP

#include <string>

#include "app/sat/hordesat/sharing/clause_histogram.hpp"

struct SolvingStatistics {

	unsigned long propagations = 0;
	unsigned long decisions = 0;
	unsigned long conflicts = 0;
	unsigned long restarts = 0;
	double memPeak = 0;
	
	// clause export
	ClauseHistogram* histProduced;
	unsigned long producedClauses = 0;
	unsigned long producedClausesFiltered = 0;
	unsigned long producedClausesAdmitted = 0;
	unsigned long producedClausesDropped = 0;

	// clause import
	ClauseHistogram* histDigested;
	unsigned long receivedClauses = 0;
	unsigned long receivedClausesFiltered = 0;
	unsigned long receivedClausesDigested = 0;
	unsigned long receivedClausesDropped = 0;

	std::string getReport() const {
		return "pps:" + std::to_string(propagations)
			+ " dcs:" + std::to_string(decisions)
			+ " cfs:" + std::to_string(conflicts)
			+ " rst:" + std::to_string(restarts)
			+ " prod:" + std::to_string(producedClauses)
			+ " (flt:" + std::to_string(producedClausesFiltered)
			+ " adm:" + std::to_string(producedClausesAdmitted)
			+ " drp:" + std::to_string(producedClausesDropped)
			+ ") recv:" + std::to_string(receivedClauses)
			+ " (flt:" + std::to_string(receivedClausesFiltered)
			+ " digd:" + std::to_string(receivedClausesDigested)
			+ " drp:" + std::to_string(receivedClausesDropped)
			+ ")";
	}

	void aggregate(const SolvingStatistics& other) {
		propagations += other.propagations;
		decisions += other.decisions;
		conflicts += other.conflicts;
		restarts += other.restarts;
		memPeak += other.memPeak;
		producedClauses += other.producedClauses;
		producedClausesAdmitted += other.producedClausesAdmitted;
		producedClausesFiltered += other.producedClausesFiltered;
		producedClausesDropped += other.producedClausesDropped;
		receivedClauses += other.receivedClauses;
		receivedClausesFiltered += other.receivedClausesFiltered;
		receivedClausesDigested += other.receivedClausesDigested;
		receivedClausesDropped += other.receivedClausesDropped;
	}
};

#endif
