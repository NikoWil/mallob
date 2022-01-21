
#ifndef DOMPASCH_MALLOB_MESSAGE_QUEUE_HPP
#define DOMPASCH_MALLOB_MESSAGE_QUEUE_HPP

#include "comm/message_handle.hpp"

#include <list>
#include <cmath>
#include "util/assert.hpp"
#include <unistd.h>

#include "util/hashing.hpp"
#include "util/sys/background_worker.hpp"
#include "util/logger.hpp"
#include "comm/msgtags.h"
#include "util/ringbuffer.hpp"
#include "util/sys/atomics.hpp"

typedef std::shared_ptr<std::vector<uint8_t>> DataPtr;
typedef std::shared_ptr<const std::vector<uint8_t>> ConstDataPtr; 

class MessageQueue {
    
private:
    struct ReceiveFragment {
        
        int source = -1;
        int id = -1;
        int tag = -1;
        int receivedFragments = 0;
        std::vector<DataPtr> dataFragments;
        
        ReceiveFragment() = default;
        ReceiveFragment(int source, int id, int tag) : source(source), id(id), tag(tag) {}

        ReceiveFragment(ReceiveFragment&& moved) {
            source = moved.source;
            id = moved.id;
            tag = moved.tag;
            receivedFragments = moved.receivedFragments;
            dataFragments = std::move(moved.dataFragments);
            moved.id = -1;
        }
        ReceiveFragment& operator=(ReceiveFragment&& moved) {
            source = moved.source;
            id = moved.id;
            tag = moved.tag;
            receivedFragments = moved.receivedFragments;
            dataFragments = std::move(moved.dataFragments);
            moved.id = -1;
            return *this;
        }

        bool valid() {return id != -1;}

        static int readId(uint8_t* data, int msglen) {
            return * (int*) (data+msglen - 3*sizeof(int));
        }

        void receiveNext(int source, int tag, uint8_t* data, int msglen) {
            assert(this->source >= 0);
            assert(valid());

            int id, sentBatch, totalNumBatches;
            // Read meta data from end of message
            memcpy(&id,              data+msglen - 3*sizeof(int), sizeof(int));
            memcpy(&sentBatch,       data+msglen - 2*sizeof(int), sizeof(int));
            memcpy(&totalNumBatches, data+msglen - 1*sizeof(int), sizeof(int));
            msglen -= 3*sizeof(int);

            log(V4_VVER, "RECVB %i %i/%i %i\n", id, sentBatch+1, totalNumBatches, source);

            // Store data in fragments structure
            
            //log(V5_DEBG, "MQ STORE (%i,%i) %i/%i\n", source, id, sentBatch, totalNumBatches);

            assert(this->source == source);
            assert(this->id == id || log_return_false("%i != %i\n", this->id, id));
            assert(this->tag == tag);
            assert(sentBatch < totalNumBatches || log_return_false("Invalid batch %i/%i!\n", sentBatch, totalNumBatches));
            if (sentBatch >= dataFragments.size()) dataFragments.resize(sentBatch+1);
            assert(receivedFragments >= 0 || log_return_false("Batched message was already completed!\n"));

            //log(V5_DEBG, "MQ STORE alloc\n");
            assert(dataFragments[sentBatch] == nullptr || log_return_false("Batch %i/%i already present!\n", sentBatch, totalNumBatches));
            dataFragments[sentBatch].reset(new std::vector<uint8_t>(data, data+msglen));
            
            //log(V5_DEBG, "MQ STORE produce\n");
            // All fragments of the message received?
            receivedFragments++;
            if (receivedFragments == totalNumBatches)
                receivedFragments = -1;
        }

        bool isFinished() {
            assert(valid());
            return receivedFragments == -1;
        }
    };

    struct SendHandle {

        int id = -1;
        int dest;
        int tag;
        MPI_Request request = MPI_REQUEST_NULL;
        DataPtr data;
        int sentBatches = -1;
        int totalNumBatches;
        int sizePerBatch;
        std::vector<uint8_t> tempStorage;
        
        SendHandle(int id, int dest, int tag, DataPtr data, int maxMsgSize) 
            : id(id), dest(dest), tag(tag), data(data) {

            sizePerBatch = maxMsgSize;
            sentBatches = 0;
            totalNumBatches = data->size() <= sizePerBatch+3*sizeof(int) ? 1 
                : std::ceil(data->size() / (float)sizePerBatch);
        }

        bool valid() {return id != -1;}
        
