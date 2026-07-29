#include <iomanip>
#include <iostream>

#include "ag_signal_morpher_1ee27305a6aa_kernel.hpp"

int main() {
    double value = 0.0;
    std::cout << std::setprecision(17);
    while (std::cin >> value) {
        std::cout
            << ag_signal_morpher_1ee27305a6aa_kernel::kernel(value)
            << '\n';
    }
    return 0;
}
