#include <thread>
#include <mutex>
#include <chrono>
#include <vector>
#include <iostream>


using namespace std;

constexpr int THREAD_COUNT = 1900;
constexpr int OP_COUNT = 1000;
constexpr int MIN_SLEEP_US = 0;
constexpr int MAX_SLEEP_US = 100;

template <typename T>
struct IncOp {
    static T process(T val) {
        using namespace std::chrono_literals;
        int t = (rand() % (MAX_SLEEP_US - MIN_SLEEP_US)) + MIN_SLEEP_US;

        this_thread::sleep_for(1us * t);
        return val + 1;
    }
};

template <typename T>
struct NumberLimitChecker {
    static bool check_done(T val) {
        return val >= OP_COUNT;
    }
};


template <typename StatelessOp, typename StopChecker>
class PessimisticLock {
public:
    PessimisticLock() :val(0) {}

    bool inc() {
        unique_lock<mutex> l(lock);
        if (StopChecker::check_done(val)) {
            return true;
        }
        val = StatelessOp::process(val);
        return false;
    }
    int get_val() const {
        return this->val;
    }
private:
    int val;
    mutex lock;
};


template <typename StatelessOp, typename StopChecker>
class OptimisticLock {
public:
    OptimisticLock() :val(0) {}

    bool inc() {
        while (true) {
            int val_prev = val;
            if (StopChecker::check_done(val)) {
                return true;
            }
            int val1 = StatelessOp::process(val);
            {
                unique_lock<mutex> l(lock);
                if (val == val_prev) {
                    val = val1;
                    return false;
                }
            }
        }
    }
    int get_val() const {
        return this->val;
    }

private:
    int val;
    mutex lock;
};

int main() {
    auto seed = static_cast<unsigned int>(time(nullptr));
    srand(seed);
    cout << "seed: " << seed << endl;
    cout << "thread_count: " << THREAD_COUNT << endl;
    cout << "total_count: " << OP_COUNT << endl;
    cout << "sleep_us: [" << MIN_SLEEP_US << ", " << MAX_SLEEP_US << ")" << endl;

    PessimisticLock<IncOp<int>, NumberLimitChecker<int>> l;
    vector<thread> threads1;
    auto t1 = chrono::system_clock::now();
    for (int j = 0; j < THREAD_COUNT; ++j) {
        threads1.push_back(move(thread([&l]() -> void {
            while (true) {
                if (l.inc())
                    break;
            }
        })));
    }
    for (int j = 0; j < THREAD_COUNT; ++j) {
        threads1[j].join();
    }
    auto t2 = chrono::system_clock::now();
    cout << "Pessimistic: result " << l.get_val() << " t: " << chrono::duration_cast<chrono::microseconds>(t2 - t1).count() << "us" << endl;

    OptimisticLock<IncOp<int>, NumberLimitChecker<int>> ol;
    vector<thread> threads2;
    t1 = chrono::system_clock::now();
    for (int j = 0; j < THREAD_COUNT; ++j) {
        threads2.push_back(move(thread([&ol]() -> void {
            while (true) {
                if (ol.inc())
                    break;
            }
        })));
    }
    for (int j = 0; j < THREAD_COUNT; ++j) {
        threads2[j].join();
    }
    t2 = chrono::system_clock::now();
    cout << "Optimistic:  result " << ol.get_val() << " t: " << chrono::duration_cast<chrono::microseconds>(t2 - t1).count() << "us" << endl;

    return 0;
}
