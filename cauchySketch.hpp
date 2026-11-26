#ifndef CAUCHY_SKETCH_H
#define CAUCHY_SKETCH_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

class CauchySketch {
public:
    static constexpr double PI = 3.14159265358979323846;
    static constexpr double INV_2_52 = 1.0 / static_cast<double>(1ULL << 52);

    // Constructor: equivale a Inicializar(k, sigma)
    CauchySketch(std::size_t k, uint64_t sigma)
        : k_(k), sigma_(sigma), y_(k, 0.0), row_seeds_(k) {
        //Precalculamos las mezclas de semilla y de fila, como fue sugerido en la tarea
        uint64_t s = mezclar(sigma_);
        for (std::size_t idx = 0; idx < k_; ++idx) {
            // j es 1-indexado en el pseudocódigo (1..k), para hacer matching 1 a 1 lo replicamos acá
            uint64_t j = static_cast<uint64_t>(idx + 1);
            row_seeds_[idx] = mezclar(s ^ j);
        }
    }

    // SplitMix64 (aritmética módulo 2^64 automática en uint64_t)
    static uint64_t mezclar(uint64_t z) {
        z += 0x9E3779B97F4A7C15ULL;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }

    // Entrada(sigma, j, i) con j >= 1 
    // NOTA: En realidad la lógica optimizada de entrada vive en actualizar
    // Esta función es para calcular valores de referencia, en caso de ser necesario
    static double entrada(uint64_t sigma, uint64_t j, uint64_t ip) {
        uint64_t s = mezclar(sigma);
        uint64_t g = mezclar(s ^ j);
        uint64_t h = mezclar(g ^ ip);
        return hashACauchy(h);
    }

    // Actualizar(S, i, c): suma c veces la columna de la IP i
    void actualizar(uint32_t ip, uint64_t c = 1) {
        double c_double = static_cast<double>(c);
        uint64_t ip64 = static_cast<uint64_t>(ip);
        for (std::size_t idx = 0; idx < k_; ++idx) {
            //Acá se resume toda la lógica de ENTRADA de verdad, usando las semillas precalculadas
            uint64_t h = mezclar(row_seeds_[idx] ^ ip64);
            y_[idx] += c_double * hashACauchy(h);
        }
    }

    // Reinicia el vector y a ceros para reutilizar el sketch en el siguiente intervalo
    void reiniciar() {
        std::fill(y_.begin(), y_.end(), 0.0);
    }

    // EstimarDistancia(Sa, Sb) usando las primeras k_prime filas (por defecto todas)
    static double estimarDistancia(const CauchySketch& a, const CauchySketch& b,
                                   std::size_t k_prime = 0) {
        if (a.k_ != b.k_ || a.sigma_ != b.sigma_) {
            throw std::invalid_argument("Los sketches deben tener el mismo k y la misma semilla.");
        }
        if (k_prime == 0) {
            k_prime = a.k_;
        }
        if (k_prime > a.k_) {
            throw std::invalid_argument("k_prime no puede ser mayor que k.");
        }

        std::vector<double> d(k_prime);
        for (std::size_t idx = 0; idx < k_prime; ++idx) {
            d[idx] = std::abs(a.y_[idx] - b.y_[idx]);
        }

        //Cálculo de la mediana
        std::sort(d.begin(), d.end());
        if (k_prime % 2 == 1) {
            return d[k_prime / 2];
        }
        return 0.5 * (d[k_prime / 2 - 1] + d[k_prime / 2]);
    }

    //getters de sketch
    std::size_t k() const { return k_; }
    uint64_t sigma() const { return sigma_; }
    const std::vector<double>& vectorY() const { return y_; }

private:
    std::size_t k_;
    uint64_t sigma_;
    std::vector<double> y_;
    std::vector<uint64_t> row_seeds_; // g precalculados para j = 1..k

    static double hashACauchy(uint64_t h) {
        double u = (static_cast<double>(h >> 12) + 0.5) * INV_2_52;
        return std::tan(PI * (u - 0.5));
    }
};

#endif // CAUCHY_SKETCH_H