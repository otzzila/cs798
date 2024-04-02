/**
 * You are allowed to use TLELock/TLEGuard ONLY in q_hash, **NOT** in q_tree !!
 */

#pragma once

// DO NOT INVOKE ANY FUNCTIONS ON THIS OBJECT TYPE. ONLY USE IT THROUGH TLEGuard!
class TLELock {
private:
    char padding0[PADDING_BYTES];
    int volatile state;
    char padding1[PADDING_BYTES];
    debugCounter numCommit;
    debugCounter numAbort;
    debugCounter numFallback;
    debugCounter numSpinIterations;
    char padding2[PADDING_BYTES];

public:
    TLELock();
    ~TLELock();
    bool tryAcquire();
    void acquire(const int tid);
    void release();
    bool isHeld();
    void incNumCommit(const int tid);
    void incNumAbort(const int tid);
    void incNumFallback(const int tid);
    void incNumSpinIterations(const int tid);
};

class TLEGuard {
private:
    int attempts;
    int status_code;
    TLELock * lock;
    bool weHoldTheLock;
    bool alreadyCommitted;
    int myTid;
public:
    TLEGuard(const int _tid);
    ~TLEGuard();
    void explicit_commit();
    void explicit_fallback();
};
