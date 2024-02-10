#pragma once
#include "util.h"
#include <atomic>
using namespace std;

class AlgorithmC {
    char padding1[PADDING_BYTES];
    atomic<int>* data;
public:
    static constexpr int TOMBSTONE = -1;
    static constexpr int EMPTY = 0;

    char padding0[PADDING_BYTES];
    const int numThreads;
    int capacity;
    char padding2[PADDING_BYTES];

    AlgorithmC(const int _numThreads, const int _capacity);
    ~AlgorithmC();
    bool insertIfAbsent(const int tid, const int & key);
    bool erase(const int tid, const int & key);
    long getSumOfKeys();
    void printDebuggingDetails(); 
};

/**
 * constructor: initialize the hash table's internals
 * 
 * @param _numThreads maximum number of threads that will ever use the hash table (i.e., at least tid+1, where tid is the largest thread ID passed to any function of this class)
 * @param _capacity is the INITIAL size of the hash table (maximum number of elements it can contain WITHOUT expansion)
 */
AlgorithmC::AlgorithmC(const int _numThreads, const int _capacity)
: numThreads(_numThreads), capacity(_capacity) {
    data = new atomic<int>[capacity] {};
}

// destructor: clean up any allocated memory, etc.
AlgorithmC::~AlgorithmC() {
    delete [] data;
}

// semantics: try to insert key. return true if successful (if key doesn't already exist), and false otherwise
bool AlgorithmC::insertIfAbsent(const int tid, const int & key) {
    uint32_t h = murmur3(key);
    for (int i = 0; i < capacity; ++i){
        int index = (h+i) % capacity;
        // Lock the data
        int found = data[index];
        if (found == key){
            return false;
        } else if (found == EMPTY){
            int expected = EMPTY;
            // Attempt a CAS
            if (data[index].compare_exchange_strong(expected, key,
            memory_order_release, memory_order_relaxed)){
                return true;
            } else if (expected == key){ // data[index] == key the expected should have an updated value
                return false;
            }
        }
    }
    return false;
}

// semantics: try to erase key. return true if successful, and false otherwise
bool AlgorithmC::erase(const int tid, const int & key) {
    uint32_t h = murmur3(key);
    for (int i = 0; i < capacity; ++i){
        int index = (h+i) % capacity;
        // Lock the data
        int found = data[index];

        if (found == EMPTY){
            return false;
        } else if (found == key){
            int expected = key;
            return (data[index].compare_exchange_strong(expected, TOMBSTONE));
        }
    }
    return false;
}

// semantics: return the sum of all KEYS in the set
int64_t AlgorithmC::getSumOfKeys() {
    int64_t total = 0;
    for (int index = 0; index < capacity; ++index){
        // Making sure we lock because time doesn't matter here and why not be extra safe
        // It shouldn't be needed but it's nice to double check
        if(data[index] > 0){
            total += data[index];
        }
    }
    return total;
}

// print any debugging details you want at the end of a trial in this function
void AlgorithmC::printDebuggingDetails() {
    
}
