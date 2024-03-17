#pragma once

#include <cassert>

#include <vector>

/***CHANGE THIS VALUE TO YOUR LARGEST KCAS SIZE****/
#define MAX_KCAS 6
/***CHANGE THIS VALUE TO YOUR LARGEST KCAS SIZE ****/

#include "../kcas/kcas.h"
#include "../recordmgr/record_manager.h"

using namespace std;
class ExternalKCASReclaim {
private:
    typedef struct Node {
		const int key;
		casword<bool> marked;
		const bool isLeaf;

		Node(const int & _key, bool _isLeaf = true): key(_key), isLeaf(_isLeaf) {
			marked.setInitVal(false);
		}
        Node(): key(-999), isLeaf(false) {}
	} Leaf;

	struct Internal : Node {
		casword<Node *> child[2];


        Internal(): Node(0, false) {}
		Internal(const int & _key, Node * left, Node * right): Node(_key, false) {
			child[0].setInitVal(left);
			child[1].setInitVal(right);
		}
	}; 
public:

private:
    volatile char padding0[PADDING_BYTES];
    const int numThreads;
    const int minKey;
    const int maxKey;
    volatile char padding1[PADDING_BYTES];
	casword<Internal *> root;
	volatile char padding2[PADDING_BYTES];
    simple_record_manager<Leaf, Internal> nodeManager;
    volatile char padding3[PADDING_BYTES];


public:
    ExternalKCASReclaim(const int _numThreads, const int _minKey, const int _maxKey);
    ~ExternalKCASReclaim();

    bool contains(const int tid, const int & key);
    bool insertIfAbsent(const int tid, const int & key); // try to insert key; return true if successful (if it doesn't already exist), false otherwise
    bool erase(const int tid, const int & key); // try to erase key; return true if successful, false otherwise

    long getSumOfKeys(); // should return the sum of all keys in the set
    void printDebuggingDetails(); // print any debugging details you want at the end of a trial in this function
private:
    tuple<Internal*, Internal*, Node*> search(const int & k);
	int compareKeys(const int & k1, const int & k2) {
		int val = k1 - k2;
		// return the sign
		return (0 < val) - (val < 0);
	}
    long getSumHelp(Node * n);
};


ExternalKCASReclaim::ExternalKCASReclaim(const int _numThreads, const int _minKey, const int _maxKey)
: numThreads(_numThreads), minKey(_minKey), maxKey(_maxKey),
nodeManager(MAX_THREADS) {
    Leaf * minLeaf = new (nodeManager.allocate<Leaf>(0)) Leaf(minKey-1);
    Leaf * maxLeaf = new (nodeManager.allocate<Leaf>(0)) Leaf(maxKey+1);
    Internal * startRoot = new (nodeManager.allocate<Internal>(0)) Internal(minKey-1, minLeaf , maxLeaf);  
			root.setInitVal(startRoot);
}

ExternalKCASReclaim::~ExternalKCASReclaim() {
    int tid = 0;
    auto guard = nodeManager.getGuard(tid);
    vector<Node*> toDelete;

    toDelete.push_back(root);
    
    while(!toDelete.empty()){
        Node * node = toDelete.back();
        toDelete.pop_back();

        if (node->isLeaf){
            nodeManager.deallocate<Leaf>(tid, node);
        } else {
            Internal * in = static_cast<Internal*>(node);
            Node * left = in->child[0];
            Node * right = in->child[1];
            if (left != nullptr){
                toDelete.push_back(left);
            }
            if (right != nullptr){
                toDelete.push_back(right);
            }

            nodeManager.deallocate<Internal>(tid, in);
        }

    }

    
}

bool ExternalKCASReclaim::contains(const int tid, const int & key) {
	// TODO do we need this while loop?
    auto guard = nodeManager.getGuard(tid, true);
	while(true){
		Node * n;
		tie(ignore, ignore, n) = search(key);

		if (compareKeys(key, n->key) != 0){
			return false;
		}
		return true;
	}
}

