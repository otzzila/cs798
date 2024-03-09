#pragma once

#include <cassert>
#include <utility>

#include <immintrin.h>

#include "defines.h"
#include "kcas.h"

#ifndef MAX_WAITS
#define MAX_WAITS 10
#endif

class DoublyLinkedList {
private:
    struct node {
        casword_t prevPtr;
        casword_t nextPtr;
        int key;
        casword_t marked;

        node(const int & tid, KCASLockFree<5> & kcas, int _key, node * prev, node * next) {
            kcas.writeInitPtr(tid, &prevPtr, (casword_t) prev);
            kcas.writeInitPtr(tid, &nextPtr, (casword_t) next);
            key = _key;
            kcas.writeInitVal(tid, &marked, (casword_t) false);
        }
    };

    volatile char padding0[PADDING_BYTES];
    const int numThreads;
    const int minKey;
    const int maxKey;
    volatile char padding2[PADDING_BYTES];
    KCASLockFree<5> kcas; // Max 5 addresses in algo, can we reduce by using a bit in a ptr?
    volatile char padding1[PADDING_BYTES];
    node head;
    volatile char padding4[PADDING_BYTES];
    node tail;
    volatile char padding5[PADDING_BYTES];

    

public:
    DoublyLinkedList(const int _numThreads, const int _minKey, const int _maxKey);
    ~DoublyLinkedList();
    
    bool contains(const int tid, const int & key);
    bool insertIfAbsent(const int tid, const int & key); // try to insert key; return true if successful (if it doesn't already exist), false otherwise
    bool erase(const int tid, const int & key); // try to erase key; return true if successful, false otherwise
    
    long getSumOfKeys(); // should return the sum of all keys in the set
    void printDebuggingDetails(); // print any debugging details you want at the end of a trial in this function
private:
    pair<node *, node *> internalSearch(const int tid, const int & key);

    void printNode(const int tid, node * n){
        if (n == nullptr || n == 0 || !n){return;}      
        printf("%p Node<%d, %d, %p, %p>\n",
            n,
            (bool) kcas.readVal(tid, &n->marked),
            n->key,
            (node *) kcas.readPtr(tid, &n->prevPtr),
            (node *) kcas.readPtr(tid, &n->nextPtr)
        );
    }
};

DoublyLinkedList::DoublyLinkedList(const int _numThreads, const int _minKey, const int _maxKey)
        : numThreads(_numThreads), minKey(_minKey), maxKey(_maxKey),
        kcas(), head(0, kcas, -1, 0, &tail), tail(0, kcas, -1, &head, 0) {
            //kcas.writeInitPtr(0, &head, (casword_t) 0);
    // it may be useful to know about / use the "placement new" operator (google)
    // because the simple_record_manager::allocate does not take constructor arguments
    // ... placement new essentially lets you call a constructor after an object already exists.
}

DoublyLinkedList::~DoublyLinkedList() {
}

bool DoublyLinkedList::contains(const int tid, const int & key) {
    assert(key > minKey - 1 && key >= minKey && key <= maxKey && key < maxKey + 1);
    pair<node *, node *> result = internalSearch(tid, key);
    node * succ = result.second;
    if (succ == 0){
        return false;
    }
    return succ->key == key;
}

