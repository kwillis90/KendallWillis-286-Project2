#include <array>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using std::cerr;

// PROMPT: Read one 32-bit number from the binary file.
// The file stores numbers in big-endian order, so we combine 4 bytes manually.
// If we cannot read all 4 bytes, stop the program with an error.
static uint32_t read_u32_be(std::ifstream &in) {
    unsigned char b[4];
    in.read(reinterpret_cast<char *>(b), 4);
    if (!in) throw std::runtime_error("Unexpected EOF while reading 32-bit word");
    return (static_cast<uint32_t>(b[0]) << 24) |
           (static_cast<uint32_t>(b[1]) << 16) |
           (static_cast<uint32_t>(b[2]) << 8) |
           static_cast<uint32_t>(b[3]);
}

// PROMPT: Convert a 32-bit number into exactly 32 binary characters.
// This is only used for printing the same simulator output format.
static std::string bin32(uint32_t x) {
    std::string s(32, '0');
    for (int i = 31; i >= 0; --i) s[31 - i] = ((x >> i) & 1u) ? '1' : '0';
    return s;
}

// PROMPT: Convert a 16-bit immediate into a signed 32-bit value.
// MIPS uses this for addi, addiu, lw, sw, beq, and bne.
static int32_t sign_extend_16(uint16_t imm) {
    return static_cast<int32_t>(static_cast<int16_t>(imm));
}

struct ProgramImage {
    uint32_t initial_pc = 0;
    uint32_t data_base = 0;
    uint32_t initial_sp = 0;
    uint32_t data_words_count = 0;
    std::vector<uint32_t> data_words;
    std::vector<uint32_t> instr_words;
};

// PROMPT: Load the binary program file into a ProgramImage object.
// File format:
//   initialPC, dataBase, initialSP, dataWordsCount, data words..., instruction words...
// The first part becomes the data section. Everything after that becomes instructions.
static ProgramImage load_program_be(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Could not open input file: " + path);

    ProgramImage img;
    img.initial_pc = read_u32_be(in);
    img.data_base = read_u32_be(in);
    img.initial_sp = read_u32_be(in);
    img.data_words_count = read_u32_be(in);

    img.data_words.reserve(img.data_words_count);
    for (uint32_t i = 0; i < img.data_words_count; ++i) {
        img.data_words.push_back(read_u32_be(in));
    }

    while (true) {
        int c = in.peek();
        if (c == std::char_traits<char>::eof()) break;
        img.instr_words.push_back(read_u32_be(in));
    }

    if (img.instr_words.empty()) {
        throw std::runtime_error("No instructions found after data segment.");
    }

    return img;
}

// PROMPT: Turn a register number like 8 into a MIPS name like $t0.
// This is used only when printing readable assembly output.
static const char *reg_name(uint32_t r) {
    static const char *names[32] = {
        "$zero", "$at", "$v0", "$v1",
        "$a0",   "$a1", "$a2", "$a3",
        "$t0",   "$t1", "$t2", "$t3",
        "$t4",   "$t5", "$t6", "$t7",
        "$s0",   "$s1", "$s2", "$s3",
        "$s4",   "$s5", "$s6", "$s7",
        "$t8",   "$t9", "$k0", "$k1",
        "$gp",   "$sp", "$fp", "$ra"};
    return (r < 32) ? names[r] : "$?";
}

