#include <iostream>
#include <thread>
using namespace std;

#define TOTAL_INCREMENTS 100000000

__thread int tid;

class mutex_t {
private:
    bool volatile wants_to_enter[2]; // who wants a turn?
    volatile int turn; // whose turn is it
public:
    mutex_t() {
        
    }
    void lock(int id) {
        wants_to_enter[id] = true;
        __sync_synchronize();
        while (wants_to_enter[!id]){
            __sync_synchronize();
            if (turn != id){
                // Temporarily stop wanting to enter
                wants_to_enter[id] = false;
                __sync_synchronize();
                while (turn != id){
                    __sync_synchronize();
                    // wait
                }
                wants_to_enter[id] = true;
                __sync_synchronize();
            }
        }
    }
    void unlock(int id) {
        turn = !id;
        wants_to_enter[id] = false;
        __sync_synchronize();
    }
};


class counter_locked {
private:
    mutex_t m;
    volatile int v;
public:
    counter_locked() {
        
    }

    void increment(int id) {
        m.lock(id);
        v++;
        m.unlock(id);
    }

    int get(int id) {
        m.lock(id);
        auto result = v;
        m.unlock(id);
        return result;
    }
};

counter_locked c;

void threadFunc(int _tid) {
    tid = _tid;
    for (int i = 0; i < TOTAL_INCREMENTS / 2; ++i) {
        c.increment(tid);
    }
}

int main(void) {
    // create and start threads
    thread t0(threadFunc, 0);
    thread t1(threadFunc, 1);
    t0.join();
    t1.join();
    cout<<c.get(0)<<endl;
    return 0;
}