bool DoublyLinkedList::insertIfAbsent(const int tid, const int & key) {
    assert(key > minKey - 1 && key >= minKey && key <= maxKey && key < maxKey + 1);
    
    while (true){
        pair<node*, node*> found = internalSearch(tid, key);
        node * pred = found.first;
        node * succ = found.second;
        // Check if already inserted
        if (succ != &tail && key == succ->key) {
            TRACE TPRINT("Already inserted " <<key << "\n");
            return false;
        }

        bool newHead = pred == 0;

        node * n = new node(tid, kcas, key, pred, succ); // make pred null
        //cout << "Insert " << (int)key << endl;
        // PRINT(key);
        //PRINT(((node*) kcas.readPtr(tid, &head)));
        //PRINT(pred);
        //PRINT(succ);
        // printDebuggingDetails();
        //kcas.writeInitPtr(tid, &n->prevPtr, (casword_t) pred);
        //kcas.writeInitPtr(tid, &n->nextPtr, (casword_t) succ);


        // Perform kcas
        auto descPtr = kcas.getDescriptor(tid);

        
        descPtr->addPtrAddr(&pred->nextPtr, (casword_t) succ, (casword_t) n);
        descPtr->addValAddr(&pred->marked, (casword_t) false, (casword_t) false);

        descPtr->addPtrAddr(&succ->prevPtr, (casword_t) pred, (casword_t) n);
        descPtr->addValAddr(&succ->marked, (casword_t) false, (casword_t) false);

        if (kcas.execute(tid, descPtr)) {
            //assert((node *) kcas.readPtr(tid, &pred->nextPtr) == n);
            //assert((node *) kcas.readPtr(tid, &succ->prevPtr) == n);
            // TPRINT("Insert worked! " << key << "\n");
            return true;
        } else {
            delete n;
            for (int i = 0; i < MAX_WAITS; ++i){
                _mm_pause();
            }
        }
    }

    assert(false);
}

bool DoublyLinkedList::erase(const int tid, const int & key) {
    assert(key > minKey - 1 && key >= minKey && key <= maxKey && key < maxKey + 1);
    while (true){
        pair<node*, node*> found = internalSearch(tid, key);
        node * pred = found.first;
        node * succ = found.second;
        // Check if already inserted
        if (succ == &tail || key != succ->key) {
            //TRACE TPRINT("Erase failed! " << key << "\n")
            return false;
        }
        
        // Perform kcas
        auto descPtr = kcas.getDescriptor(tid);
        casword_t afterW = kcas.readPtr(tid, &succ->nextPtr);
        node * after = (node *) afterW;


        descPtr->addPtrAddr(&pred->nextPtr, (casword_t) succ, (casword_t) after);
        descPtr->addValAddr(&pred->marked, (casword_t) false, (casword_t) false);
        
        // We get marked for removal
        descPtr->addValAddr(&succ->marked, (casword_t) false, (casword_t) true);

        descPtr->addValAddr(&after->marked, (casword_t) false, (casword_t) false);
        descPtr->addPtrAddr(&after->prevPtr, (casword_t) succ, (casword_t) pred);
        

        if (kcas.execute(tid, descPtr)) {
            //{TPRINT("Erase Worked! " << key << "\n" );}
            //PRINT(true);
            return true;
        }
    }
    
    assert(false);
}

long DoublyLinkedList::getSumOfKeys() {
    long total = 0;

    node * current = (node*) kcas.readPtr(0, &head.nextPtr);
    const int tid = 0;

    while (current != &tail) {
        total += current->key;

        current = (node *)kcas.readPtr(tid, &current->nextPtr);
    };


    return total;
}

void DoublyLinkedList::printDebuggingDetails() {
    node * current = (node*) kcas.readPtr(0, &head.nextPtr);
    const int tid = 0;

    long total = 0;

    while (current != 0) {
        assert(current != 0);
        PRINT(current);
        printNode(tid, current);

        total += current->key;
        current = (node *)kcas.readPtr(tid, &current->nextPtr);
    };

    PRINT(total);
}


pair<DoublyLinkedList::node*, DoublyLinkedList::node*> DoublyLinkedList::internalSearch(const int tid, const int & key){
    node * pred = &head;
    node * succ = (node*) kcas.readPtr(0, &head.nextPtr);

    while (true){
        if (succ == &tail || key <= succ->key){
            return pair(pred, succ);
        }
        pred = succ;
        // Read ptr as node
        succ = (node *) kcas.readPtr(tid, &succ->nextPtr); 
    }

}
