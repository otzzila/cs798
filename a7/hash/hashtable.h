#pragma once
#include <immintrin.h>
#include <atomic>
#include "util.h"
#include "tle.h"
using namespace std;

class TLEHashTableExpand {
private:
    enum {
        TOMBSTONE = (int) 0x7FFFFFFF,       // largest possible key is this minus one
        EMPTY = (int) 0                     // smallest possible key is this plus one
    };

    char padding0[PADDING_BYTES];
    ElapsedTimer debugTimer;                // just for debugging
    int numThreads;
    volatile int * data;
    volatile int * old;
    int64_t capacity;
    int64_t oldCapacity;
    counter * approxInserts;                // only create ONCE in the constructor, then use set() to reset its value if needed
    counter * approxDeletes;                // only create ONCE in the constructor, then use set() to reset its value if needed
    char padding1[PADDING_BYTES];
    volatile uint64_t version;
    char padding1[PADDING_BYTES];

    bool isExpandNeeded(const int tid, int64_t probeCount);
    void expand(const int tid);
    int64_t getAccurateSize();
    void migrateInsert(const int & key);

public:
    TLEHashTableExpand(const int _numThreads, const int64_t _capacity);
    ~TLEHashTableExpand();
    bool insertIfAbsent(const int tid, const int & key);
    bool erase(const int tid, const int & key);
    long getSumOfKeys();
    void printDebuggingDetails();
};

// _capacity is the INITIAL size of the hash table (maximum number of elements it can contain WITHOUT expansion)
TLEHashTableExpand::TLEHashTableExpand(const int _numThreads, const int64_t _capacity) {
    numThreads = _numThreads;
    capacity = _capacity;
    data = new volatile int[capacity];
    for (int64_t i=0;i<capacity;++i) data[i] = EMPTY;
    approxInserts = new counter(numThreads);
    approxDeletes = new counter(numThreads);
    old = NULL;
    oldCapacity = 0;
    version = 0;
    debugTimer.startTimer();
}

TLEHashTableExpand::~TLEHashTableExpand() {
    delete[] data;
    delete[] old;
    delete approxInserts;
    delete approxDeletes;
}

int64_t TLEHashTableExpand::getAccurateSize() {
    int64_t accurateInserts = approxInserts->getAccurate();
    int64_t accurateDeletes = approxDeletes->getAccurate();
    return accurateInserts - accurateDeletes;
}

bool TLEHashTableExpand::isExpandNeeded(const int tid, int64_t probeCount) {
    return ((approxInserts->get() > capacity/3) ||
            (probeCount > 100 && approxInserts->getAccurate() > capacity/3));
}

void TLEHashTableExpand::expand(const int tid) {
    int64_t expansionStartTime = debugTimer.getElapsedMillis();

    // EXPANSION CODE HERE :)
    int64_t accurateSize = getAccurateSize();

    version += 1;

    delete[] old;
    old = data;

    TRACE {cout << "Expanding" << endl;}
    TRACE {PRINT(capacity); }
    oldCapacity = capacity;
    capacity = max(max(accurateSize, int64_t(1)) * 4, oldCapacity);
    TRACE {PRINT(capacity); }

    data = new volatile int [capacity];
    for (int64_t i=0;i<capacity;++i) data[i] = EMPTY;

    // Now move over old values
    #pragma omp parallel for
    for (int64_t i=0;i<oldCapacity;++i){
        int key = old[i];
        if (key != EMPTY && key != TOMBSTONE){
            migrateInsert(key);
        }
    }

    // suggested adjustment to counters at the end of expansion:
    approxInserts->set(accurateSize);
    approxDeletes->set(0);

    auto expansionEndTime = debugTimer.getElapsedMillis();
    printf("tid=%d expansion at_ms=%ld duration_ms=%ld oldCapacity=%ld newCapacity=%ld\n", tid, expansionStartTime, (expansionEndTime - expansionStartTime), oldCapacity, capacity);
}

void TLEHashTableExpand::migrateInsert(const int & key){
    int64_t h = murmur3(key);

    for(int64_t probe=0; probe < capacity; ++probe){
        int64_t index = (h+probe) % capacity;
        int found = data[index];

        if (found == EMPTY){
            int expected = EMPTY;
            // Can't use TLEGuard to CAS because we already have the gloval lock
            bool success = __atomic_compare_exchange_n(&data[index], &expected, key, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
            
            if (success){
                return;
            }
            // Otherwise continue trying to insert it
        }
    }

    assert(false);
}

// semantics: try to insert key. return true if successful (if key doesn't already exist), and false otherwise
bool TLEHashTableExpand::insertIfAbsent(const int tid, const int & key) {
    int64_t h = murmur3(key);
restart:
    uint64_t currentVersion = version;
    for (int64_t probeCount = 0; probeCount < capacity; ++probeCount){
        TLEGuard guard = TLEGuard(tid); // Must keep the guard out here in case capacity changes
        if (version != currentVersion){
            goto restart;
        }

        if (isExpandNeeded(tid, probeCount)){
            guard.explicit_fallback();
            // Not sure if this is required, but should be fast if we have the lock
            if (isExpandNeeded(tid, probeCount)){
                expand(tid);
                goto restart;
            }
            
        }

        // Look at next value
        int64_t index = (h+probeCount) % capacity;
        int found = data[index];

        // Attempt the insert
        if (found == key) {
            guard.explicit_commit();
            return false;
        } else if (found == EMPTY){
            data[index] = key;
            approxInserts->inc(tid);
            return true;
        }
        // else continue to next probeCount
    }

    assert(false);
    return false;
}

// semantics: try to erase key. return true if successful, and false otherwise
bool TLEHashTableExpand::erase(const int tid, const int & key) {
    int64_t h = murmur3(key);
    restart:
    uint64_t currentVersion = version;
      
    for(int64_t i=0; i < capacity; ++i){
        if (currentVersion != version){
            goto restart;
        }
        TLEGuard guard = TLEGuard(tid);
        int64_t index = (h+i) % capacity;
        int found = data[index];

        if (found == key){
            data[index] = TOMBSTONE;
            approxDeletes->inc(tid);
            return true;
        } else if (found == EMPTY){
            guard.explicit_commit();
            return false;
        }
        // else continue to next value
    }

    assert(false);
    return false;

}

// semantics: return the sum of all KEYS in the set
int64_t TLEHashTableExpand::getSumOfKeys() {
    int64_t sum = 0;
    #pragma omp parallel for reduction(+: sum)
    for (int64_t i=0;i<capacity;i++) {
        if (data[i] != TOMBSTONE && data[i] != EMPTY) {
            sum += data[i]; // note: this line is correct without fetch&add ONLY because of the #pragma omp reduction above!
        }
    }
    return sum;
}

void TLEHashTableExpand::printDebuggingDetails() {}
