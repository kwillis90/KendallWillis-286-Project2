# MIPS Simulator Project README

## Group Members
- Kendall Willis
  Email: kendalw@siue.edu
- Models L1 and L2 cache with stalls and misses
- Outputs cycle-by-cycle state of the machine

---

## How to Run
Compile:
make

Run:
./mipssim <input_file> <output_file>

Example:
./mipssim t1.bin output.txt

---

## AI Tools Used
- ChatGPT used for explanations expermentation and understanding concepts 
- GitHub Copilot (code suggestions and minor fixes)
- Perplexity AI (concept lookup and clarification)

---

## Example Prompts Used
- "explain how MIPS instruction decoding works"
- "explain cache misses and stalls in simple terms"
- "why does my program fetch and execute in the same cycle"
- "explain pending fill logic in cache systems"
- "what causes a stall in lw/sw instructions"

---

## Notes
- The simulator runs one cycle at a time (no fetch and execute in same cycle)
- Cache misses introduce stalls using pending fill logic

---
    
## Summary
This project demonstrates:
- MIPS instruction decoding
- CPU cycle simulation
- Cache memory behavior (L1/L2)
- Debugging complex system logic