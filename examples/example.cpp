#include <iostream>

int main() {
    int x = 2;
    double factor = 1.5;
    bool shouldPrint = true;

    x = x + 3;

    if (x >= 5 && shouldPrint) {
        std::cout << x * factor << std::endl;
    } else {
        std::cout << 0 << std::endl;
    }

    while (x < 7) {
        x = x + 1;
    }

    std::cout << x << std::endl;
    return 0;
}
