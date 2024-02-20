#pragma once
#include "util.h"
#include <atomic>
#include <cmath>
using namespace std;

#include <immintrin.h>
#ifndef WAIT_COUNT
    #define WAIT_COUNT 10
#endif

class AlgorithmD {
private:
    enum {
        MARKED_MASK = (int) 0x80000000,     // most significant bit of a 32-bit key
        TOMBSTONE = (int) 0x7FFFFFFF,       // largest value that doesn't use bit MARKED_MASK
        EMPTY = (int) 0
    }; // with these definitions, the largest "real" key we allow in the table is 0x7FFFFFFE, and the smallest is 1 !!

    struct table {
        char padding0[PADDING_BYTES];
        // data types
        atomic<int> * data;
        atomic<int> * old;
        int capacity;
        int oldCapacity;
        // Approximate total values with inserts and deletes
        counter * approxInserts;
        counter * approxDeletes;
        atomic<int> chunksClaimed;
        atomic<int> chunksDone;
        char padding1[PADDING_BYTES];
        // constructor
        table(int numThreads, int _capacity) : old(nullptr), oldCapacity(0), chunksClaimed(0), chunksDone(0),
        capacity(_capacity)
         {
            data = new atomic<int>[capacity];
            for (int i = 0; i < capacity; ++i){
                atomic_init(&data[i], EMPTY);
            }
            approxInserts = new counter(numThreads);
            approxDeletes = new counter(numThreads);
        }

        // Not sure if any good
        table(int numThreads, table * oldT): old(oldT->data), oldCapacity(oldT->capacity), chunksClaimed(0), chunksDone(0)
        {
            // Calculate new capacity
            int numInsertedValues = oldT->approxInserts->getAccurate() - oldT->approxDeletes->getAccurate();
            //int numInsertedValues = oldT->approxInserts->getAccurate();
            if (numInsertedValues <= 0.0) { numInsertedValues = 1.0; }

            capacity = max(4 * numInsertedValues, oldT->capacity);

            data = new atomic<int>[capacity];
            for (int i = 0; i < capacity; ++i){
                atomic_init(&data[i], EMPTY);
            }
            TRACE {
                for (int i = 0; i < capacity; ++i){
                    if (data[i] != EMPTY){
                        throw new bad_alloc();
                    }
                }
            }
            
            approxInserts = new counter(numThreads);
            approxDeletes = new counter(numThreads);
        } 

        // destructor
        ~table(){
            // The next table may control data...
            //if (data != nullptr){ delete [] data; }
            //if (old != nullptr){ delete [] old; }
            //delete approxInserts;
            //delete approxDeletes;
        }
    };
    
    static const int CHUNK_SIZE = 4096;
    bool expandAsNeeded(const int tid, table * t, int i);
    void helpExpansion(const int tid, table * t);
    void startExpansion(const int tid, table * t);
    void migrate(const int tid, table * t, int myChunk);
    
    char padding0[PADDING_BYTES];
    int numThreads;
    int initCapacity;
    // more fields (pad as appropriate)
    char padding1[PADDING_BYTES];
    atomic<table *> currentTable;
    char padding2[PADDING_BYTES];
public:
    AlgorithmD(const int _numThreads, const int _capacity);
    ~AlgorithmD();
    bool insertIfAbsent(const int tid, const int & key, bool disableExpansion);
    bool erase(const int tid, const int & key);
    long getSumOfKeys();
    void printDebuggingDetails(); 

private:
    bool insertIfAbsentHashed(const int tid, const int & key, uint32_t h, bool disableExpansion);
    bool eraseHashed(const int tid, const int & key, uint32_t h);
};

/**
 * constructor: initialize the hash table's internals
 * 
 * @param _numThreads maximum number of threads that will ever use the hash table (i.e., at least tid+1, where tid is the largest thread ID passed to any function of this class)
 * @param _capacity is the INITIAL size of the hash table (maximum number of elements it can contain WITHOUT expansion)
 */