// PROMPT: Decode one 32-bit instruction into readable MIPS assembly text.
// This function does not execute the instruction. It only creates the text shown in output.
static std::string decode_instr(uint32_t instr, uint32_t pc) {
    uint32_t opcode = (instr >> 26) & 0x3Fu;
    uint32_t rs = (instr >> 21) & 0x1Fu;
    uint32_t rt = (instr >> 16) & 0x1Fu;
    uint32_t rd = (instr >> 11) & 0x1Fu;
    uint32_t shamt = (instr >> 6) & 0x1Fu;
    uint32_t funct = instr & 0x3Fu;
    uint16_t imm16 = static_cast<uint16_t>(instr & 0xFFFFu);
    uint32_t targ26 = instr & 0x03FFFFFFu;

    std::ostringstream out;
    if (instr == 0x0000000Cu) return "syscall";

    switch (opcode) {
        case 0x00:
            switch (funct) {
                case 0x20: out << "add " << reg_name(rd) << ", " << reg_name(rs) << ", " << reg_name(rt); return out.str();
                case 0x21: out << "addu " << reg_name(rd) << ", " << reg_name(rs) << ", " << reg_name(rt); return out.str();
                case 0x2A: out << "slt " << reg_name(rd) << ", " << reg_name(rs) << ", " << reg_name(rt); return out.str();
                case 0x00: out << "sll " << reg_name(rd) << ", " << reg_name(rt) << ", " << shamt; return out.str();
                case 0x08: out << "jr " << reg_name(rs); return out.str();
                default: return "nop";
            }
        case 0x08: out << "addi " << reg_name(rt) << ", " << reg_name(rs) << ", " << sign_extend_16(imm16); return out.str();
        case 0x09: out << "addiu " << reg_name(rt) << ", " << reg_name(rs) << ", " << sign_extend_16(imm16); return out.str();
        case 0x0D: out << "ori " << reg_name(rt) << ", " << reg_name(rs) << ", " << static_cast<uint32_t>(imm16); return out.str();
        case 0x0F: out << "lui " << reg_name(rt) << ", " << static_cast<uint32_t>(imm16); return out.str();
        case 0x23: out << "lw " << reg_name(rt) << ", " << sign_extend_16(imm16) << "(" << reg_name(rs) << ")"; return out.str();
        case 0x2B: out << "sw " << reg_name(rt) << ", " << sign_extend_16(imm16) << "(" << reg_name(rs) << ")"; return out.str();
        case 0x04: out << "beq " << reg_name(rs) << ", " << reg_name(rt) << ", " << sign_extend_16(imm16); return out.str();
        case 0x05: out << "bne " << reg_name(rs) << ", " << reg_name(rt) << ", " << sign_extend_16(imm16); return out.str();
        case 0x02: {
            uint32_t target = ((pc + 4) & 0xF0000000u) | (targ26 << 2);
            out << "j " << target;
            return out.str();
        }
        case 0x03: {
            uint32_t target = ((pc + 4) & 0xF0000000u) | (targ26 << 2);
            out << "jal " << target;
            return out.str();
        }
        case 0x1C:
            if (funct == 0x02) {
                out << "mul " << reg_name(rd) << ", " << reg_name(rs) << ", " << reg_name(rt);
                return out.str();
            }
            return "nop";
        default:
            return "nop";
    }
}

// PROMPT: Print the first part of the output file.
// This includes the program header, data section, and decoded instructions.
static void write_decoder_output(std::ostream &out, const ProgramImage &img) {
    out << "initialPC: " << img.initial_pc << "\n";
    out << "dataStartAddress: " << img.data_base << "\n";
    out << "initialStackPointer: " << img.initial_sp << "\n";
    out << "Number of data items: " << img.data_words_count << "\n";

    for (uint32_t i = 0; i < img.data_words.size(); ++i) {
        uint32_t addr = img.data_base + i * 4;
        out << addr << ":\t" << bin32(img.data_words[i]) << "\t"
            << static_cast<int32_t>(img.data_words[i]) << "\n";
    }

    for (uint32_t i = 0; i < img.instr_words.size(); ++i) {
        uint32_t pc = img.initial_pc + i * 4;
        uint32_t instr = img.instr_words[i];
        out << pc << ":\t" << bin32(instr) << "\t" << decode_instr(instr, pc) << "\n";
    }
}

// PROMPT: Store the CPU state.
// pc is the program counter. regs holds the 32 MIPS registers.
struct Machine {
    uint32_t pc = 0;
    int32_t regs[32]{};
};

// PROMPT: Store one L1 cache line.
// Each L1 line has 2 words, a valid bit, a dirty bit, and a tag.
struct L1Line {
    bool valid = false;
    bool dirty = false;
    uint32_t tag = 0;
    std::array<uint32_t, 2> words{0, 0};
};

// PROMPT: Store one L2 cache line.
// Each L2 line has 4 words, a valid bit, a dirty bit, and a tag.
struct L2Line {
    bool valid = false;
    bool dirty = false;
    uint32_t tag = 0;
    std::array<uint32_t, 4> words{0, 0, 0, 0};
};

