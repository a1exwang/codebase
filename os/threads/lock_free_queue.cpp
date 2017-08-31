#include <chrono>
#include <thread>
#include <iostream>
#include <vector>
#include <cstring>
#include <mutex>
#include <condition_variable>
#include <atomic>


using namespace std;
constexpr int THREAD_COUNT = 200;
constexpr int OP_COUNT = 1000;
constexpr int QUEUE_CAP = OP_COUNT * 2;
constexpr int DONE = 0;

static std::mutex mtx_cout;

// Asynchronous output
struct sync_cout {
    std::unique_lock<std::mutex> lk;
    sync_cout() :lk(std::unique_lock<std::mutex>(mtx_cout)) { }

    template<typename T>
    sync_cout& operator<<(const T& _t) {
        std::cout << _t;
        return *this;
    }

    sync_cout& operator<<(std::ostream& (*fp)(std::ostream&)) {
        std::cout << fp;
        return *this;
    }
};


class SingleProducerLFQueue {
public:
    SingleProducerLFQueue() :globalHead(0), globalTail(0) {
        memset(data, 0, sizeof(int) * QUEUE_CAP);
    }
    void enq(int val) {
        // Notice that only one thread will access this code.
        // It is safe without locks.
        data[globalTail] = val;
        globalTail++;
//        sync_cout() << "E " << globalTail << endl;
    }
    int deq() {
        while (true) {
            int localHead = globalHead;
            // test empty
            if (globalTail == localHead) {
                emptyCount++;
                if (this->done) {
                    throw 0;
                }
                continue;
            }
            if (globalHead.compare_exchange_weak(localHead, localHead + 1)) {
                cmpxchgCount++;
                return data[localHead];
            }
            else {
                cmpxchgCount++;
                cmpxchgFailed++;
            }
        }
    }
    bool empty() const {
        return globalTail == globalHead;
    }
    void setDone() {
        // Notice that only one thread will access this code.
        // It is safe without locks.
        done = true;
    }
    bool isDone() const {
        return done;
    }
    static void init() {
        cmpxchgFailed.store(0);
        cmpxchgCount.store(0);
        emptyCount.store(0);
    }
    static void deinit() {
        sync_cout() << "cmpxchgFailed:\t" << SingleProducerLFQueue::cmpxchgFailed << endl <<
                    "cmpxchgSuccess:\t" << SingleProducerLFQueue::cmpxchgCount - SingleProducerLFQueue::cmpxchgFailed << endl <<
                    "emptyQueueDequeCount: \t" <<  SingleProducerLFQueue::emptyCount << endl;
    }
private:
    bool done = false;
    int data[QUEUE_CAP] = {0};
    atomic<int> globalHead;
    int globalTail;

    static atomic<int> cmpxchgFailed;
    static atomic<int> cmpxchgCount;
    static atomic<int> emptyCount;
};
atomic<int> SingleProducerLFQueue::cmpxchgFailed;
atomic<int> SingleProducerLFQueue::cmpxchgCount;
atomic<int> SingleProducerLFQueue::emptyCount;


class LockIntQueue {
public:
    LockIntQueue() : data(), currentHead(0), currentTail(0), lock() {
        memset(data, 0, sizeof(int) * QUEUE_CAP);
    }
    void enq(int val) {
        unique_lock<mutex> _l(lock);
        data[currentTail] = val;
        currentTail++;
        cv.notify_one();
    }
    int deq() {
        // check empty
        // return
        unique_lock<mutex> _l(lock);
        while (empty()) {
            if (this->done) {
                throw 0;
            }
            cv.wait(_l);
        }
        int ret = data[currentHead];
        currentHead++;
        return ret;
    }
    bool empty() const {
        return currentTail == currentHead;
    }
    void setDone() {
        unique_lock<mutex> _l(lock);
        done = true;
        cv.notify_all();
    }
    bool isDone() const {
        return done;
    }

private:
    int data[QUEUE_CAP] = {0};
    bool done = false;
    int currentHead;
    int currentTail;
    mutex lock;
    condition_variable cv;
};


template <typename Q>
int testSingleConsumerQueue(string name) {
    auto seed = static_cast<unsigned int>(time(nullptr));
    srand(seed);
    sync_cout() << "seed: " << seed << endl;
    sync_cout() << "thread_count: " << THREAD_COUNT << endl;
    sync_cout() << "total_count: " << OP_COUNT << endl;

    Q q;
    int dataOut[OP_COUNT];
    memset(dataOut, 0, sizeof(int) * OP_COUNT);

    vector<thread> consumers;
    mutex m;
    auto _t1 = chrono::system_clock::now();
    decltype(_t1 - _t1) totalGlobalTime = 0s;
    for (int j = 0; j < THREAD_COUNT; ++j) {
        consumers.push_back(move(thread([&totalGlobalTime, &m, j, &dataOut, &q]() -> void {
            decltype(_t1 - _t1) totalTime = 0s;
            while (true) {
                try {
                    auto t1 = chrono::system_clock::now();
                    int val = q.deq();
                    auto t2 = chrono::system_clock::now();
                    auto delta = t2 - t1;
                    totalTime += delta;
                    dataOut[val] = val;
                }
                catch (int i) {
                    // DONE
                    if (i == DONE)
                        break;
                }
            }
            {
                m.lock();
                totalGlobalTime += totalTime;
                m.unlock();
            }
        })));
    }

    decltype(_t1 - _t1) totalTime = 0s;
    for (int i = 0; i < OP_COUNT; ++i) {
        auto t1 = chrono::system_clock::now();
        q.enq(i);
        auto t2 = chrono::system_clock::now();
        totalTime += t2 - t1;
    }
    sync_cout() << "Total enq latency: " << chrono::duration_cast<chrono::nanoseconds>(totalTime).count()/OP_COUNT << "ns" << endl;
    q.setDone();

    for (int j = 0; j < THREAD_COUNT; ++j) {
        consumers[j].join();
    }
    for (int j = 0; j < OP_COUNT; ++j) {
        if (dataOut[j] != j) {
            cerr << "Wrong dataOut" << endl;
            return -1;
        }
    }
    totalGlobalTime += totalTime;
    sync_cout() << "Average deq latency: "
                << (static_cast<double>(totalGlobalTime.count()) / OP_COUNT) << "ns" << endl;
    sync_cout() << "Concurrent test for \"" + name + "\" completed" << endl << endl;
    return 0;
}

int main () {
    SingleProducerLFQueue::init();
    testSingleConsumerQueue<LockIntQueue>("Mutex + Conditional Variable");
    testSingleConsumerQueue<SingleProducerLFQueue>("Lock Free");
    SingleProducerLFQueue::deinit();
    return 0;
}
