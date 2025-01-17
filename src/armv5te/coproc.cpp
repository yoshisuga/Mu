#include "cpu.h"
#include "cpudefs.h"
#include "mmu.h"

extern "C" {
#include "../pxa255/pxa255.h"
#include "../pxa255/pxa255_PwrClk.h"
}


void do_cp15_mrc(uint32_t insn)
{
    uint32_t value;
    switch (insn & 0xEF00EF) {
        case 0x000000: /* MRC p15, 0, <Rd>, c0, c0, 0: ID Code Register */
            //value = 0x41069264; /* ARM926EJ-S revision 4 */
            //value = 0x69052100;//Intel PXA255 "01101001000001010010000100000000"
            value = 0x69052D05;//Intel PXA261 "01101001000001010010110100000101"
            break;
        case 0x000010: /* MRC p15, 0, <Rd>, c0, c0, 1: Cache Type Register */
            value = 0x1D112152; /* ICache: 16KB 4-way 8 word, DCache: 8KB 4-way 8 word */
            break;
        case 0x000020: /* MRC p15, 0, <Rd>, c0, c0, 2: TCM Status Register */
            value = 0;
            break;
        case 0x010000: /* MRC p15, 0, <Rd>, c1, c0, 0: Control Register */
            value = arm.control;
            break;
        case 0x020000: /* MRC p15, 0, <Rd>, c2, c0, 0: Translation Table Base Register */
            value = arm.translation_table_base;
            break;
        case 0x030000: /* MRC p15, 0, <Rd>, c3, c0, 0: Domain Access Control Register */
            value = arm.domain_access_control;
            break;
        case 0x050000: /* MRC p15, 0, <Rd>, c5, c0, 0: Data Fault Status Register */
            value = arm.data_fault_status;
            break;
        case 0x050020: /* MRC p15, 0, <Rd>, c5, c0, 1: Instruction Fault Status Register */
            value = arm.instruction_fault_status;
            break;
        case 0x060000: /* MRC p15, 0, <Rd>, c6, c0, 0: Fault Address Register */
            value = arm.fault_address;
            break;
        case 0x07006A: /* MRC p15, 0, <Rd>, c7, c10, 3: Test and clean DCache */
            value = 1 << 30;
            break;
        case 0x07006E: /* MRC p15, 0, <Rd>, c7, c14, 3: Test, clean, and invalidate DCache */
            value = 1 << 30;
            break;
        case 0x0D0000: /* MRC p15, 0, <Rd>, c13, c0, 0: Read FCSE PID */
            value = 0;
            break;
        case 0x0F0000: /* MRC p15, 0, <Rd>, c15, c0, 0: Debug Override Register */
            // Unimplemented
            value = 0;
            break;
        case 0x0F0001: /* MRC p15, 0, <Rd>, c15, c1, 0: Unknown */
            //TODO: Unknown(implmentation defined cp15 register)
            value = 0;
            break;
        default:
            warn("Unknown coprocessor instruction MRC %08X", insn);
            value = 0;
            break;
    }
    if ((insn >> 12 & 15) == 15) {
        arm.cpsr_n = value >> 31 & 1;
        arm.cpsr_z = value >> 30 & 1;
        arm.cpsr_c = value >> 29 & 1;
        arm.cpsr_v = value >> 28 & 1;
    } else
        arm.reg[insn >> 12 & 15] = value;
}