bool ExternalKCASReclaim::insertIfAbsent(const int tid, const int & key) {
    assert(key > minKey - 1 && key >= minKey && key <= maxKey && key < maxKey + 1);
	//printf("%dAttempting to Add %d\n", tid, key);
    auto guard = nodeManager.getGuard(tid);
    
	while (true){
		Internal *gp;
		Internal * p;
		Node *n;

		tie(gp, p, n) = search(key);
		int dir = compareKeys(key, n->key);

		if(dir == 0){
			return false;
		}
		Node * na = new (nodeManager.allocate<Leaf>(tid)) Leaf(key);

		// node with smaller key and two children
		// TODO ordering???
		Node * left = n;
		Node * right = na;
		if (left->key > right->key){
			swap(left, right);
		}
		Internal * n1 = new (nodeManager.allocate<Internal>(tid)) Internal(min(key, n->key), left, right);


		kcas::start();

		kcas::add(&p->marked, false, false);
		// child direction is 0/1 as 0 is ignored here
		int pdir = compareKeys(key, p->key) == 1 ? 1 : 0;
		kcas::add(&p->child[pdir], n, static_cast<Node*>(n1));
		kcas::add(&n->marked, false, false);
		
		if (kcas::execute()){
			//TPRINT("Added " << key << endl);
			// printf("+%d\n", key);
			return true;
		}

        // Failed, deallocate n1
        nodeManager.deallocate<Leaf>(tid, na);
        nodeManager.deallocate<Internal>(tid, n1);

	}
}

bool ExternalKCASReclaim::erase(const int tid, const int & key) {
    assert(key > minKey - 1 && key >= minKey && key <= maxKey && key < maxKey + 1);
    auto guard = nodeManager.getGuard(tid);

    while(true){
		Internal *gp;
		Internal *p;
		Node *n;
		tie(gp,p,n) = search(key);


		if (compareKeys(key, n->key) != 0){
			return false;
		}

		//cout << "Erasing " << key << endl;
		//cout << "gp: " << gp->key << " @ " << gp << " p: " << p->key << " @ " << p << " n: " << n->key << " @ " << n << endl;
		//printTree(root);
		//cout << "-----------------------------" << endl;
		

		kcas::start();

		int nDir = compareKeys(n->key, p->key) == 1 ? 1 : 0;
		int pDir = compareKeys(p->key, gp->key) == 1 ? 1 : 0;
		int pOtherDir = nDir == 1 ? 0 : 1; // P's child that is not n!!!
		Node * pOther = p->child[pOtherDir];
		//cout << "pDir: " << pDir << " ";
		//cout << "pOther: " << pOther->key << " @ " << pOther << endl;

		//PRINT(nDir);
		//PRINT(pDir);
		//cout << endl;
		kcas::add(&gp->child[pDir], static_cast<Node*>(p), pOther);

		

		
		kcas::add(&p->child[nDir], n, n);
		kcas::add(&gp->marked, false, false);
		kcas::add(&n->marked, false, true);
		kcas::add(&p->marked, false, true);

		// extra
		kcas::add(&p->child[pOtherDir], pOther, pOther);

		if(kcas::execute()){
			//TPRINT("Removed " << key << endl);
			// printf("-%d\n", key);
            nodeManager.retire<Leaf>(tid, n);
            nodeManager.retire<Internal>(tid, p);
			return true;


			//printTree(root);
			//cout << "=============================" << endl;
		}

	}

	return false;
}

long ExternalKCASReclaim::getSumOfKeys() {
    auto guard = nodeManager.getGuard(0, true);

    return getSumHelp(root) - (minKey -1)  - (maxKey + 1);
}

long ExternalKCASReclaim::getSumHelp(Node * n){
	if (n->marked){
		PRINT(n->marked);
	}
	if (n->isLeaf){
		return n->key;
	} else {
		Internal * nInt = static_cast<Internal *>(n);
		return getSumHelp(nInt->child[0]) + getSumHelp(nInt->child[1]);
	}
}

tuple<ExternalKCASReclaim::Internal*, ExternalKCASReclaim::Internal*, ExternalKCASReclaim::Node*> ExternalKCASReclaim::search(const int & k) {
	Node * n = root;
	Internal * gp = nullptr;
	Internal * p = nullptr;


	Internal * in = static_cast<Internal *>(n);
	

	while(!n->isLeaf){
		int dir = compareKeys(k, n->key);

		gp = p;
		p = static_cast<Internal *>(n);
		n = p->child[dir == 1 ? 1 : 0];
		
	}
	return tuple<Internal*,Internal*,Node*>(gp,p,n);

}

void ExternalKCASReclaim::printDebuggingDetails() {

}


