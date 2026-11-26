#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <iomanip>
#include <iostream>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "cauchySketch.hpp"
#include "cache.hpp"

//Este ejecutable recibe los parámetros por línea de comandos 
//Incluyendo --B, --K, --sigma y --T
//Devuelve los resultados de los experimentos correspondientes a la actividad 1
//La gracia es que así la parte de python simplemente lo compila y lo invoca con subprocess
//Similar a la tarea 1

//tiene 4 modos, uno corresponde a cada prueba de la actividad 1.2

#pragma pack(push, 1)
struct Record {
    uint64_t ts_us;
    uint32_t src, dst;
    uint16_t sport, dport, len;
    uint8_t proto, flags;
};
#pragma pack(pop)
static_assert(sizeof(Record) == 24, "Record debe ocupar 24 bytes");

int main(int argc, char** argv) {
    std::string mode;
    std::string trace_path;
    std::size_t k = 512;
    std::size_t B = 4096;
    uint64_t sigma = 42;
    uint64_t n_samples = 1000000;
    uint64_t c_count = 100000;
    uint64_t ip = 0xC0A80001ULL;
    uint64_t j_row = 1;
    double T_sec = 10.0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--mode" && i + 1 < argc) mode = argv[++i];
        else if (arg == "--trace" && i + 1 < argc) trace_path = argv[++i];
        else if (arg == "--k" && i + 1 < argc) k = std::stoull(argv[++i]);
        else if (arg == "--B" && i + 1 < argc) B = std::stoull(argv[++i]);
        else if (arg == "--sigma" && i + 1 < argc) sigma = std::stoull(argv[++i]);
        else if (arg == "--n" && i + 1 < argc) n_samples = std::stoull(argv[++i]);
        else if (arg == "--c" && i + 1 < argc) c_count = std::stoull(argv[++i]);
        else if (arg == "--ip" && i + 1 < argc) ip = std::stoull(argv[++i], nullptr, 0);
        else if (arg == "--j" && i + 1 < argc) j_row = std::stoull(argv[++i]);
        else if (arg == "--T" && i + 1 < argc) T_sec = std::stod(argv[++i]);
    }

    std::cout << std::setprecision(15);

    // Prueba 1: mediana(|C_ji|) sobre n_samples entradas
    if (mode == "median") {
        std::vector<double> vals(n_samples);
        //Generamos n_samples entradas a partir de la función entrada del sketch
        //Esto recorre todo el array, notemos que mueve en j e i
        //Pasando como parámetro --k 1, podemos comprobar mediana = 1 en una sola fila
        //Pero si recorremos todo de una, comprobamos en una sola pasada mediana = 1 e independencia de paso
        for (uint64_t idx = 0; idx < n_samples; ++idx) {
            uint64_t j = (idx % k) + 1;
            uint64_t ip_i = idx / k;
            vals[idx] = std::abs(CauchySketch::entrada(sigma, j, ip_i));
        }
        //Cálculo de la mediana
        std::size_t mid = n_samples / 2;
        std::nth_element(vals.begin(), vals.begin() + mid, vals.end());
        double med = vals[mid];
        if (n_samples % 2 == 0) {
            double left_max = *std::max_element(vals.begin(), vals.begin() + mid);
            med = 0.5 * (left_max + med);
        }
        std::cout << "{\"n\": " << n_samples << ", \"median\": " << med << "}\n";
        return 0;
    }

    // Prueba 2: determinismo de Entrada(sigma, j, ip)
    if (mode == "determinism") {
        double first = CauchySketch::entrada(sigma, j_row, ip);
        bool all_equal = true;
        //Repetimos muchas veces la llamada a entrada para confirmar determinismo
        for (uint64_t r = 0; r < n_samples; ++r) {
            if (CauchySketch::entrada(sigma, j_row, ip) != first) {
                all_equal = false;
                break;
            }
        }
        std::cout << "{\"value\": " << first
                  << ", \"all_equal\": " << (all_equal ? "true" : "false")
                  << ", \"reps\": " << n_samples << "}\n";
        return 0;
    }

    // Prueba 3: una sola IP con cuenta c contra sketch vacío
    if (mode == "single-ip") {
        //Hacemos lo solicitado
        //Creamos un sketch vacío y uno con una IP
        CauchySketch s_ip(k, sigma);
        CauchySketch s_empty(k, sigma);
        s_ip.actualizar(static_cast<uint32_t>(ip), c_count);
        //Estimamos L y comprobamos que sea c
        double l_hat = CauchySketch::estimarDistancia(s_ip, s_empty);
        std::cout << "{\"k\": " << k << ", \"sigma\": " << sigma
                  << ", \"c\": " << c_count << ", \"L_hat\": " << l_hat << "}\n";
        return 0;
    }

    // Prueba 4: par de intervalos consecutivos (q=1 y q=2) en traza real con caché B
    //Esto no es la prueba directamente
    //En validar_sketch.py llamamos a esta función con B = 0 y B normal
    //Y ahí comparamos el L que entrega esta función
    if (mode == "interval-pair") {
        if (trace_path.empty()) {
            std::cerr << "Error: falta --trace <archivo.bin>\n";
            return 1;
        }

        int fd = open(trace_path.c_str(), O_RDONLY);
        if (fd < 0) { perror("open"); return 1; }
        struct stat st;
        if (fstat(fd, &st) < 0) { perror("fstat"); close(fd); return 1; }

        void* p = mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (p == MAP_FAILED) { perror("mmap"); close(fd); return 1; }
        close(fd);
        madvise(p, st.st_size, MADV_SEQUENTIAL);

        const Record* trace = static_cast<const Record*>(p);
        std::size_t n_packets = st.st_size / sizeof(Record);

        uint64_t T_us = static_cast<uint64_t>(T_sec * 1e6);
        uint64_t t0 = trace[0].ts_us;
        uint64_t t1 = t0 + T_us;
        uint64_t t2 = t0 + 2 * T_us;

        //Creamos los dos sketches y el caché necesario

        CauchySketch s1(k, sigma);
        CauchySketch s2(k, sigma);
        DirectCache cache(B);

        uint64_t n1 = 0, n2 = 0;
        bool flushed_s1 = false;

        //Stream! 

        for (std::size_t idx = 0; idx < n_packets; ++idx) {
            uint64_t ts = trace[idx].ts_us;
            uint32_t dst_ip = trace[idx].dst;

            if (ts <= t1) {
                cache.registrar(s1, dst_ip);
                ++n1;
            } else if (ts <= t2) {
                if (!flushed_s1) {
                    cache.vaciar(s1);
                    flushed_s1 = true;
                }
                cache.registrar(s2, dst_ip);
                ++n2;
            } else {
                break; // Ya pasamos el intervalo 2
            }
        }
        if (!flushed_s1) cache.vaciar(s1);
        cache.vaciar(s2);

        double l_hat = CauchySketch::estimarDistancia(s1, s2);
        std::cout << "{\"B\": " << B
                  << ", \"k\": " << k
                  << ", \"T\": " << T_sec
                  << ", \"N1\": " << n1
                  << ", \"N2\": " << n2
                  << ", \"L_hat\": " << l_hat
                  << ", \"updates\": " << cache.llamadasActualizar()
                  << ", \"phi_B\": " << cache.phi() << "}\n";

        munmap(p, st.st_size);
        return 0;
    }

    std::cerr << "Modo invalido. Use: median | determinism | single-ip | interval-pair\n";
    return 1;
}