// PROMPT: Track a cache miss that takes more than one cycle.
// The miss may first move data from memory to L2, then from L2 to L1.
struct PendingFill {
    bool active = false;
    int remaining = 0;
    bool to_l1 = false;
    bool to_l2 = false;
    bool is_instr = false;
    uint32_t addr = 0;
    std::array<uint32_t, 4> l2_words{0, 0, 0, 0};
    std::array<uint32_t, 2> l1_words{0, 0};
};

// PROMPT: Store the entire memory system for the simulator.
// This includes backing memory, stack memory, L1 instruction cache, L1 data cache, L2 cache,
// LRU bits, and pending cache misses.
struct MemorySystem {
    const ProgramImage &img;
    std::vector<int32_t> data_mem;
    std::map<uint32_t, int32_t> stack_mem;
    std::array<L1Line, 2> l1i;
    std::array<L1Line, 2> l1d;
    std::array<std::array<L2Line, 2>, 2> l2{};
    std::array<int, 2> lru_bits{1, 0};
    PendingFill pending_if;
    PendingFill pending_mem;

    // Initialize the backing data memory from the loaded program image.
    explicit MemorySystem(const ProgramImage &p) : img(p) {
        data_mem.reserve(img.data_words.size());
        for (uint32_t w : img.data_words) data_mem.push_back(static_cast<int32_t>(w));
    }

    // Check whether an address belongs to the static data segment.
    bool addr_in_data(uint32_t addr) const {
        uint32_t start = img.data_base;
        uint32_t end_exclusive = img.data_base + static_cast<uint32_t>(img.data_words.size()) * 4u;
        return addr >= start && addr < end_exclusive && (addr % 4u == 0u);
    }

    // Read a word from backing memory, which may be a data location, stack location, or
    // instruction memory if the address lies inside the loaded text segment.
    uint32_t load_word_backing(uint32_t addr) const {
        if (addr_in_data(addr)) {
            uint32_t idx = (addr - img.data_base) / 4u;
            return static_cast<uint32_t>(data_mem.at(idx));
        }
        auto it = stack_mem.find(addr);
        if (it != stack_mem.end()) return static_cast<uint32_t>(it->second);
        if (addr >= img.initial_pc && ((addr - img.initial_pc) % 4u == 0u)) {
            uint32_t idx = (addr - img.initial_pc) / 4u;
            if (idx < img.instr_words.size()) return img.instr_words[idx];
        }
        return 0;
    }

    // Write a word back into backing memory.
    // Data writes go to the data segment, while other addresses are stored in the stack map.
    void store_word_backing(uint32_t addr, uint32_t value) {
        if (addr_in_data(addr)) {
            uint32_t idx = (addr - img.data_base) / 4u;
            data_mem.at(idx) = static_cast<int32_t>(value);
            return;
        }
        stack_mem[addr] = static_cast<int32_t>(value);
    }

    // Cache address helpers for block base, set index, tag, and word offsets.
    static uint32_t l1_block_base(uint32_t addr) { return addr & ~0x7u; }
    static uint32_t l2_block_base(uint32_t addr) { return addr & ~0xFu; }
    static uint32_t l1_index(uint32_t addr) { return (addr >> 3) & 0x1u; }
    static uint32_t l1_tag(uint32_t addr) { return addr >> 4; }
    static uint32_t l1_word_offset(uint32_t addr) { return (addr >> 2) & 0x1u; }
    static uint32_t l2_index(uint32_t addr) { return (addr >> 4) & 0x1u; }
    static uint32_t l2_tag(uint32_t addr) { return addr >> 5; }
    static uint32_t l2_word_offset(uint32_t addr) { return (addr >> 2) & 0x3u; }

