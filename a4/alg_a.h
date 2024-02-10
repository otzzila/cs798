#pragma once
#include "util.h"
#include <atomic>
#include <mutex>
using namespace std;

class AlgorithmA {
protected:
    struct lockedKey {
        mutex m;
        int key = EMPTY; // Protected by lock, does not need atomic
        char padding[PADDING_BYTES - sizeof(int) - sizeof(mutex)];
    };

    char padding1[PADDING_BYTES];
    lockedKey* data;

public:
    static constexpr int TOMBSTONE = -1;
    static constexpr int EMPTY = 0;

    char padding0[PADDING_BYTES];
    const int numThreads;
    int capacity;
    char padding2[PADDING_BYTES];

    AlgorithmA(const int _numThreads, const int _capacity);
    ~AlgorithmA();
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
AlgorithmA::AlgorithmA(const int _numThreads, const int _capacity)
: numThreads(_numThreads), capacity(_capacity) {
    // Allocate some data
    data = new lockedKey[capacity];
}

// destructor: clean up any allocated memory, etc.
AlgorithmA::~AlgorithmA() {
    delete[] data; // Free our data
}

// semantics: try to insert key. return true if successful (if key doesn't already exist), and false otherwise
bool AlgorithmA::insertIfAbsent(const int tid, const int & key) {
    uint32_t h = murmur3(key);
    for (int i = 0; i < capacity; ++i){
        int index = (h+i) % capacity;
        // Lock the data
        data[index].m.lock();
        int found = data[index].key;
        if (found == key){
            data[index].m.unlock();
            return false;
        } else if (found == EMPTY){
            data[index].key = key;
            data[index].m.unlock();
            return true;
        }
        // unlock and continue
        data[index].m.unlock();
    }

    return false;
}

// semantics: try to erase key. return true if successful, and false otherwise
bool AlgorithmA::erase(const int tid, const int & key) {
    uint32_t h = murmur3(key);
    for (int i = 0; i < capacity; ++i){
        int index = (h+i) % capacity;


        data[index].m.lock();
        int found = data[index].key;
        if (found == key){
            // Replace with tombstone
            data[index].key = TOMBSTONE;
            data[index].m.unlock();
            return true;
        } else if (found == EMPTY){
            data[index].m.unlock();
            return false;
        }
        // unlock and continue
        data[index].m.unlock();
    }
    return false;
}

// semantics: return the sum of all KEYS in the set
int64_t AlgorithmA::getSumOfKeys() {
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
void AlgorithmA::printDebuggingDetails() {
    /*
    for (int index = 0; index < capacity; ++index){
        data[index].m.lock();
        int found = data[index].key;
        if (found == EMPTY){
            cout << " ";
        } else if (found == TOMBSTONE){
            cout << "_";
        } else {
            cout << "X";
        }
        // cout << data[index].key;
        data[index].m.unlock();

        if (index % 50 == 49){
            cout << endl;
        }
    }
    cout << endl << endl;
    */
   /* 
    for (int index = 0; index < capacity; ++index){
        data[index].m.lock();
        int found = data[index].key;
        if (found > 0){
            cout << index << ":" << found << endl;
        }
        data[index].m.unlock();
    }
    cout << endl; */
}
