#ifndef CACHE_HPP
#define CACHE_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

#include "cauchySketch.hpp"

class DirectCache {
public:
    struct Entry {
        uint32_t ip = 0;
        uint64_t cuenta = 0; // cuenta == 0 indica casilla vacía
    };

    // Equivale a InicializarCaché(B)
    explicit DirectCache(std::size_t B) : B_(B), tabla_(B) {}

    // Equivale a Registrar(S, K, i)
    void registrar(CauchySketch& sketch, uint32_t ip) {
        ++paquetes_;

        // Caso B = 0: sin caché (cada paquete actualiza el sketch directamente)
        if (B_ == 0) {
            sketch.actualizar(ip, 1);
            ++llamadas_actualizar_;
            return;
        }

        std::size_t idx = static_cast<std::size_t>(
            CauchySketch::mezclar(static_cast<uint64_t>(ip)) % B_
        );

        Entry& slot = tabla_[idx];
        if (slot.cuenta != 0) {
            if (slot.ip == ip) {
                ++slot.cuenta;
            } else {
                sketch.actualizar(slot.ip, slot.cuenta);
                ++llamadas_actualizar_;
                slot.ip = ip;
                slot.cuenta = 1;
            }
        } else {
            slot.ip = ip;
            slot.cuenta = 1;
        }
    }

    // Equivale a Vaciar(S, K)
    void vaciar(CauchySketch& sketch) {
        for (Entry& slot : tabla_) {
            if (slot.cuenta != 0) {
                sketch.actualizar(slot.ip, slot.cuenta);
                ++llamadas_actualizar_;
                slot.ip = 0;
                slot.cuenta = 0;
            }
        }
    }

    // Métricas para phi_B
    uint64_t paquetes() const { return paquetes_; }
    uint64_t llamadasActualizar() const { return llamadas_actualizar_; }

    double phi() const {
        if (paquetes_ == 0) return 0.0;
        return static_cast<double>(llamadas_actualizar_) /
               static_cast<double>(paquetes_);
    }

    void reiniciarMetricas() {
        paquetes_ = 0;
        llamadas_actualizar_ = 0;
    }

    std::size_t capacidad() const { return B_; }

private:
    std::size_t B_;
    std::vector<Entry> tabla_;
    uint64_t paquetes_ = 0;
    uint64_t llamadas_actualizar_ = 0;
};

#endif // CACHE_HPP