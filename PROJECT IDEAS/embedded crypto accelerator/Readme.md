# **Embedded system Cryptography Accelerator using PQC**  
**Lattice-Based PQC (Kyber KEM) with NTT Hardware Acceleration**  

## **Project Overview**  
This project implements a **RISC-V-based embedded accelerator** for post-quantum cryptography (PQC), focusing on **Kyber KEM** (Key Encapsulation Mechanism) with hardware-accelerated **Number Theoretic Transform (NTT)**. The design leverages:  
- **RISC-V ISA extensions**: `Zvk` (Vector Cryptography) + `Zbkb` (Bit Manipulation) for optimized polynomial arithmetic.  
- **Dedicated NTT co-processor**: For fast polynomial multiplication in lattice-based PQC.  
- **Secure key generation**: True Random Number Generator (TRNG) for quantum-resistant key pairs.  
- **Software stack**: Python/C library to offload PQC operations to the hardware accelerator.  

---

## **Hardware Architecture**  
### **1. RISC-V Core Customization**  
- **Base ISA**: RV32IMC (32-bit, Multiply, Compressed).  
- **Extensions**:  
  - `Zvk`: Vectorized NTT operations (e.g., `vntt` instruction).  
  - `Zbkb`: Bit manipulation for Kyber’s CBD (Centered Binomial Distribution).  
  - `Zkn`: SHA-3 acceleration (for Kyber’s symmetric primitives).  
- **Secure enclave**: Isolated memory for private keys (PMP-protected).  

### **2. NTT Accelerator**  
- **Polynomial multiplier**: Radix-2 NTT in hardware (fully pipelined).  
- **Memory**: Dual-port RAM for coefficient storage (512-bit wide for vectorized access).  
- **Control unit**: Custom instructions to trigger NTT (`ntt.start`, `ntt.readresult`).  

### **3. TRNG & Side-Channel Protection**  
- **Entropy source**: Ring oscillator + ADC noise sampling.  
- **Countermeasures**: Constant-time NTT, masked arithmetic.  

---

## **Software Stack**  
### **1. Firmware (On-Device)**  
- **Kyber KEM** in C:  
  - Optimized for RISC-V (`Zvk`/`Zbkb`).  
  - Uses hardware NTT via MMIO.  
- **Secure boot**: Signed with Dilithium (PQC signature).  

### **2. Host Library (Python/C)**  
```python
# Example: Offload Kyber to accelerator  
from kyber_accelerator import Kyber  
k = Kyber(security_level=3)  # Kyber-768  
pk, sk = k.keygen()          # Uses hardware TRNG  
ciphertext, shared_key = k.encaps(pk)  
decrypted_key = k.decaps(sk, ciphertext)  
```

### **3. Benchmarks & Testing**  
- **Performance**: Cycles per NTT operation vs. software-only.  
- **Power**: Measured with Joulescope on FPGA.  
- **Side-channel tests**: ChipWhisperer for DPA/EMA.  

---

## **Repository Structure**  
```  
├── hardware/  
│   ├── rtl/                  # Verilog sources (RISC-V core, NTT accelerator)  
│   ├── fpga/                 # Xilinx/Vivado project  
│   └── asic/                 # OpenLANE configs  
├── software/  
│   ├── firmware/             # Kyber RV32IMC assembly + C  
│   ├── host_lib/             # Python/C API for accelerator  
│   └── tests/                # KAT (Known Answer Tests)  
├── docs/  
│   ├── schematics/            # Block diagrams  
│   └── riscv-extensions.md   # Custom ISA docs  
└── tools/                    # Scripts for TRNG calibration, etc.  
```

---

## **Key Features**  
--> **Full Kyber-512/768/1024 support** (NIST PQC Standard)  
--> **4x faster NTT** vs. software (measured on Artix-7 FPGA)  
--> **Side-channel resistant design** (constant-time, masked NTT)  
--> **End-to-end demo**: Key exchange between host MCU and accelerator  

---

## **Future Work**  
- Add **Dilithium** (PQC signatures) with same NTT accelerator.  
- **RISC-V SoC**: Integrate with OpenTitan for secure root-of-trust.  
- **Formal verification**: Prove correctness of NTT hardware.  

---

## **Getting Started**  
1. **Build RISC-V core**:  
   ```bash  
   cd hardware/rtl && make  
   ```  
2. **Run Kyber test**:  
   ```bash  
   cd software/tests && pytest kyber_kat.py  
   ```  
3. **FPGA synthesis**:  
   ```bash  
   cd hardware/fpga && vivado -mode batch -source build.tcl  
   ```  

---
 