AlgorithmD::AlgorithmD(const int _numThreads, const int _capacity)
: numThreads(_numThreads), initCapacity(_capacity) {
    currentTable = new table(numThreads, initCapacity);
}

// destructor: clean up any allocated memory, etc.
AlgorithmD::~AlgorithmD() {
    delete currentTable;
}

bool AlgorithmD::expandAsNeeded(const int tid, table * t, int i) {
    // If expansion is ongoing, help out
    helpExpansion(tid, t);

    // Expanding based on inserts only because that is getting filled
    if (t->approxInserts->get() > t->capacity/2 ||
        (i > 10 && t->approxInserts->getAccurate() > t->capacity/2)){
            startExpansion(tid, t);
            return true;
        }
    return false;
}

void AlgorithmD::helpExpansion(const int tid, table * t) {
    int totalOldChunks = ceil(static_cast<float>(t->oldCapacity) / CHUNK_SIZE);
    
    while(t->chunksClaimed < totalOldChunks) {
        int myChunk = t->chunksClaimed.fetch_add(1);
        if (myChunk < totalOldChunks){
            // We got a real chunk and we have to move it
            migrate(tid, t, myChunk);
            int chunksDone = t->chunksDone.fetch_add(1);
        }
    }
    // busy wait until done ? (this seems like blocking)
    while (!(t->chunksDone == totalOldChunks)) {
        for (int i = 0; i < WAIT_COUNT; ++i){
            _mm_pause();
        }
     } // TODO backoff here
    // Assert totals are the same
    
    
}

void AlgorithmD::startExpansion(const int tid, table * t) {
    if (currentTable == t){
        table * newT = new table(numThreads, t);
        // Attempt to insert or else delete
        if (!currentTable.compare_exchange_strong(t, newT)) { 
            delete [] newT->data;
            delete newT->approxDeletes;
            delete newT->approxInserts;
            delete newT; 
        } 
        else {TRACE {TPRINT("New table!");} } 
    }
    helpExpansion(tid, currentTable);
}

void AlgorithmD::migrate(const int tid, table * t, int myChunk) {
    TRACE TPRINT("MIGRATING CHUNK " << myChunk);
    TRACE TPRINT("Capacity " << t->capacity);
    int start = myChunk * CHUNK_SIZE;
    for (int idx = start; idx < start + CHUNK_SIZE && idx < t->oldCapacity; ++idx){
        
        // Mark key before copying it
        int currentKey = t->old[idx];
        int expected = currentKey;
        while (!t->old[idx].compare_exchange_strong(currentKey, currentKey | MARKED_MASK)) {
            /* try again */ 
            // Should not be marked because this is our chunk to migrate
        }

        //TRACE TPRINT(">M Inserting");
        // Now copy it
        currentKey = currentKey & (~MARKED_MASK); // This should be redundant but I'm keeping it for now
        if (currentKey != EMPTY && currentKey != TOMBSTONE){
            bool result = insertIfAbsent(tid, currentKey, true); // Disable expansion or you get stuck in a loop
            TRACE {if(!result) {
                    TPRINT("Failed migrate insert: " << bool(currentKey &MARKED_MASK) << " val: " << (currentKey & (~MARKED_MASK)));
                    TPRINT(this << tid << myChunk << t);
                }
            }
        }
        //TRACE TPRINT(">M Done Inserting");
    }

    // Indicate that we are done migrating
    
    TRACE TPRINT("<DONE MIGRATION")
    TRACE {
        uint32_t newTotal = 0;
        for (int i = 0; i < t->capacity; ++i){
            int value = t->data[i] & (~MARKED_MASK);
            if (value != EMPTY && value != TOMBSTONE){
                newTotal += value;
            }
        }

        uint32_t oldTotal = 0;
        for (int i = 0; i < t->oldCapacity; ++i){
            int value = t->old[i] & (~MARKED_MASK);
            if (value != EMPTY && value != TOMBSTONE){
                oldTotal += value;
            }
        }
        TRACE PRINT(newTotal);
        TRACE PRINT(oldTotal);
        
    }
                       
}