    // Write back a dirty L1 line into L2 when the L1 line is evicted.
    // If the corresponding L2 block is not present, allocate or evict an L2 way,
    // write any dirty L2 victim back to backing memory, then fill the L2 block
    // from backing memory before updating the half corresponding to the L1 line.
    void write_back_l1_line_to_l2(const L1Line &line, uint32_t evict_index) {
        if (!line.valid || !line.dirty) return;
        uint32_t block_base = (line.tag << 4) | (evict_index << 3);
        uint32_t set = l2_index(block_base);
        uint32_t tag = l2_tag(block_base);

        int hit_way = -1;
        for (int w = 0; w < 2; ++w) {
            if (l2[set][w].valid && l2[set][w].tag == tag) hit_way = w;
        }
        int way = hit_way;
        if (way == -1) {
            if (!l2[set][0].valid) way = 0;
            else if (!l2[set][1].valid) way = 1;
            else way = lru_bits[set];
            if (l2[set][way].valid && l2[set][way].dirty) {
                uint32_t old_base = (l2[set][way].tag << 5) | (set << 4);
                for (int i = 0; i < 4; ++i) store_word_backing(old_base + static_cast<uint32_t>(i * 4), l2[set][way].words[i]);
            }
            l2[set][way].valid = true;
            l2[set][way].dirty = false;
            l2[set][way].tag = tag;
            std::array<uint32_t, 4> full{};
            uint32_t l2base = l2_block_base(block_base);
            for (int i = 0; i < 4; ++i) full[i] = load_word_backing(l2base + static_cast<uint32_t>(i * 4));
            l2[set][way].words = full;
        }

        uint32_t sub = (block_base >> 3) & 0x1u;
        l2[set][way].words[sub * 2u + 0u] = line.words[0];
        l2[set][way].words[sub * 2u + 1u] = line.words[1];
        l2[set][way].dirty = true;
        lru_bits[set] = (way == 0) ? 1 : 0;
    }

    // Read a full L2 block from backing memory.
    std::array<uint32_t, 4> get_l2_block_from_memory(uint32_t addr) const {
        std::array<uint32_t, 4> blk{};
        uint32_t base = l2_block_base(addr);
        for (int i = 0; i < 4; ++i) blk[i] = load_word_backing(base + static_cast<uint32_t>(i * 4));
        return blk;
    }

    // Extract the 2-word L1 line from the 4-word L2 block corresponding to the address.
    std::array<uint32_t, 2> extract_l1_words_from_l2(uint32_t addr, const std::array<uint32_t, 4> &blk) const {
        uint32_t half = (addr >> 3) & 0x1u;
        return {blk[half * 2u], blk[half * 2u + 1u]};
    }

    // Lookup an address in L2. If present, return the full L2 block and update LRU.
    bool l2_lookup(uint32_t addr, std::array<uint32_t, 4> &blk_out) {
        uint32_t set = l2_index(addr);
        uint32_t tag = l2_tag(addr);
        for (int w = 0; w < 2; ++w) {
            if (l2[set][w].valid && l2[set][w].tag == tag) {
                blk_out = l2[set][w].words;
                lru_bits[set] = (w == 0) ? 1 : 0;
                return true;
            }
        }
        return false;
    }

    // Install a full 4-word block into L2.
    // Evicts the selected way if needed, writing it back to memory if dirty.
    void install_l2_block(uint32_t addr, const std::array<uint32_t, 4> &blk) {
        uint32_t set = l2_index(addr);
        uint32_t tag = l2_tag(addr);
        int way;
        if (!l2[set][0].valid) way = 0;
        else if (!l2[set][1].valid) way = 1;
        else way = lru_bits[set];

        if (l2[set][way].valid && l2[set][way].dirty) {
            uint32_t old_base = (l2[set][way].tag << 5) | (set << 4);
            for (int i = 0; i < 4; ++i) store_word_backing(old_base + static_cast<uint32_t>(i * 4), l2[set][way].words[i]);
        }

        l2[set][way].valid = true;
        l2[set][way].dirty = false;
        l2[set][way].tag = tag;
        l2[set][way].words = blk;
        lru_bits[set] = (way == 0) ? 1 : 0;
    }

    // Install a 2-word line into an L1 cache.
    // If the target L1 line is dirty, write it back to L2 first.
    void install_l1(std::array<L1Line, 2> &cache, uint32_t addr, const std::array<uint32_t, 2> &words, bool dirty) {
        uint32_t idx = l1_index(addr);
        if (cache[idx].valid && cache[idx].dirty) write_back_l1_line_to_l2(cache[idx], idx);
        cache[idx].valid = true;
        cache[idx].dirty = dirty;
        cache[idx].tag = l1_tag(addr);
        cache[idx].words = words;
    }

