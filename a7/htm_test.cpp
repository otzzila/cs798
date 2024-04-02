#include <immintrin.h>

int main() {
    if (_xbegin() == _XBEGIN_STARTED) _xend();
    return 0;
}

// compile with "g++ -mrtm htm_test.cpp"
