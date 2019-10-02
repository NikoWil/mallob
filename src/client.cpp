
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <thread>

#include "mpi.h"
#include "client.h"
#include "permutation.h"
#include "job_transfer.h"

void readAllInstances(Client* client) {

    MyMpi::log("Started client I/O thread to read instances.");

    Client& c = *client;
    for (auto it = c.jobsByArrival.begin(); it != c.jobsByArrival.end(); ++it) {

        Job &job = it->second;
        int jobId = job.getId();

        MyMpi::log("Reading \"" + c.jobInstances[jobId] + "\" (#" + std::to_string(jobId) + ") ...");
        c.readFormula(c.jobInstances[jobId], job);
        if (job.getFormulaSize() > 1)
            MyMpi::log("Read \"" + c.jobInstances[jobId] + "\" (#" + std::to_string(jobId) + ").");

        std::unique_lock<std::mutex> lock(c.jobReadyLock);
        c.jobReady[jobId] = true;
        lock.unlock();
    }
}

void Client::init() {

    int internalRank = MyMpi::rank(comm);
    std::string filename = "scenarios/scenario004_1." + std::to_string(internalRank);
    readInstanceList(filename);
    MyMpi::log("Started client main thread to introduce instances.");

    instanceReaderThread = std::thread(readAllInstances, this);
}

Client::~Client() {
    instanceReaderThread.join();
}

void Client::mainProgram() {

    //std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
    for (auto it = jobsByArrival.begin(); it != jobsByArrival.end(); ++it) {
        float arrival = it->first;
        int timeMillis = (int) (1000 * (arrival - elapsed_time()));
        std::this_thread::sleep_for(std::chrono::milliseconds(timeMillis));

        Job& job = it->second;
        int jobId = job.getId();

        // Wait until job is ready to be sent
        while (true) {
            std::unique_lock<std::mutex> lock(jobReadyLock);
            if (jobReady[jobId])
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        if (job.getFormulaSize() == 1) {
            // Some I/O error kept the instance from being read
            MyMpi::log("Skipping job #" + std::to_string(jobId) + " due to previous I/O error");
            continue;
        }

        // Find the job's canonical initial node
        int n = MyMpi::size(MPI_COMM_WORLD) - MyMpi::size(comm);
        MyMpi::log("Creating permutation of size " + std::to_string(n) + " ...");
        AdjustablePermutation p(n, jobId);
        int nodeRank = p.get(0);

        const JobSignature sig(jobId, nodeRank, job.getFormulaSize(), job.getAssumptionsSize());

        MyMpi::log_send("Introducing job #" + std::to_string(jobId), nodeRank);
        MyMpi::send(MPI_COMM_WORLD, nodeRank, MSG_INTRODUCE_JOB, sig);
        MyMpi::log_send("Sending job #" + std::to_string(jobId), nodeRank);
        MyMpi::send(MPI_COMM_WORLD, nodeRank, MSG_SEND_JOB, job);
    }
}

void Client::readInstanceList(std::string& filename) {

    MyMpi::log("Reading instances from file " + filename);
    std::fstream file;
    file.open(filename, std::ios::in);
    if (file.is_open()) {
        std::string line;
        while(std::getline(file, line)) {
            if (line.substr(0, 1) == std::string("#")) {
                continue;
            }
            int id; float arrival; float priority; std::string instanceFilename;
            int pos = 0, next = 0;
            next = line.find(" "); id = std::stoi(line.substr(pos, next-pos)); line = line.substr(next+1);
            next = line.find(" "); arrival = std::stof(line.substr(pos, next-pos)); line = line.substr(next+1);
            next = line.find(" "); priority = std::stof(line.substr(pos, next-pos)); line = line.substr(next+1);
            next = line.find(" "); instanceFilename = line.substr(pos, next-pos); line = line.substr(next+1);
            Job job(id, priority);
            while (jobsByArrival.count(arrival)) {
                arrival += 0.00001f;
            }
            jobsByArrival[arrival] = job;
            jobInstances[id] = instanceFilename;
        }
        file.close();
    }
    MyMpi::log("Read " + std::to_string(jobsByArrival.size()) + " job instances from file " + filename);
}

void Client::readFormula(std::string& filename, Job& job) {

    MyMpi::log("Reading instance " + filename);
    std::fstream file;
    file.open(filename, std::ios::in);
    if (file.is_open()) {

        std::vector<int> formula;
        std::string line;
        while(std::getline(file, line)) {
            if (line.substr(0, 1) == std::string("c") || line.substr(0, 1) == std::string("p")) {
                continue;
            }
            int next = 0; int pos = 0;
            while (true) {
                next = line.find(" ", pos);
                int lit;
                if (next == std::string::npos)
                    lit = 0;
                else
                    lit = std::stoi(line.substr(pos, pos+next));
                formula.push_back(lit);
                if (next == std::string::npos)
                    break;
                pos = next+1;
            }
        }
        job.setFormula(formula);

        MyMpi::log("Read instance " + filename + ".");
    } else {
        MyMpi::log("ERROR: File " + filename + " could not be opened. Skipping job #"
                   + std::to_string(job.getId()));
    }
}