    // Begin an instruction fetch miss sequence.
    // If the block is in L2, schedule a direct L2-to-L1 fill.
    // Otherwise, schedule a memory-to-L2 fill followed by L2-to-L1.
    void start_if_request(uint32_t pc) {
        if (pending_if.active) return;
        std::array<uint32_t, 4> blk{};
        if (l2_lookup(pc, blk)) {
            pending_if.active = true;
            pending_if.remaining = 1;
            pending_if.to_l1 = true;
            pending_if.to_l2 = false;
            pending_if.is_instr = true;
            pending_if.addr = pc;
            pending_if.l1_words = extract_l1_words_from_l2(pc, blk);
        } else {
            pending_if.active = true;
            pending_if.remaining = 1;
            pending_if.to_l1 = false;
            pending_if.to_l2 = true;
            pending_if.is_instr = true;
            pending_if.addr = pc;
            pending_if.l2_words = get_l2_block_from_memory(pc);
        }
    }

    // Begin a data request miss sequence for load or store.
    void start_mem_request(uint32_t addr, bool is_instr) {
        PendingFill &p = pending_mem;
        if (p.active) return;
        std::array<uint32_t, 4> blk{};
        if (l2_lookup(addr, blk)) {
            p.active = true;
            p.remaining = 1;
            p.to_l1 = true;
            p.to_l2 = false;
            p.is_instr = is_instr;
            p.addr = addr;
            p.l1_words = extract_l1_words_from_l2(addr, blk);
        } else {
            p.active = true;
            p.remaining = 1;
            p.to_l1 = false;
            p.to_l2 = true;
            p.is_instr = is_instr;
            p.addr = addr;
            p.l2_words = get_l2_block_from_memory(addr);
        }
    }

    // Advance one pending fill operation by one cycle.
    // Moves from memory-to-L2 and then from L2-to-L1 in consecutive cycles.
    void advance_pending(PendingFill &p) {
        if (!p.active) return;
        --p.remaining;
        if (p.remaining > 0) return;

        if (p.to_l2) {
            install_l2_block(p.addr, p.l2_words);
            p.to_l2 = false;
            p.to_l1 = true;
            p.remaining = 1;
            p.l1_words = extract_l1_words_from_l2(p.addr, p.l2_words);
            return;
        }

        if (p.to_l1) {
            if (p.is_instr) install_l1(l1i, p.addr, p.l1_words, false);
            else install_l1(l1d, p.addr, p.l1_words, false);
            p.active = false;
        }
    }

    // End of cycle operations: update both pending instruction and data fills.
    void end_cycle() {
        advance_pending(pending_if);
        advance_pending(pending_mem);
    }

    // Perform an instruction fetch from L1. If the line is present, return it.
    // Otherwise, start a miss request and return false.
    bool fetch_instruction(uint32_t pc, uint32_t &instr_out) {
        uint32_t idx = l1_index(pc);
        uint32_t tag = l1_tag(pc);
        uint32_t off = l1_word_offset(pc);
        if (l1i[idx].valid && l1i[idx].tag == tag) {
            instr_out = l1i[idx].words[off];
            return true;
        }
        start_if_request(pc);
        return false;
    }

    // Check whether the data word is already in L1 data cache.
    // If it is, return true; otherwise start a fill request.
    bool data_ready(uint32_t addr, uint32_t &word_out) {
        uint32_t idx = l1_index(addr);
        uint32_t tag = l1_tag(addr);
        uint32_t off = l1_word_offset(addr);
        if (l1d[idx].valid && l1d[idx].tag == tag) {
            word_out = l1d[idx].words[off];
            return true;
        }
        start_mem_request(addr, false);
        return false;
    }

    // Load a 32-bit word for execution, returning false if the data path is still pending.
    bool load_word(uint32_t addr, int32_t &value) {
        uint32_t raw = 0;
        if (!data_ready(addr, raw)) return false;
        value = static_cast<int32_t>(raw);
        return true;
    }

