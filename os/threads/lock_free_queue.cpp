#include <chrono>
#include <thread>
#include <iostream>
#include <vector>
#include <cstring>
#include <mutex>
#include <condition_variable>


using namespace std;
constexpr int THREAD_COUNT = 1900;
constexpr int OP_COUNT = 1000;
constexpr int QUEUE_CAP = OP_COUNT * 2;
constexpr int DONE = 0;

class LFIntQueue {
public:
    LFIntQueue() :currentHead(0), currentTail(0) {
        memset(data, 0, sizeof(int) * QUEUE_CAP);
    }
    void enq(int val) {

    }
    int deq() {
        // check empty
        // return
    }
    bool empty() const {

    }
private:
    int data[QUEUE_CAP] = {0};
    int currentHead;
    int currentTail;
};


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
        return currentHead;
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
int testQueue(string name) {
    auto seed = static_cast<unsigned int>(time(nullptr));
    srand(seed);
    cout << "seed: " << seed << endl;
    cout << "thread_count: " << THREAD_COUNT << endl;
    cout << "total_count: " << OP_COUNT << endl;

    Q q;
    int dataOut[OP_COUNT];
    memset(dataOut, 0, sizeof(int) * OP_COUNT);

    vector<thread> consumers;
    for (int j = 0; j < THREAD_COUNT; ++j) {
        consumers.push_back(move(thread([j, &dataOut, &q]() -> void {
            while (true) {
                try {
                    int val = q.deq();
                    dataOut[val] = val;
                }
                catch (int i) {
                    // DONE
                    if (i == DONE)
                        break;
                }
            }
        })));
    }

    for (int i = 0; i < OP_COUNT; ++i) {
        q.enq(i);
    }
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
    cout << "Concurrent test for \"" + name + "\" completed" << endl;
    return 0;
}

int main () {
//    testQueue<LFIntQueue>("Lock Free");
    testQueue<LockIntQueue>("Mutex + Conditional Variable");
    return 0;
}