        SendHandle(SendHandle&& moved) {
            assert(moved.valid());
            id = moved.id;
            dest = moved.dest;
            tag = moved.tag;
            request = moved.request;
            data = std::move(moved.data);
            sentBatches = moved.sentBatches;
            totalNumBatches = moved.totalNumBatches;
            sizePerBatch = moved.sizePerBatch;
            tempStorage = std::move(moved.tempStorage);
            
            moved.id = -1;
            moved.data = DataPtr();
            moved.request = MPI_REQUEST_NULL;
        }
        SendHandle& operator=(SendHandle&& moved) {
            assert(moved.valid());
            id = moved.id;
            dest = moved.dest;
            tag = moved.tag;
            request = moved.request;
            data = std::move(moved.data);
            sentBatches = moved.sentBatches;
            totalNumBatches = moved.totalNumBatches;
            sizePerBatch = moved.sizePerBatch;
            tempStorage = std::move(moved.tempStorage);
            
            moved.id = -1;
            moved.data = DataPtr();
            moved.request = MPI_REQUEST_NULL;
            return *this;
        }

        bool test() {
            assert(valid());
            assert(request != MPI_REQUEST_NULL);
            int flag = false;
            MPI_Test(&request, &flag, MPI_STATUS_IGNORE);
            return flag;
        }

        bool isFinished() const {return sentBatches == totalNumBatches;}

        void sendNext() {
            assert(valid());
            assert(!isFinished() || log_return_false("Handle (n=%i) already finished!\n", sentBatches));
            
            if (!isBatched()) {
                // Send first and only message
                //log(V5_DEBG, "MQ SEND SINGLE id=%i\n", id);
                MPI_Isend(data->data(), data->size(), MPI_BYTE, dest, tag, MPI_COMM_WORLD, &request);
                sentBatches = 1;
                return;
            }

            size_t begin = sentBatches*sizePerBatch;
            size_t end = std::min(data->size(), (size_t)(sentBatches+1)*sizePerBatch);
            assert(end>begin || log_return_false("%ld <= %ld\n", end, begin));
            size_t msglen = (end-begin)+3*sizeof(int);
            tempStorage.resize(msglen);

            // Copy actual data
            memcpy(tempStorage.data(), data->data()+begin, end-begin);
            // Copy meta data at insertion point
            memcpy(tempStorage.data()+(end-begin), &id, sizeof(int));
            memcpy(tempStorage.data()+(end-begin)+sizeof(int), &sentBatches, sizeof(int));
            memcpy(tempStorage.data()+(end-begin)+2*sizeof(int), &totalNumBatches, sizeof(int));

            MPI_Isend(tempStorage.data(), tempStorage.size(), MPI_BYTE, dest, 
                    tag+MSG_OFFSET_BATCHED, MPI_COMM_WORLD, &request);

            
            sentBatches++;
            log(V4_VVER, "SENDB %i %i/%i %i\n", id, sentBatches, totalNumBatches, dest);
            //log(V5_DEBG, "MQ SEND BATCHED id=%i %i/%i\n", id, sentBatches, totalNumBatches);
        }

        bool isBatched() const {return totalNumBatches > 1;}
        size_t getTotalNumBatches() const {assert(isBatched()); return totalNumBatches;}
    };

    size_t _max_msg_size;
    int _my_rank;
    unsigned long long _iteration = 0;

    // Basic receive stuff
    MPI_Request _recv_request;
    uint8_t* _recv_data;
    std::list<SendHandle> _self_recv_queue;

    // Fragmented messages stuff
    robin_hood::unordered_node_map<std::pair<int, int>, ReceiveFragment, IntPairHasher> _fragmented_messages;
    Mutex _fragmented_mutex;
    ConditionVariable _fragmented_cond_var;
    std::list<ReceiveFragment> _fragmented_queue;
    std::atomic_int _num_fused = 0;
    Mutex _fused_mutex;
    std::list<MessageHandle> _fused_queue;

    // Send stuff
    std::list<SendHandle> _send_queue;
    int _running_send_id = 1;

    // Garbage collection
    Mutex _garbage_mutex;
    std::list<DataPtr> _garbage_queue;

    // Callbacks
    typedef std::function<void(MessageHandle&)> MsgCallback;
    robin_hood::unordered_map<int, MsgCallback> _callbacks;
    std::function<void(int)> _send_done_callback = [](int) {};

    BackgroundWorker _batch_assembler;
    BackgroundWorker _gc;

public:
    MessageQueue(int maxMsgSize);
    ~MessageQueue();

    void registerCallback(int tag, const MsgCallback& cb);
    void registerSentCallback(std::function<void(int)> callback);
    void clearCallbacks();

    int send(DataPtr data, int dest, int tag);
    void advance();

private:
    void runFragmentedMessageAssembler();
    void runGarbageCollector();

    void processReceived();
    void processSelfReceived();
    void processAssembledReceived();
    void processSent();
};

#endif