    // Store a word into the L1 data cache, marking the line dirty.
    // If the target line is not present, a fill request is started and the store stalls.
    bool store_word(uint32_t addr, int32_t value) {
        uint32_t idx = l1_index(addr);
        uint32_t tag = l1_tag(addr);
        uint32_t off = l1_word_offset(addr);
        if (!(l1d[idx].valid && l1d[idx].tag == tag)) {
            start_mem_request(addr, false);
            return false;
        }
        l1d[idx].words[off] = static_cast<uint32_t>(value);
        l1d[idx].dirty = true;
        return true;
    }
};

// Print the CPU registers in four columns, using MIPS register names.
static void print_registers(std::ostream &out, const Machine &m) {
    for (int row = 0; row < 8; ++row) {
        int i = row * 4;
        out << std::setw(6) << reg_name(i) << ":" << std::setw(17) << m.regs[i] << "\t"
            << std::setw(6) << reg_name(i + 1) << ":" << std::setw(17) << m.regs[i + 1] << "\t"
            << std::setw(6) << reg_name(i + 2) << ":" << std::setw(17) << m.regs[i + 2] << "\t"
            << std::setw(6) << reg_name(i + 3) << ":" << std::setw(17) << m.regs[i + 3] << "\n";
    }
}

// Print the contents of an L1 cache, including valid/dirty bits, tags, and stored words.
static void print_l1_cache(std::ostream &out, const std::string &title, const std::array<L1Line, 2> &cache) {
    out << title << "\n";
    out << "------------------------------------------------------------------------------------------------------\n";
    out << "Idx | V | D |    Tag     |              Word 0              |              Word 1              \n";
    out << "------------------------------------------------------------------------------------------------------\n";
    for (int i = 0; i < 2; ++i) {
        out << std::setw(3) << i << " | " << (cache[i].valid ? 1 : 0) << " | " << (cache[i].dirty ? 1 : 0)
            << " | 0x" << std::hex << std::setw(8) << std::setfill('0') << cache[i].tag << std::dec << std::setfill(' ')
            << " | " << bin32(cache[i].words[0])
            << " | " << bin32(cache[i].words[1]) << "\n";
    }
    out << "------------------------------------------------------------------------------------------------------\n\n";
}

// Print the full L2 cache state, including two ways per set and the LRU metadata.
static void print_l2_cache(std::ostream &out, const MemorySystem &mem) {
    out << "L2 Cache State:\n";
    out << "LRU Bits: line 0 = " << mem.lru_bits[0] << ", line 1 = " << mem.lru_bits[1] << "\n";
    out << "------------------------------------------------------------------------------------------------------\n";
    out << "Way | Idx | V | D |    Tag     |              Word 0              |              Word 1              |              Word 2              |              Word 3              \n";
    out << "------------------------------------------------------------------------------------------------------\n";
    for (int way = 0; way < 2; ++way) {
        for (int idx = 0; idx < 2; ++idx) {
            const auto &line = mem.l2[idx][way];
            out << " " << way << "  |  " << idx << " | " << (line.valid ? 1 : 0) << " | " << (line.dirty ? 1 : 0)
                << " | 0x" << std::hex << std::setw(8) << std::setfill('0') << line.tag << std::dec << std::setfill(' ')
                << " | " << bin32(line.words[0])
                << " | " << bin32(line.words[1])
                << " | " << bin32(line.words[2])
                << " | " << bin32(line.words[3]) << "\n";
        }
    }
    out << "------------------------------------------------------------------------------------------------------\n\n";
}

// Print the simulator's static data segment contents.
static void print_data_section(std::ostream &out, const ProgramImage &img, const MemorySystem &mem) {
    out << "Data Section:\n";
    for (uint32_t i = 0; i < mem.data_mem.size(); ++i) {
        uint32_t addr = img.data_base + i * 4;
        out << addr << ": " << mem.data_mem[i] << "\n";
    }
}

