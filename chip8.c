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
    memcpy(chip->memory,fontset,80);
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

int main() {
    Chip8 my_chip;
    
    // 1. 初始化虚拟硬件
    chip8_init(&my_chip);
    printf("PC起点: 0x%X\n", my_chip.pc);
    printf("内存第0个字节(字体集第一位): 0x%X\n", my_chip.memory[0]);

    // 2. 尝试加载一个ROM（如果没有测试文件，这一步可以注释掉）
    // chip8_load_rom(&my_chip, "test_rom.ch8");
    
    return 0;
}