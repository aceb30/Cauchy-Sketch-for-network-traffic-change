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
#include <unordered_map>

//Este código corresponde a la actividad 1.3
//Calcula Nq, Lq y rq de forma exacta para todos los intervalos de largo T de la traza
//Emite un csv limpio q,N_q,L_q,r_q
//Para $q = 1$ deja L_q y r_q vacíos (que pandas lee automáticamente como NaN).

#pragma pack(push, 1)
struct Record {
    uint64_t ts_us;
    uint32_t src, dst;
    uint16_t sport, dport, len;
    uint8_t proto, flags;
};
#pragma pack(pop)
static_assert(sizeof(Record) == 24, "Record debe ocupar 24 bytes");

//std::unordered_map implementa una tabla hash con separate chaining
//Separate chaining: colisiones se manejan con listas enlazadas
//su costo en memoria es proporcional al número de ip's distintas de un intervalo
//a cambio de esta gran memoria, es exacta
using FreqMap = std::unordered_map<uint32_t, uint64_t>;

static uint64_t calcular_l1(const FreqMap& curr, const FreqMap& prev) {
    uint64_t l1 = 0;
    for (const auto& [ip, c_curr] : curr) {
        auto it = prev.find(ip);
        //suma el valor ans si la ip estaba en el hash previo
        //si no, directamente suma las cuentas de la ip
        if (it != prev.end()) {
            uint64_t c_prev = it->second;
            l1 += (c_curr >= c_prev) ? (c_curr - c_prev) : (c_prev - c_curr);
        } else {
            l1 += c_curr;
        }
    }
    for (const auto& [ip, c_prev] : prev) {
        if (curr.find(ip) == curr.end()) {
            l1 += c_prev;
        }
    }
    return l1;
}

static void emitir_intervalo(uint64_t q, uint64_t n_curr, uint64_t n_prev,
                             const FreqMap& curr, const FreqMap& prev) {
    if (q == 1) {
        std::cout << q << "," << n_curr << ",,\n";
        return;
    }
    uint64_t l_q = calcular_l1(curr, prev);
    uint64_t denom = n_curr + n_prev;
    if (denom > 0) {
        double r_q = static_cast<double>(l_q) / static_cast<double>(denom);
        std::cout << q << "," << n_curr << "," << l_q << ","
                  << std::setprecision(10) << r_q << "\n";
    } else {
        std::cout << q << "," << n_curr << "," << l_q << ",\n";
    }
}

int main(int argc, char** argv) {
    std::string trace_path;
    double T_sec = 10.0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--trace" && i + 1 < argc) trace_path = argv[++i];
        else if (arg == "--T" && i + 1 < argc) T_sec = std::stod(argv[++i]);
    }

    //mismo proceso de lectura que en cauchy_cli.cpp

    if (trace_path.empty()) {
        std::cerr << "Uso: " << argv[0] << " --trace <traza.bin> [--T 10]\n";
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
    if (n_packets == 0) {
        munmap(p, st.st_size);
        return 0;
    }

    const uint64_t T_us = static_cast<uint64_t>(T_sec * 1e6);
    const uint64_t t0 = trace[0].ts_us;

    //creamos los hashes exactos para el intervalo previo y el actual
    //de ahí salen nuestras cuentas
    FreqMap prev_map, curr_map;
    uint64_t prev_N = 0, curr_N = 0;
    uint64_t cur_q = 1;

    std::cout << "q,N_q,L_q,r_q\n";

    for (std::size_t i = 0; i < n_packets; ++i) {
        uint64_t ts = trace[i].ts_us;
        // Intervalo q >= 1 cubre (t0 + (q-1)T, t0 + qT], primer paquete en q=1
        uint64_t pkt_q = (ts <= t0) ? 1 : ((ts - t0 - 1) / T_us + 1);

        while (pkt_q > cur_q) {
            emitir_intervalo(cur_q, curr_N, prev_N, curr_map, prev_map);
            prev_map = std::move(curr_map);
            curr_map.clear();
            prev_N = curr_N;
            curr_N = 0;
            ++cur_q;
        }

        ++curr_map[trace[i].dst];
        ++curr_N;
    }

    // Emitir el último intervalo
    emitir_intervalo(cur_q, curr_N, prev_N, curr_map, prev_map);

    munmap(p, st.st_size);
    return 0;
}