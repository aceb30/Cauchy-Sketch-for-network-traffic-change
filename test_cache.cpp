#include <cmath>
#include <iostream>
#include <vector>
#include "cauchySketch.hpp"
#include "cache.hpp"
//Test para verificar que se cumple la invariabilidad
//Criterio de aceptación
//Diferencia máxima debe ser del orden 1e-13 o -14, o directamente 
//phi_B debe ser menor a 1
int main() {
    const std::size_t k = 128;
    const uint64_t sigma = 42;

    CauchySketch exacto(k, sigma);
    CauchySketch con_cache(k, sigma);
    DirectCache cache(4); // B pequeño para forzar muchas colisiones

    // Incluye 0.0.0.0 (ip = 0) y varias repeticiones
    std::vector<uint32_t> ips = {
        0, 0xC0A80001, 0x08080808, 0, 0xC0A80001,
        0x01010101, 0x0A000001, 0xC0A80001, 0x08080808, 0
    };

    for (uint32_t ip : ips) {
        exacto.actualizar(ip, 1);
        cache.registrar(con_cache, ip);
    }
    cache.vaciar(con_cache);

    double max_diff = 0.0;
    for (std::size_t j = 0; j < k; ++j) {
        max_diff = std::max(max_diff, std::abs(exacto.vectorY()[j] - con_cache.vectorY()[j]));
    }

    std::cout << "Diferencia maxima (redondeo fp): " << max_diff << "\n";
    std::cout << "Paquetes: " << cache.paquetes()
              << " | Actualizar: " << cache.llamadasActualizar()
              << " | phi_B: " << cache.phi() << "\n";
    return 0;
}