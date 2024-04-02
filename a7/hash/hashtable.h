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

    bool isExpandNeeded(const int tid, int64_t probeCount);
    void expand(const int tid);
    int64_t getAccurateSize();

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



    // suggested adjustment to counters at the end of expansion:
    //  approxInserts->set(accurateSize);
    //  approxDeletes->set(0);

    auto expansionEndTime = debugTimer.getElapsedMillis();
    printf("tid=%d expansion at_ms=%ld duration_ms=%ld oldCapacity=%ld newCapacity=%ld\n", tid, expansionStartTime, (expansionEndTime - expansionStartTime), oldCapacity, capacity);
}

// semantics: try to insert key. return true if successful (if key doesn't already exist), and false otherwise
bool TLEHashTableExpand::insertIfAbsent(const int tid, const int & key) {

}

// semantics: try to erase key. return true if successful, and false otherwise
bool TLEHashTableExpand::erase(const int tid, const int & key) {

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
