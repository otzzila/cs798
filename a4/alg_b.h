#pragma once
#include "util.h"
#include <atomic>
#include <mutex>
using namespace std;

class AlgorithmB {
public:
    static constexpr int TOMBSTONE = -1;
    static constexpr int EMPTY = 0;

    struct lockedKey {
        mutex m;
        int key = EMPTY; // Protected by lock, does not need atomic
        char padding[PADDING_BYTES - sizeof(int) - sizeof(mutex)];
    };

    char padding1[PADDING_BYTES];
    lockedKey* data;

    char padding0[PADDING_BYTES];
    const int numThreads;
    int capacity;
    char padding2[PADDING_BYTES];

    AlgorithmB(const int _numThreads, const int _capacity);
    ~AlgorithmB();
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
AlgorithmB::AlgorithmB(const int _numThreads, const int _capacity)
: numThreads(_numThreads), capacity(_capacity) {
    data = new lockedKey[capacity];   
}

// destructor: clean up any allocated memory, etc.
AlgorithmB::~AlgorithmB() {
    delete [] data;
}

// semantics: try to insert key. return true if successful (if key doesn't already exist), and false otherwise
bool AlgorithmB::insertIfAbsent(const int tid, const int & key) {
    uint32_t h = murmur3(key);
    for (int i = 0; i < capacity; ++i){
        int index = (h+i) % capacity;
        // Lock the data
        
        int found = data[index].key;
        if (found == key){
            return false;
        } else if (found == EMPTY){
            data[index].m.lock();
            found = data[index].key;
            if (found == key){
                // Someone inserted before we could
                data[index].m.unlock();
                return false;
            } else if (found == EMPTY){
                // Still empty, so we insert
                data[index].key = key;
                data[index].m.unlock();
                return true;
            } else {
                // Someone inserted something else, continue
                data[index].m.unlock();
            }
            
        }
    }

    return false;
}

// semantics: try to erase key. return true if successful, and false otherwise
bool AlgorithmB::erase(const int tid, const int & key) {
    uint32_t h = murmur3(key);
    for (int i = 0; i < capacity; ++i){
        int index = (h+i) % capacity;


        int found = data[index].key;
        if (found == key){
            // Replace with tombstone
            data[index].m.lock();
            bool actuallyRenamed = data[index].key == key;
            data[index].key = TOMBSTONE;
            data[index].m.unlock();
            return actuallyRenamed;
        } else if (found == EMPTY){
            return false;
        }
    }
    return false;
}

// semantics: return the sum of all KEYS in the set
int64_t AlgorithmB::getSumOfKeys() {
    int64_t total = 0;
    for (int index = 0; index < capacity; ++index){
        // Making sure we lock because time doesn't matter here and why not be extra safe
        // It shouldn't be needed but it's nice to double check
        data[index].m.lock();
        if(data[index].key > 0){
            total += data[index].key;
        }
        data[index].m.unlock();
    }
    return total;
}

// print any debugging details you want at the end of a trial in this function
void AlgorithmB::printDebuggingDetails() {
    
}