inline bool AlgorithmD::insertIfAbsentHashed(const int tid, const int & key, uint32_t h, bool disableExpansion = false) {
    table * tab = currentTable;

    int indexBase = h % tab->capacity;
    for(int i=0; i < tab->capacity; ++i){
        if (!disableExpansion && expandAsNeeded(tid, tab, i)) {
            return insertIfAbsentHashed(tid, key, h, disableExpansion);
        }

        // lookup data
        // using the indexing method that allows faster migration
        //int index = floor(h / INT32_MAX * tab->capacity);
        int index = indexBase + i;
        if (index >= tab->capacity) {
            index -= tab->capacity;
        }
        int found = tab->data[index];

        if (found & MARKED_MASK) {
            // Restart to help in new table
            return insertIfAbsentHashed(tid, key, h, disableExpansion);
        }
        else if (found == key){
            return false; // already here
        } else if (found == EMPTY){
            // Attempt insert
            if(tab->data[index].compare_exchange_strong(found, key)){
                // CAS success
                tab->approxInserts->inc(tid); // record we inserted;
                return true;
            } else {
                // CAS failed. found now contains the found value
                if (found & MARKED_MASK) {
                    // Marked for expansion, try to insert
                    return insertIfAbsentHashed(tid, key, h, disableExpansion);
                } else if (found == key){
                    return false;
                }
            }
        }
    }
    
    return false;
}
// semantics: try to insert key. return true if successful (if key doesn't already exist), and false otherwise
bool AlgorithmD::insertIfAbsent(const int tid, const int & key, bool disableExpansion = false) {
    uint32_t h = murmur3(key);
    return insertIfAbsentHashed(tid, key, h, disableExpansion);
}


inline bool AlgorithmD::eraseHashed(const int tid, const int & key, uint32_t h){
    table * tab = currentTable;

    int indexBase = h % tab->capacity;
    for(int i=0; i < tab->capacity; ++i){
        // Check if expanding
        if (expandAsNeeded(tid, tab, i)){
            return eraseHashed(tid, key, h);
        }
        // int index = floor(h / INT32_MAX * tab->capacity);
        int index = indexBase + i;
        if (index >= tab->capacity){
            index -= tab->capacity;
        }
        int found = tab->data[index];
        
        if (found & MARKED_MASK){
            // Marked for expansion, restart
            return eraseHashed(tid, key, h);
        } else if (found == key) {
            // Atempt to delete
            if (tab->data[index].compare_exchange_strong(found, TOMBSTONE)){
                tab->approxDeletes->inc(tid);
                return true;
            } else if (found == key | MARKED_MASK){
                // restart in the new table
                return eraseHashed(tid, key, h);
            } else if (found == TOMBSTONE) {
                // This must now be a tombstone
                return false;
            }
        } else if (found == EMPTY) {
            return false;
        } 
        // tombstone or a different key so we continue


    }

    return false;
}

// semantics: try to erase key. return true if successful, and false otherwise
bool AlgorithmD::erase(const int tid, const int & key) {
    uint32_t h = murmur3(key);
    return eraseHashed(tid, key, h);
}

// semantics: return the sum of all KEYS in the set
int64_t AlgorithmD::getSumOfKeys() {
    // wait until migration is done
    table * t = currentTable;
    int totalOldChunks = ceil(static_cast<float>(t->oldCapacity) / CHUNK_SIZE);
    
    while(t->chunksClaimed < totalOldChunks || t->chunksDone < totalOldChunks)
    { /* busy wait */}

    int64_t total = 0;
    for (int i = 0; i < t->capacity; ++i){
        int value = t->data[i] & (~MARKED_MASK);
        if ((value != TOMBSTONE) && (value != EMPTY)){
            total += value;
        }
    }

    return total;
}

// print any debugging details you want at the end of a trial in this function
void AlgorithmD::printDebuggingDetails() {
    PRINT(initCapacity);
    PRINT(currentTable.load()->capacity);
}