// Print the stack memory contents from the current stack pointer up to the initial SP.
static void print_stack_section(std::ostream &out, uint32_t current_sp, uint32_t initial_sp, const MemorySystem &mem) {
    out << "Stack Section:\n";
    if (current_sp < initial_sp) {
        for (uint32_t addr = current_sp; addr < initial_sp; addr += 4) {
            int32_t val = 0;
            auto it = mem.stack_mem.find(addr);
            if (it != mem.stack_mem.end()) val = it->second;
            out << addr << ": " << val << "\n";
            if (addr > initial_sp - 4) break;
        }
    }
    out << "--------------------------------------------------\n";
}

// PROMPT: Store the instruction currently being shown/executed.
// This keeps the instruction word, its PC, and its decoded text together.
struct LatchedInstr {
    bool valid = false;
    uint32_t pc = 0;
    uint32_t instr = 0;
    std::string text;
};

// PROMPT: Execute one instruction.
// The instruction has already been fetched and stored in LatchedInstr.
// Return true if the instruction finished this cycle.
// Return false if lw/sw caused a cache miss and the simulator must stall.
static bool execute_instruction(Machine &m, MemorySystem &mem, const LatchedInstr &latched, bool &halt) {
    uint32_t instr = latched.instr;
    uint32_t pc = latched.pc;

    uint32_t opcode = (instr >> 26) & 0x3Fu;
    uint32_t rs = (instr >> 21) & 0x1Fu;
    uint32_t rt = (instr >> 16) & 0x1Fu;
    uint32_t rd = (instr >> 11) & 0x1Fu;
    uint32_t shamt = (instr >> 6) & 0x1Fu;
    uint32_t funct = instr & 0x3Fu;
    uint16_t imm16 = static_cast<uint16_t>(instr & 0xFFFFu);
    uint32_t targ26 = instr & 0x03FFFFFFu;

    uint32_t next_pc = pc + 4;

    if (instr == 0x0000000Cu) {
        halt = true;
        m.pc = next_pc;
        m.regs[0] = 0;
        return true;
    }

    switch (opcode) {
        case 0x00: {
            switch (funct) {
                case 0x20: // add
                    if (rd != 0) m.regs[rd] = m.regs[rs] + m.regs[rt];
                    break;

                case 0x21: { // addu
                    if (rd != 0) {
                        uint32_t left = static_cast<uint32_t>(m.regs[rs]);
                        uint32_t right = static_cast<uint32_t>(m.regs[rt]);
                        m.regs[rd] = static_cast<int32_t>(left + right);
                    }
                    break;
                }

                case 0x2A: // slt
                    if (rd != 0) m.regs[rd] = (m.regs[rs] < m.regs[rt]) ? 1 : 0;
                    break;

                case 0x00: { // sll
                    if (rd != 0) {
                        uint32_t value = static_cast<uint32_t>(m.regs[rt]);
                        m.regs[rd] = static_cast<int32_t>(value << shamt);
                    }
                    break;
                }

                case 0x08: // jr
                    next_pc = static_cast<uint32_t>(m.regs[rs]);
                    break;

                default:
                    break;
            }
            break;
        }

        case 0x08: // addi
            if (rt != 0) m.regs[rt] = m.regs[rs] + sign_extend_16(imm16);
            break;

        case 0x09: // addiu
            if (rt != 0) m.regs[rt] = m.regs[rs] + sign_extend_16(imm16);
            break;

        case 0x0D: { // ori
            if (rt != 0) {
                uint32_t left = static_cast<uint32_t>(m.regs[rs]);
                uint32_t right = static_cast<uint32_t>(imm16);
                m.regs[rt] = static_cast<int32_t>(left | right);
            }
            break;
        }

        case 0x0F: // lui
            if (rt != 0) {
                uint32_t value = static_cast<uint32_t>(imm16) << 16;
                m.regs[rt] = static_cast<int32_t>(value);
            }
            break;

        case 0x23: { // lw
            uint32_t addr = static_cast<uint32_t>(m.regs[rs] + sign_extend_16(imm16));
            int32_t value = 0;

            if (!mem.load_word(addr, value)) {
                return false;
            }

            if (rt != 0) m.regs[rt] = value;
            break;
        }

        case 0x2B: { // sw
            uint32_t addr = static_cast<uint32_t>(m.regs[rs] + sign_extend_16(imm16));

            if (!mem.store_word(addr, m.regs[rt])) {
                return false;
            }
            break;
        }

        case 0x04: // beq
            if (m.regs[rs] == m.regs[rt]) {
                next_pc = pc + 4 + static_cast<uint32_t>(sign_extend_16(imm16) << 2);
            }
            break;

        case 0x05: // bne
            if (m.regs[rs] != m.regs[rt]) {
                next_pc = pc + 4 + static_cast<uint32_t>(sign_extend_16(imm16) << 2);
            }
            break;

        case 0x02: // j
            next_pc = ((pc + 4) & 0xF0000000u) | (targ26 << 2);
            break;

        case 0x03: // jal
            m.regs[31] = static_cast<int32_t>(pc + 4);
            next_pc = ((pc + 4) & 0xF0000000u) | (targ26 << 2);
            break;

        case 0x1C: { // SPECIAL2, including mul
            if (funct == 0x02 && rd != 0) {
                int64_t left = static_cast<int64_t>(m.regs[rs]);
                int64_t right = static_cast<int64_t>(m.regs[rt]);
                int64_t product = left * right;
                m.regs[rd] = static_cast<int32_t>(product);
            }
            break;
        }

        default:
            break;
    }

    m.pc = next_pc;
    m.regs[0] = 0;
    return true;
}

