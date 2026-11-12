#include "chip8.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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

void chip8_init(Chip8 *chip)
{
    // 1. 硬件级 Reset：把整个结构体所占用的内存全部填为 0
    // 对应硬件里的复位信号（Reset），所有寄存器清零
    memset(chip, 0, sizeof(Chip8));

    // 2. 设定程序的起跑线
    // 物理上，CHIP-8 的程序总是从内存的 0x200（512）地址开始加载
    chip->pc = 0x200;

    // 3. 把栈指针清空（虽然 memset 已经把 sp 变成 0 了，但明确写出来体现架构清晰）
    chip->sp = 0;

    // 4. 加载字体集（Fontset）到内存的 0x000 到 0x04F 处
    memcpy(chip->memory, fontset, sizeof(fontset));
}

#define START_ADDRESS 0x200 // 宏定义 ROM 的起始地址

void chip8_load_rom(Chip8 *chip, const char *filename)
{
    // 1. 以二进制读取模式 ("rb") 打开文件
    FILE *file = fopen(filename, "rb");
    if (file == NULL)
    {
        printf("Error: Cannot open ROM file %s\n", filename);
        return;
    }

    // 2. 计算文件大小 (把文件指针移到末尾，用 ftell 算大小，再移回开头)
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // 3. 警告：CHIP-8 的 4KB 内存，前 512 字节被系统和字体集占了
    // 如果 ROM 大于 (4096 - 512) = 3584 字节，就会发生内存溢出！
    if (size > (4096 - START_ADDRESS))
    {
        printf("Error: ROM file is too large to fit in memory!\n");
        fclose(file);
        return;
    }

    // 4. 分配一段缓冲区来装文件数据
    uint8_t *buffer = (uint8_t *)malloc(size);
    if (buffer == NULL)
    {
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

void chip8_cycle(Chip8 *chip)
{
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
    switch (chip->opcode & 0xF000)
    {
    case 0x0000:
        // 处理 0 开头的指令
        if (chip->opcode == 0x00E0)
        {
            memset(chip->video, 0, sizeof(chip->video));
            printf("  -> Executed 00E0: Screen cleared!\n");
        }
        break;

    case 0x6000:
    { // 6XNN: 将 NN 赋值给寄存器 VX
        uint8_t X = (chip->opcode & 0x0F00) >> 8;
        uint8_t NN = (chip->opcode & 0x00FF);
        chip->V[X] = NN;
        printf("  -> Executed 6XNN: V[%X] = %d\n", X, NN);
        break;
    }

    case 0x7000:
    { // 7XNN: 将 NN 加到寄存器 VX
        uint8_t X = (chip->opcode & 0x0F00) >> 8;
        uint8_t NN = (chip->opcode & 0x00FF);
        chip->V[X] += NN;
        printf("  -> Executed 7XNN: V[%X] += %d\n", X, NN);
        break;
    }
    case 0x8000:
    {
        // 提取寄存器编号 X 和 Y
        uint8_t X = (chip->opcode & 0x0F00) >> 8;
        uint8_t Y = (chip->opcode & 0x00F0) >> 4;
        uint8_t N = chip->opcode & 0x000F; // 最低 4 位决定具体操作

        switch (N)
        {
        case 0x4:
        { // 8XY4: VX = VX + VY, 产生进位时 VF = 1
            // 最佳实践：用 16 位无符号整数暂存，防止 8 位溢出丢数据
            uint16_t sum = chip->V[X] + chip->V[Y];

            // 如果超过 255，说明产生了第 9 位进位
            if (sum > 255)
            {
                chip->V[0xF] = 1;
            }
            else
            {
                chip->V[0xF] = 0;
            }

            // 只保留低 8 位存回 VX
            chip->V[X] = sum & 0xFF;
            printf("  -> Executed 8XY4: V[%X] = V[%X] + V[%X] (Carry: %d)\n", X, X, Y, chip->V[0xF]);
            break;
        }
        case 0x5:
        { // 8XY5: VX = VX - VY, VX > VY 时 VF = 1 (无借位)
            // 硬件规则：先判断大小设置标志位，再做减法
            if (chip->V[X] >= chip->V[Y])
            {
                chip->V[0xF] = 1; // 没有借位
            }
            else
            {
                chip->V[0xF] = 0; // 发生借位
            }

            chip->V[X] = chip->V[X] - chip->V[Y];
            printf("  -> Executed 8XY5: V[%X] = V[%X] - V[%X] (No Borrow: %d)\n", X, X, Y, chip->V[0xF]);
            break;
        }
        case 0x0:
        { // 8XY0: VX = VY
            chip->V[X] = chip->V[Y];
            printf("  -> Executed 8XY0: V[%X] = V[%X]\n", X, Y);
            break;
        }
        case 0x1:
        { // 8XY1: VX = VX | VY
            chip->V[X] |= chip->V[Y];
            chip->V[0xF] = 0;
            printf("  -> Executed 8XY1: V[%X] |= V[%X]\n", X, Y);
            break;
        }
        case 0x2:
        { // 8XY2: VX = VX & VY (按位与)
            chip->V[X] &= chip->V[Y];
            chip->V[0xF] = 0; // 逻辑运算后 VF 必须清零
            printf("  -> Executed 8XY2: V[%X] &= V[%X]\n", X, Y);
            break;
        }
        case 0x3:
        { // 8XY3: VX = VX ^ VY (按位异或)
            chip->V[X] ^= chip->V[Y];
            chip->V[0xF] = 0; // 逻辑运算后 VF 必须清零
            printf("  -> Executed 8XY3: V[%X] ^= V[%X]\n", X, Y);
            break;
        }
        case 0x6:
        { // 8XY6: VX = VY >> 1 (右移一位)
            // 提取 VY 的最低位 (LSB) 存入 VF
            chip->V[0xF] = chip->V[Y] & 0x1;
            chip->V[X] = chip->V[Y] >> 1;
            printf("  -> Executed 8XY6: V[%X] = V[%X] >> 1 (LSB: %d)\n", X, Y, chip->V[0xF]);
            break;
        }
        case 0xE:
        { // 8XYE: VX = VY << 1 (左移一位)
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
    case 0x1000:
    { // 1NNN: 无条件跳转 (Jump)
        // 提取 NNN (低 12 位地址)
        uint16_t NNN = chip->opcode & 0x0FFF;
        // 架构师心法：直接覆盖 PC 的值，硬件里的 MUX 会立刻改变取指地址
        chip->pc = NNN;
        printf("  -> Executed 1NNN: Jumped to 0x%X\n", NNN);
        break;
    }

    case 0x2000:
    { // 2NNN: 调用子程序 (Call)
        uint16_t NNN = chip->opcode & 0x0FFF;
        // 1. 将当前的 PC (即下一条指令的地址) 压入栈中
        // 架构师防坑：必须检查栈是否溢出 (CHIP-8 栈只能存 16 层)
        if (chip->sp < 16)
        {
            chip->stack[chip->sp] = chip->pc;
            chip->sp++;
        }
        else
        {
            printf("  -> Error: Stack Overflow!\n");
        }
        // 2. 跳转到目标地址
        chip->pc = NNN;
        printf("  -> Executed 2NNN: Called subroutine at 0x%X, SP: %d\n", NNN, chip->sp);
        break;
    }

    case 0x3000:
    { // 3XNN: 条件跳转 (如果 VX == NN，则跳过下一条指令)
        uint8_t X = (chip->opcode & 0x0F00) >> 8;
        uint8_t NN = (chip->opcode & 0x00FF);

        if (chip->V[X] == NN)
        {
            // 架构师心法：因为取指阶段 PC 已经 +2 了，这里再 +2 就能实现“跳过”
            chip->pc += 2;
            printf("  -> Executed 3XNN: V[%X] == %d, Skipped next instruction\n", X, NN);
        }
        else
        {
            printf("  -> Executed 3XNN: V[%X] != %d, No skip\n", X, NN);
        }
        break;
    }
    case 0xD000:
    { // DXYN: 绘制精灵图
        uint8_t X = (chip->opcode & 0x0F00) >> 8;
        uint8_t Y = (chip->opcode & 0x00F0) >> 4;
        uint8_t N = chip->opcode & 0x000F;

        // 获取屏幕上的起始坐标，并做 64x32 的边界环绕（取模）
        uint8_t x_pos = chip->V[X] % 64;
        uint8_t y_pos = chip->V[Y] % 32;

        // 碰撞标志初始化为 0
        chip->V[0xF] = 0;
        // 外层循环：一行一行地画 (共 N 行)
        for (int row = 0; row < N; row++)
        {
            // 从内存读取当前行要画的 8 个像素数据
            uint8_t sprite_byte = chip->memory[chip->index + row];

            // 内层循环：一个 bit 一个 bit 地画 (每个字节 8 bit)
            for (int col = 0; col < 8; col++)
            {
                // 使用位掩码 0x80 >> col 来检查当前 bit 是否为 1
                if ((sprite_byte & (0x80 >> col)) != 0)
                {
                    // 计算屏幕上的绝对像素坐标 (带环绕逻辑)
                    int px = (x_pos + col) % 64;
                    int py = (y_pos + row) % 32;

                    // 计算在一维 video 数组里的索引
                    int index = py * 64 + px;

                    // 碰撞检测：如果当前位置已经有像素了 (值为 1)
                    if (chip->video[index] == 1)
                    {
                        chip->V[0xF] = 1; // 发生碰撞！
                    }

                    // XOR 操作：1 变 0，0 变 1
                    chip->video[index] ^= 1;
                }
            }
        }
        printf("  -> Executed DXYN: Drew sprite at (%d, %d), Collision: %d\n", x_pos, y_pos, chip->V[0xF]);
        break;
    }
    break;
    }

    default:
        printf("  -> Unknown opcode: 0x%X\n", chip->opcode);
        break;
    }
}

void draw_screen(Chip8 *chip)
{
    printf("\n--- Virtual Screen ---\n");
    for (int y = 0; y < 32; y++)
    {
        // 👇 内层循环：遍历 64 个像素列
        for (int x = 0; x < 64; x++)
        {
            if (chip->video[y * 64 + x] == 1)
            {
                printf("█"); // 像素亮起
            }
            else
            {
                printf(" "); // 像素熄灭（空格）
            }
        }
        printf("\n"); // 每画完 64 个像素，换到下一行
    }
    printf("-----------------------\n");
}

int main()
{
    Chip8 my_chip;
    chip8_init(&my_chip);

    // 设置坐标 (V1 = 10, V2 = 5)
    my_chip.V[1] = 10;
    my_chip.V[2] = 5;

    // 设置索引寄存器 I，指向内存中的 'F' 字符 (内存第 75 字节处)
    my_chip.index = 75;

    // 指令 D125 (DXYN, 表示在 V1, V2 坐标处画 5 行高的精灵图)
    my_chip.memory[0x200] = 0xD1;
    my_chip.memory[0x201] = 0x25;

    // 踩油门，让 CPU 执行这条 DXYN 指令！
    chip8_cycle(&my_chip);

    // 打印虚拟屏幕
    draw_screen(&my_chip);

    return 0;
}