void do_cp15_mcr(uint32_t insn)
{
    uint32_t value = reg(insn >> 12 & 15);
    switch (insn & 0xEF00EF) {
        case 0x010000: { /* MCR p15, 0, <Rd>, c1, c0, 0: Control Register */
            uint32_t change = value ^ arm.control;
            //TODO: actually implement this register fully
            /*
            if ((value & 0xFFFF8CF0) != 0x00050070)
                error("Bad or unimplemented control register value: %x (unsupported: %x)\n", value, (value & 0xFFFF8CF8) ^ 0x00050078);
            */
            arm.control = value;
            if (change & 1) // MMU is being turned on or off
                addr_cache_flush();
            break;
        }
        case 0x020000: /* MCR p15, 0, <Rd>, c2, c0, 0: Translation Table Base Register */
            arm.translation_table_base = value & ~0x3FFF;
            addr_cache_flush();
            break;
        case 0x030000: /* MCR p15, 0, <Rd>, c3, c0, 0: Domain Access Control Register */
            arm.domain_access_control = value;
            addr_cache_flush();
            break;
        case 0x050000: /* MCR p15, 0, <Rd>, c5, c0, 0: Data Fault Status Register */
            arm.data_fault_status = value;
            break;
        case 0x050020: /* MCR p15, 0, <Rd>, c5, c0, 1: Instruction Fault Status Register */
            arm.instruction_fault_status = value;
            break;
        case 0x060000: /* MCR p15, 0, <Rd>, c6, c0, 0: Fault Address Register */
            arm.fault_address = value;
            break;
        case 0x070080: /* MCR p15, 0, <Rd>, c7, c0, 4: Wait for interrupt */
            cycle_count_delta = 0;
            if (arm.interrupts == 0) {
                arm.reg[15] -= 4;
                cpu_events |= EVENT_WAITING;
            }
            break;
        case 0x080005: /* MCR p15, 0, <Rd>, c8, c5, 0: Invalidate instruction TLB */
        case 0x080007: /* MCR p15, 0, <Rd>, c8, c7, 0: Invalidate TLB */
        case 0x080025: /* MCR p15, 0, <Rd>, c8, c5, 1: Invalidate instruction TLB entry */
        case 0x080027: /* MCR p15, 0, <Rd>, c8, c7, 1: Invalidate TLB (used by polydumper) */
        case 0x070005: /* MCR p15, 0, <Rd>, c7, c5, 0: Invalidate ICache */
        case 0x070025: /* MCR p15, 0, <Rd>, c7, c5, 1: Invalidate ICache line */
        case 0x070007: /* MCR p15, 0, <Rd>, c7, c7, 0: Invalidate ICache and DCache */
            addr_cache_flush();
            break;

        case 0x080006: /* MCR p15, 0, <Rd>, c8, c6, 0: Invalidate data TLB */
        case 0x080026: /* MCR p15, 0, <Rd>, c8, c6, 1: Invalidate data TLB entry */
        case 0x070026: /* MCR p15, 0, <Rd>, c7, c6, 1: Invalidate single DCache entry */
        case 0x07002A: /* MCR p15, 0, <Rd>, c7, c10, 1: Clean DCache line */
        case 0x07002E: /* MCR p15, 0, <Rd>, c7, c14, 1: Clean and invalidate single DCache entry */
        case 0x07008A: /* MCR p15, 0, <Rd>, c7, c10, 4: Drain write buffer */
        case 0x0F0000: /* MCR p15, 0, <Rd>, c15, c0, 0: Debug Override Register */
            #ifdef SUPPORT_LINUX
                // Normally ignored, but somehow needed for linux to boot correctly
                addr_cache_flush();
            #endif
            break;
        case 0x0F0001: /* MCR p15, 0, <Rd>, c15, c1, 0: Unknown */
            //TODO: Unknown(implmentation defined cp15 register)
            break;
        default:
            warn("Unknown coprocessor instruction MCR %08X", insn);
            break;
    }
}

void do_cp15_instruction(Instruction i)
{
    uint32_t insn = i.raw;
    if(insn & 0x00100000)
        return do_cp15_mrc(insn);
    else
        return do_cp15_mcr(insn);
}

void do_cp14_instruction(Instruction i)
{
    uint32_t instr = i.raw;
    bool specialInstr = i.cond == 0xF;
    bool success;

    success = pxa255pwrClkPrvCoprocRegXferFunc(&pxa255PwrClk, specialInstr, (instr & 0x00100000) != 0, (instr >> 21) & 0x07, (instr >> 12) & 0x0F, (instr >> 16) & 0x0F, instr & 0x0F, (instr >> 5) & 0x07);

    //fail if instr dosent actully exist
    if(!success)
       undefined_instruction();
}

