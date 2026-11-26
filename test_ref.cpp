#include <iomanip>
#include <iostream>
#include "cauchySketch.hpp"
//Implementación de prueba de valores de referencia de la tarea
int main() {
    std::cout << "Mezclar(0) = 0x" << std::hex << std::uppercase
              << CauchySketch::mezclar(0) << std::dec << "\n";

    uint64_t ip = 0xC0A80001ULL; // 192.168.0.1
    std::cout << std::fixed << std::setprecision(9);
    for (uint64_t j = 1; j <= 3; ++j) {
        std::cout << "Entrada(42, " << j << ", ip) = "
                  << CauchySketch::entrada(42, j, ip) << "\n";
    }
    return 0;
}