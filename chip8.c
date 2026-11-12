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
        case 0x8000: {
            // 提取寄存器编号 X 和 Y
            uint8_t X = (chip->opcode & 0x0F00) >> 8;
            uint8_t Y = (chip->opcode & 0x00F0) >> 4;
            uint8_t N = chip->opcode & 0x000F; // 最低 4 位决定具体操作
         
            switch (N) {
                case 0x4: { // 8XY4: VX = VX + VY, 产生进位时 VF = 1
                    // 最佳实践：用 16 位无符号整数暂存，防止 8 位溢出丢数据
                    uint16_t sum = chip->V[X] + chip->V[Y]; 
                    
                    // 如果超过 255，说明产生了第 9 位进位
                    if (sum > 255) {
                        chip->V[0xF] = 1;
                    } else {
                        chip->V[0xF] = 0;
                    }
                    
                    // 只保留低 8 位存回 VX
                    chip->V[X] = sum & 0xFF; 
                    printf("  -> Executed 8XY4: V[%X] = V[%X] + V[%X] (Carry: %d)\n", X, X, Y, chip->V[0xF]);
                    break;
                    }
                case 0x5: { // 8XY5: VX = VX - VY, VX > VY 时 VF = 1 (无借位)
                    // 硬件规则：先判断大小设置标志位，再做减法
                    if (chip->V[X] >= chip->V[Y]) {
                        chip->V[0xF] = 1; // 没有借位
                    } else {
                        chip->V[0xF] = 0; // 发生借位
                    }
                    
                    chip->V[X] = chip->V[X] - chip->V[Y];
                    printf("  -> Executed 8XY5: V[%X] = V[%X] - V[%X] (No Borrow: %d)\n", X, X, Y, chip->V[0xF]);
                    break;
                }
                case 0x0: { // 8XY0: VX = VY
                    chip->V[X] = chip->V[Y];
                    printf("  -> Executed 8XY0: V[%X] = V[%X]\n", X, Y);
                    break;
                }
                case 0x1: { // 8XY1: VX = VX | VY
                    chip->V[X] |= chip->V[Y];
                    chip->V[0xF] = 0; 
                    printf("  -> Executed 8XY1: V[%X] |= V[%X]\n", X, Y);
                    break;
                }
                                case 0x2: { // 8XY2: VX = VX & VY (按位与)
                    chip->V[X] &= chip->V[Y];
                    chip->V[0xF] = 0; // 逻辑运算后 VF 必须清零
                    printf("  -> Executed 8XY2: V[%X] &= V[%X]\n", X, Y);
                    break;
                }
                case 0x3: { // 8XY3: VX = VX ^ VY (按位异或)
                    chip->V[X] ^= chip->V[Y];
                    chip->V[0xF] = 0; // 逻辑运算后 VF 必须清零
                    printf("  -> Executed 8XY3: V[%X] ^= V[%X]\n", X, Y);
                    break;
                }
                case 0x6: { // 8XY6: VX = VY >> 1 (右移一位)
                    // 提取 VY 的最低位 (LSB) 存入 VF
                    chip->V[0xF] = chip->V[Y] & 0x1; 
                    chip->V[X] = chip->V[Y] >> 1;
                    printf("  -> Executed 8XY6: V[%X] = V[%X] >> 1 (LSB: %d)\n", X, Y, chip->V[0xF]);
                    break;
                }
                case 0xE: { // 8XYE: VX = VY << 1 (左移一位)
                    // 提取 VY 的最高位 (MSB) 存入 VF
                    chip->V[0xF] = (chip->V[Y] & 0x80) >> 7;
                    chip->V[X] = chip->V[Y] << 1;
                    printf("  -> Executed 8XYE: V[%X] = V[%X] << 1 (MSB: %d)\n", X, Y, chip->V[0xF]);
                    break;
                }
          
                default:
                    printf("  -> Unhandled 8XYN opcode: 0x%X\n", chip->opcode);
                    break;
            }
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
    
    // 1. 测试 AND 逻辑运算 (8XY2)
    // 设 V1 = 0x0F (00001111), V2 = 0x33 (00110011)
    my_chip.V[1] = 0x0F;
    my_chip.V[2] = 0x33;
    // 指令 0x8122 (V1 = V1 & V2)
    my_chip.memory[0x200] = 0x81;
    my_chip.memory[0x201] = 0x22;
    chip8_cycle(&my_chip);
    printf(">> Test AND: V1 = 0x%X (Expected: 0x03)\n\n", my_chip.V[1]);

    // 2. 测试右移进位 (8XY6)
    // 设 V3 = 0x05 (00000101)
    my_chip.V[3] = 0x05;
    // 指令 0x8136 (V3 = V3 >> 1)
    my_chip.memory[0x202] = 0x83;
    my_chip.memory[0x203] = 0x36;
    chip8_cycle(&my_chip);
    printf(">> Test Shift Right: V3 = 0x%X, VF = %d (Expected: V3=0x02, VF=1)\n\n", my_chip.V[3], my_chip.V[0xF]);

    return 0;
}