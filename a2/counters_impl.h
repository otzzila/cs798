#ifndef COUNTERS_IMPL_H
#define COUNTERS_IMPL_H

#include <atomic>
#include <mutex>

#define MAX_THREADS 256

class CounterNaive {
private:
    char padding0[64];
    int64_t v;
    char padding1[64];
public:
    CounterNaive(int _numThreads) : v(0) {}
    int64_t inc(int tid) {
        return v++;
    }
    int64_t read() {
        return v;
    }
};

class CounterLocked {
private:
    char padding0[64];
    int64_t v;
    char padding1[64];
    std::mutex m;
public:
    CounterLocked(int _numThreads) : v(0), m() {}
    int64_t inc(int tid) {
        // We lock around every operation. Otherwise the same as naive
        m.lock();
        int64_t result = v++;
        m.unlock();
        return result;
    }
    int64_t read() {
        m.lock();
        int64_t result = v;
        m.unlock();
        return result;
    }
};

class CounterFetchAndAdd {
private:
    char padding0[64];
    std::atomic<int64_t> v;
    char padding1[64];
public:
    CounterFetchAndAdd(int _numThreads) {}
    int64_t inc(int tid) {
        #ifdef TEST_WRITE
        v = v + 1;
        return v;
        #endif
        #ifndef TESTWRITE
        return v++;
        #endif
    }
    int64_t read() {
        return v;
    }
};

struct padded_int64_t {
    atomic<int64_t> v;
    volatile char padding[64-sizeof(atomic<int64_t>)];
};

class CounterApproximate {
private:
    static const int64_t c = 8; // error constant
    char padding0[64];
    atomic<int64_t> approx_value;
    char padding1[64];
    padded_int64_t threadScratch[MAX_THREADS]; // local thread counts and padding
    char padding2[64];
    int numThreads;
    char padding3[64];
public:
    CounterApproximate(int _numThreads) : approx_value(0), numThreads(_numThreads) {
    }

    int64_t inc(int tid) {
        // Take current thread's value and add one.
        int64_t current_value = ++threadScratch[tid].v;
        if (current_value >= c * numThreads){
            // atomically add c*numThreads
            std::atomic_fetch_add(&approx_value, c*numThreads);
            threadScratch[tid].v = 0; // reset this thread's count to 0 
        }
        return 0;
    }
    int64_t read() {
        return approx_value;
    }
};


class CounterShardedLocked {
private:
    char padding0[64];
    std::mutex mutexes[MAX_THREADS];
    char padding1[64];
    padded_int64_t threadScratch[MAX_THREADS]; // local thread counts and padding
    char padding2[64];
    int numThreads;
    char padding3[64];
public:
    CounterShardedLocked(int _numThreads) : numThreads(_numThreads) {
    }
    int64_t inc(int tid) {
        // Aquire our own thread and then increment
        mutexes[tid].lock();
        threadScratch[tid].v++;
        mutexes[tid].unlock();
        return 0;
    }
    int64_t read() {
        // Lock every counter
        for (int i = 0; i < numThreads; ++i){
            mutexes[i].lock();
        }
        // Sum the counters
        int64_t total = 0;
        for (int i = 0; i < numThreads; ++i){
            total += threadScratch[i].v;
        }
        // Free the locks
        for (int i = 0; i < numThreads; ++i){
            mutexes[i].unlock();
        }
        return total;
    }
};


class CounterShardedWaitfree {
private:
    char padding1[64];
    padded_int64_t * threadScratch; // local thread counts and padding
    char padding2[64];
    int numThreads;
    char padding3[64];
public:
    CounterShardedWaitfree(int _numThreads) : numThreads(_numThreads) {
        threadScratch = new padded_int64_t[numThreads];
    }
    ~CounterShardedWaitfree() {
        delete [] threadScratch;
    }
    int64_t inc(int tid) {
        threadScratch[tid].v++;
        return 0;
    }
    int64_t read() {
        // We can just sum these at whatever time we get to them
        int64_t total = 0;
        for (int i = 0; i < numThreads; ++i){
            total += threadScratch[i].v;
        }
        return total;
    }
};


#endif