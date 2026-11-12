#include"chip8.h"
#include<stdlib.h>
#include<string.h>
#include<stdio.h>

// CHIP-8 标准内置字体集 (80 字节)
static const uint8_t fontset[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};


void chip8_init(Chip8 *chip) {
    // 1. 硬件级 Reset：把整个结构体所占用的内存全部填为 0
    // 对应硬件里的复位信号（Reset），所有寄存器清零
    memset(chip, 0, sizeof(Chip8));
    
    // 2. 设定程序的起跑线
    // 物理上，CHIP-8 的程序总是从内存的 0x200（512）地址开始加载
    chip->pc = 0x200;
    
    // 3. 把栈指针清空（虽然 memset 已经把 sp 变成 0 了，但明确写出来体现架构清晰）
    chip->sp = 0;
    
    // 4. 加载字体集（Fontset）到内存的 0x000 到 0x04F 处
    memcpy(chip->memory,fontset,sizeof(fontset));
}

#define START_ADDRESS 0x200 // 宏定义 ROM 的起始地址

void chip8_load_rom(Chip8 *chip, const char *filename) {
    // 1. 以二进制读取模式 ("rb") 打开文件
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        printf("Error: Cannot open ROM file %s\n", filename);
        return;
    }

    // 2. 计算文件大小 (把文件指针移到末尾，用 ftell 算大小，再移回开头)
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // 3. 警告：CHIP-8 的 4KB 内存，前 512 字节被系统和字体集占了
    // 如果 ROM 大于 (4096 - 512) = 3584 字节，就会发生内存溢出！
    if (size > (4096 - START_ADDRESS)) {
        printf("Error: ROM file is too large to fit in memory!\n");
        fclose(file);
        return;
    }

    // 4. 分配一段缓冲区来装文件数据
    uint8_t *buffer = (uint8_t *)malloc(size);
    if (buffer == NULL) {
        printf("Error: Failed to allocate memory.\n");
        fclose(file);
        return;
    }

    // 5. 把文件内容读进缓冲区
    fread(buffer, 1, size, file);
    fclose(file);

    // 6. 核心动作：把缓冲区的数据拷贝到虚拟机内存的 0x200 处
    // 好比把 ROM 里的指令烧录到芯片的闪存里
    memcpy(&chip->memory[START_ADDRESS], buffer, size);

    // 7. 释放 malloc 申请的内存，防止内存泄漏(对应 C++ 的 delete[])
    free(buffer);
    
    printf("Successfully loaded ROM: %s (%ld bytes)\n", filename, size);
}

void chip8_cycle(Chip8 *chip) {
    // 1. 取指 (Fetch)
    // CHIP-8 指令是 16 位的（2个字节）。
    // 从 PC 指向的内存读高 8 位，左移 8 位；再读低 8 位拼在一起。
    chip->opcode = (chip->memory[chip->pc] << 8) | chip->memory[chip->pc + 1];
    
    // 打印看看取到了什么指令（十六进制）
    printf("Fetched opcode: 0x%X at PC: 0x%X\n", chip->opcode, chip->pc);
    
    // PC 自增 2（因为执行完这条指令，PC 要指向下一条指令）
    chip->pc += 2;

    // 开始译码和执行
       // 开始译码和执行：用 0xF000 掩码只看高 4 位
    switch (chip->opcode & 0xF000) {
        case 0x0000:
            // 处理 0 开头的指令
            if (chip->opcode == 0x00E0) {
                memset(chip->video, 0, sizeof(chip->video));
                printf("  -> Executed 00E0: Screen cleared!\n");
            }
            break;

        case 0x1000:
            // 1NNN: 跳转到地址 NNN（你之前的假指令 0x1234 属于这个）
            printf("  -> Executed 1NNN: Jumped to 0x%X\n", chip->opcode & 0x0FFF);
            break;

        case 0x6000: { // 6XNN: 将 NN 赋值给寄存器 VX
            uint8_t X = (chip->opcode & 0x0F00) >> 8;
            uint8_t NN = (chip->opcode & 0x00FF);
            chip->V[X] = NN;
            printf("  -> Executed 6XNN: V[%X] = %d\n", X, NN);
            break;
        }

        case 0x7000: { // 7XNN: 将 NN 加到寄存器 VX
            uint8_t X = (chip->opcode & 0x0F00) >> 8;
            uint8_t NN = (chip->opcode & 0x00FF);
            chip->V[X] += NN;
            printf("  -> Executed 7XNN: V[%X] += %d\n", X, NN);
            break;
        }

        default:
            printf("  -> Unknown opcode: 0x%X\n", chip->opcode);
            break;
    }
}
int main() {
    Chip8 my_chip;
    chip8_init(&my_chip);
    
    // 指令 1：6A05 (把 5 赋值给寄存器 VA)
    my_chip.memory[0x200] = 0x6A; 
    my_chip.memory[0x201] = 0x05;
    
    // 指令 2：7A03 (给寄存器 VA 加上 3)
    my_chip.memory[0x202] = 0x7A;
    my_chip.memory[0x203] = 0x03;

    // 让虚拟CPU跑两个时钟周期
    chip8_cycle(&my_chip);
    chip8_cycle(&my_chip);
    
    // 打印结果，看看 VA 是不是变成了 8 (5 + 3)
    printf("Register VA: %d\n", my_chip.V[0xA]);

    return 0;
}