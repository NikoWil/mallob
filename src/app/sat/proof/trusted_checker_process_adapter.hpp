
#pragma once

#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include "trusted/trusted_utils.hpp"
#include "trusted/trusted_checker_process.hpp"
#include "util/logger.hpp"
#include "util/params.hpp"
#include "util/sys/fileutils.hpp"
#include "util/sys/proc.hpp"
#include "util/sys/process.hpp"
#include "util/sys/subprocess.hpp"

class TrustedCheckerProcessAdapter {

private:
    Logger& _logger;
    std::string _path_directives;
    std::string _path_feedback;
    FILE* _f_directives;
    FILE* _f_feedback;
    Subprocess* _subproc;
    pid_t _child_pid {-1};

    const int _nb_vars;

    // buffering
    int _buf_lits[TRUSTED_CHK_MAX_BUF_SIZE];
    int _buflen_lits {0};

public:
    TrustedCheckerProcessAdapter(Logger& logger, int solverId, int nbVars) :
            _logger(logger), _nb_vars(nbVars) {
        auto basePath = "/tmp/mallob." + std::to_string(Proc::getPid()) + ".slv" 
            + std::to_string(solverId) + ".ts.";
        _path_directives = basePath + "directives";
        _path_feedback = basePath + "feedback";
        mkfifo(_path_directives.c_str(), 0666);
        mkfifo(_path_feedback.c_str(), 0666);

        Parameters params;
        params.fifoDirectives.set(_path_directives);
        params.fifoFeedback.set(_path_feedback);
        _subproc = new Subprocess(params, "trusted_checker_process");
        _child_pid = _subproc->start();

        _f_directives = fopen(_path_directives.c_str(), "w");
        _f_feedback = fopen(_path_feedback.c_str(), "r");
    }

    ~TrustedCheckerProcessAdapter() {
        if (_child_pid != -1) terminate();
        fclose(_f_feedback);
        fclose(_f_directives);
        FileUtils::rm(_path_feedback);
        FileUtils::rm(_path_directives);
        delete _subproc;
    }

    void init(const u8* formulaSignature) {

        writeDirectiveType(TRUSTED_CHK_INIT);
        TrustedUtils::writeInt(_nb_vars, _f_directives);
        TrustedUtils::writeSignature(formulaSignature, _f_directives);
        if (!awaitResponse()) TrustedUtils::doAbort();
    }

    inline void load(const int* fData, size_t fSize) {
        assert(_buflen_lits == 0);
        size_t offset = 0;
        while (offset < fSize) {
            const auto nbInts = std::min(fSize-offset, (size_t)TRUSTED_CHK_MAX_BUF_SIZE);
            memcpy(_buf_lits, fData+offset, nbInts*sizeof(int));
            _buflen_lits = nbInts;
            flushLiteralBuffer();
            offset += nbInts;
        }
        assert(offset == fSize);
    }

    inline void loadLiteral(int lit) {

        _buf_lits[_buflen_lits++] = lit;
        if (_buflen_lits+1 == TRUSTED_CHK_MAX_BUF_SIZE) {
            // flush buffer
            flushLiteralBuffer();
        }
    }

    inline bool endLoading() {

        if (_buflen_lits > 0) flushLiteralBuffer();
        writeDirectiveType(TRUSTED_CHK_END_LOAD);
        if (!awaitResponse()) TrustedUtils::doAbort();
        return true;
    }

    inline bool produceClause(unsigned long id, const int* literals, int nbLiterals,
        const unsigned long* hints, int nbHints,
        uint8_t* outSignatureOrNull, int& inOutSigSize) {

        if (_buflen_lits > 0) flushLiteralBuffer();
        writeDirectiveType(TRUSTED_CHK_CLS_PRODUCE);
        const int totalSize = 2 + nbLiterals + 1 + 2*nbHints;
        TrustedUtils::writeInt(totalSize, _f_directives);
        TrustedUtils::writeUnsignedLong(id, _f_directives);
        for (size_t i = 0; i < nbLiterals; i++)
            TrustedUtils::writeInt(literals[i], _f_directives);
        TrustedUtils::writeInt(0, _f_directives);
        for (size_t i = 0; i < nbHints; i++)
            TrustedUtils::writeUnsignedLong(hints[i], _f_directives);

        if (!awaitResponse()) TrustedUtils::doAbort();
        if (inOutSigSize < 16) TrustedUtils::doAbort();
        TrustedUtils::readSignature(outSignatureOrNull, _f_feedback);
        inOutSigSize = 16;
        return true;
    }

    inline bool importClause(unsigned long id, const int* literals, int nbLiterals,
        const uint8_t* signatureData, int signatureSize) {

        if (_buflen_lits > 0) flushLiteralBuffer();
        writeDirectiveType(TRUSTED_CHK_CLS_IMPORT);
        const int totalSize = 2 + nbLiterals + 1 + 4;
        TrustedUtils::writeInt(totalSize, _f_directives);
        TrustedUtils::writeUnsignedLong(id, _f_directives);
        for (size_t i = 0; i < nbLiterals; i++)
            TrustedUtils::writeInt(literals[i], _f_directives);
        TrustedUtils::writeInt(0, _f_directives);
        TrustedUtils::writeSignature(signatureData, _f_directives);

        if (!awaitResponse()) TrustedUtils::doAbort();
        return true;
    }

    inline bool deleteClauses(const unsigned long* ids, int nbIds) {

        if (_buflen_lits > 0) flushLiteralBuffer();
        writeDirectiveType(TRUSTED_CHK_CLS_DELETE);
        const int totalSize = 2 * nbIds;
        TrustedUtils::writeInt(totalSize, _f_directives);
        for (size_t i = 0; i < nbIds; i++) TrustedUtils::writeUnsignedLong(ids[i], _f_directives);

        if (!awaitResponse()) TrustedUtils::doAbort();
        return true;
    }

    inline bool validateUnsat() {

        if (_buflen_lits > 0) flushLiteralBuffer();
        writeDirectiveType(TRUSTED_CHK_VALIDATE);

        if (!awaitResponse()) TrustedUtils::doAbort();
        u8 sig[16];
        TrustedUtils::readSignature(sig, _f_feedback);
        auto str = Logger::dataToHexStr(sig, 16);
        LOGGER(_logger, V2_INFO, "TRUSTED checker reported UNSAT - sig %s\n", str.c_str());
        return true;
    }

    void terminate() {
        if (_child_pid == -1) return;
        Process::sendSignal(_child_pid, SIGKILL);
        while (Process::didChildExit(_child_pid) == 0) usleep(1000);
        _child_pid = -1;
    }

private:
    void flushLiteralBuffer() {
        writeDirectiveType(TRUSTED_CHK_LOAD);
        TrustedUtils::writeInt(_buflen_lits, _f_directives);
        TrustedUtils::writeInts(_buf_lits, _buflen_lits, _f_directives);
        _buflen_lits = 0;
    }

    void writeDirectiveType(char type) {
        TrustedUtils::writeChar(type, _f_directives);
    }
    bool awaitResponse() {
        UNLOCKED_IO(fflush)(_f_directives);
        int res = TrustedUtils::readChar(_f_feedback);
        return (char)res == TRUSTED_CHK_RES_ACCEPT;
    }
};