// PROMPT: Run the full MIPS simulator.
// Each loop is one cycle. The simulator fetches, executes if ready, handles cache stalls,
// advances pending cache fills, and prints the CPU/cache/memory state.
static void run_simulator(std::ostream &out, const ProgramImage &img) {
    Machine m;
    m.pc = img.initial_pc;
    for (int &r : m.regs) r = 0;
    m.regs[29] = static_cast<int32_t>(img.initial_sp);

    MemorySystem mem(img);
    uint32_t cycle = 1;
    bool halt = false;
    bool have_current = false;
    LatchedInstr current{};

    while (true) {
        uint32_t fetched_word = 0;
        bool fetch_ready = mem.fetch_instruction(m.pc, fetched_word);

        LatchedInstr shown{};
        if (!have_current) {
            if (fetch_ready) {
                shown.valid = true;
                shown.pc = m.pc;
                shown.instr = fetched_word;
                shown.text = decode_instr(fetched_word, m.pc);
                current = shown;
                have_current = true;
            } else {
                shown.valid = false;
                shown.pc = m.pc;
                shown.instr = 0;
                shown.text = "nop";
            }
        } else {
            shown = current;
        }

        out << "Cycle " << cycle << ":\n";
        if (shown.valid) {
            out << "Fetched: " << shown.pc << ":\t" << bin32(shown.instr) << "\t" << shown.text << "\n";
        } else {
            out << "Fetched: " << shown.pc << ":\t\t" << shown.text << "\n";
        }
        out << "Registers:\n";

        bool completed = false;
        if (have_current) {
            completed = execute_instruction(m, mem, current, halt);
            if (completed) have_current = false;
        }
        mem.end_cycle();
        print_registers(out, m);
        print_l1_cache(out, "L1 Instruction Cache State:", mem.l1i);
        print_l1_cache(out, "L1 Data Cache State:", mem.l1d);
        print_l2_cache(out, mem);
        print_data_section(out, img, mem);
        print_stack_section(out, static_cast<uint32_t>(m.regs[29]), img.initial_sp, mem);

        if (halt && completed) break;
        ++cycle;
    }
}


// Check the command-line arguments, load the binary file, create the output file,
// print the decoder section, then run the simulator.
int main(int argc, char **argv) {
    if (argc != 3) {
        cerr << "Usage: mipssim <input binary> <output file>\n";
        return 1;
    }

    const std::string input_path = argv[1];
    const std::string output_path = argv[2];

    try {
        ProgramImage img = load_program_be(input_path);
        std::ofstream out(output_path);
        if (!out) throw std::runtime_error("Could not open output file: " + output_path);
        write_decoder_output(out, img);
        out << "\n";
        run_simulator(out, img);
        return 0;
    } catch (const std::exception &e) {
        cerr << "Error: " << e.what() << "\n";
        return 2;
    }
}
