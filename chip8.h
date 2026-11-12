#include <stdint.h>

typedef struct {
    // 1. 内存：4KB 内存，用来存放程序指令和数据 (对应真实的 SRAM)
    uint8_t memory[4096];     
    
    // 2. 通用寄存器：16个 8位寄存器，名字叫 V0 到 VF (对应 CPU 里的 Register File)
    uint8_t V[16];            
    
    // 3. 索引寄存器：16位，通常用来存内存地址 (类似 RISC-V 里的指针)
    uint16_t index;               
    
    // 4. 程序计数器：16位，永远指向下一条要执行的指令地址
    uint16_t pc;              
    
    // 5. 栈和栈指针：用于函数调用（CHIP-8规定最多16层嵌套）
    uint16_t stack[16];       
    uint8_t sp;               
    
    // 6. 当前指令：16位，存放从内存里取出来的那条指令 (取指阶段)
    uint16_t opcode;          
    
    // 7. 延迟定时器和声音定时器 (相当于硬件里的倒计时器)
    uint8_t delay_timer;
    uint8_t sound_timer;
    
    // 8. 屏幕显示状态：64x32 像素，每个像素是黑白 (类似显存)
    uint8_t video[64 * 32];
    
    // 9. 键盘状态：16个按键
    uint8_t keypad[16];
} Chip8;
