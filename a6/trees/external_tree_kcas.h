#pragma once

#include <cassert>

#include <utility>

/***CHANGE THIS VALUE TO YOUR LARGEST KCAS SIZE ****/
#define MAX_KCAS 6
/***CHANGE THIS VALUE TO YOUR LARGEST KCAS SIZE ****/

#include "../kcas/kcas.h"

#include <cstdio>
#include <sstream>

using namespace std;
class ExternalKCAS {
private:
	typedef struct Node {
		const int key;
		casword<bool> marked;
		const bool isLeaf;

		Node(const int & _key, bool _isLeaf = true): key(_key), isLeaf(_isLeaf) {
			marked.setInitVal(false);
		}
	} Leaf;

	struct Internal : Node {
		casword<Node *> child[2];

		Internal(const int & _key, Node * left, Node * right): Node(_key, false) {
			child[0].setInitVal(left);
			child[1].setInitVal(right);
		}
	}; 


	/* VARS */
	volatile char padding0[PADDING_BYTES];
	const int numThreads;
	const int minKey;
	const int maxKey;

	
	volatile char padding1[PADDING_BYTES];
	casword<Internal *> root;
	volatile char padding2[PADDING_BYTES];

	
	tuple<Internal*, Internal*, Node*> search(const int & k);
	int compareKeys(const int & k1, const int & k2) {
		int val = k1 - k2;
		// return the sign
		return (0 < val) - (val < 0);
	}

	long getSumHelp(Node * n);

	

public:
	string printTree(Node * n);
	void printTreeHelp(Node * n, int depth, stringstream & ss);
	ExternalKCAS(const int _numThreads, const int _minKey, const int _maxKey);
	~ExternalKCAS();
	bool contains(const int tid, const int & key);
	bool insertIfAbsent(const int tid, const int & key); // try to insert key; return true if successful (if it doesn't already exist), false otherwise
	bool erase(const int tid, const int & key); // try to erase key; return true if successful, false otherwise
    
	long getSumOfKeys(); // should return the sum of all keys in the set
	void printDebuggingDetails(); // print any debugging details you want at the end of a trial in this function

};

ExternalKCAS::ExternalKCAS(const int _numThreads, const int _minKey, const int _maxKey)
        : numThreads(_numThreads), minKey(_minKey), maxKey(_maxKey) {
			Internal * startRoot = new Internal(minKey-1, new Leaf(minKey-1), new Leaf(maxKey+1));  
			root.setInitVal(startRoot);
}

ExternalKCAS::~ExternalKCAS() {
}

bool ExternalKCAS::contains(const int tid, const int & key) { 
	// TODO do we need this while loop?
	while(true){
		Node * n;
		tie(ignore, ignore, n) = search(key);

		if (compareKeys(key, n->key) != 0){
			return false;
		}
		return true;
	}
}

bool ExternalKCAS::insertIfAbsent(const int tid, const int & key) {
    assert(key > minKey - 1 && key >= minKey && key <= maxKey && key < maxKey + 1);
	//printf("%dAttempting to Add %d\n", tid, key);
	while (true){
		Internal *gp;
		Internal * p;
		Node *n;

		tie(gp, p, n) = search(key);
		int dir = compareKeys(key, n->key);

		if(dir == 0){
			return false;
		}
		Node * na = new Leaf(key);

		// node with smaller key and two children
		// TODO ordering???
		Node * left = n;
		Node * right = na;
		if (left->key > right->key){
			swap(left, right);
		}
		Node * n1 = new Internal(min(key, n->key), left, right);


		kcas::start();

		kcas::add(&p->marked, false, false);
		// child direction is 0/1 as 0 is ignored here
		int pdir = compareKeys(key, p->key) == 1 ? 1 : 0;
		kcas::add(&p->child[pdir], n, n1);
		kcas::add(&n->marked, false, false);
		
		if (kcas::execute()){
			//TPRINT("Added " << key << endl);
			// printf("+%d\n", key);
			return true;
		}

	}
}

bool ExternalKCAS::erase(const int tid, const int & key) {
	assert(key > minKey - 1 && key >= minKey && key <= maxKey && key < maxKey + 1);

	//printf("%dAttempting to Remove %d\n", tid, key);
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
			return true;


			//printTree(root);
			//cout << "=============================" << endl;
		}

	}


	return false;
}

tuple<ExternalKCAS::Internal*, ExternalKCAS::Internal*, ExternalKCAS::Node*> ExternalKCAS::search(const int & k) {
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


long ExternalKCAS::getSumOfKeys() {
	// We have to adjust for the two sentinal nodes
	return getSumHelp(root) - (minKey -1)  - (maxKey + 1);
}

long ExternalKCAS::getSumHelp(Node * n){
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

void ExternalKCAS::printDebuggingDetails() {

}

string ExternalKCAS::printTree(Node * n){
	stringstream ss;
	printTreeHelp(n, 0, ss);
	cout << ss.str() << endl;
	return ss.str();
}

void ExternalKCAS::printTreeHelp(Node * n, int depth, stringstream & ss){
	for (int i = 0; i < depth; i++){
		ss << " ";
	}
	ss << n->key << " m" << n->marked << " @" << n << "\n";

	if (!n->isLeaf){
		Internal * in = static_cast<Internal *>(n);
		printTreeHelp(in->child[0], depth+1, ss);
		printTreeHelp(in->child[1], depth+1, ss);
	}
}