#include <thread>
#include <algorithm>
#include <string>
#include <cstdio>
#include <iostream>


char * getArg(char** start, char** end, const std::string & optname){
    char ** pos = std::find(start, end, optname);
    if(pos != end && pos + 1 != end){
        return * (pos + 1);
    }
    return nullptr;
}

// Using arguments here to remember how to use them nicely
void printThreadInfo(int number, bool useCout){
    if (useCout){
        std::cout << number << " Hello World!" << std::endl;
    } else {
        printf("%d Hello World!\n", number);
    }
}

int main(int argc, char * argv[]){
    // Read inputs naively
    std::string method(getArg(argv, argv+argc, "-t"));

    int numThreads = std::stoi(getArg(argv, argv+argc, "-n"));

    bool useCout = false;
    if (method == "cout"){
        useCout = true;
    }

    // Now we create the threads
    std::thread threads[numThreads];

    for (int i = 0; i < numThreads; i++){
        threads[i] = std::thread(printThreadInfo, i, useCout);
    }

    for (int i = 0; i < numThreads; i++){
        threads[i].join();
    }

    return 0;
}