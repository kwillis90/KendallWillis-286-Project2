#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

static uint32_t parse_bin32(const std::string& s) {
    if (s.size() != 32) throw std::runtime_error("Line is not 32 bits");
    uint32_t x = 0;
    for (char c : s) {
        x <<= 1;
        if (c == '1') x |= 1u;
        else if (c != '0') throw std::runtime_error("Invalid character in bitstring");
    }
    return x;
}

static void write_u32_be(std::ofstream& out, uint32_t x) {
    unsigned char b[4];
    b[0] = static_cast<unsigned char>((x >> 24) & 0xFF);
    b[1] = static_cast<unsigned char>((x >> 16) & 0xFF);
    b[2] = static_cast<unsigned char>((x >> 8) & 0xFF);
    b[3] = static_cast<unsigned char>((x >> 0) & 0xFF);
    out.write(reinterpret_cast<char*>(b), 4);
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: txt2bin <input_bits.txt> <output.bin>\n";
        return 1;
    }

    std::ifstream in(argv[1]);
    if (!in) throw std::runtime_error("Could not open input text file");

    std::ofstream out(argv[2], std::ios::binary);
    if (!out) throw std::runtime_error("Could not open output bin file");

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        uint32_t w = parse_bin32(line);
        write_u32_be(out, w);
    }
    return 0;
}