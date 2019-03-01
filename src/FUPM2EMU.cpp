#include"../include/FUPM2EMU.h"
#include<iostream>
#include<cstdio>
#include<cstring>

//#define DEBUG_COUT

namespace FUPM2EMU
{
    //////////////// STATE ////////////////
    State::State()
    {
        // Заполнение нулями регистров.
        std::memset(Registers, 0, RegistersNumber * sizeof(uint32_t));

        // Обнуление регистра флагов.
        Flags = 0;

        // Создание и заполнение нулями блока памяти.
        Memory = std::shared_ptr<uint32_t>(new uint32_t[MemorySize]);
        std::memset(Memory.get(), 0, MemorySize * sizeof(uint32_t));

        // Тестовая программа:
        Memory.get()[0] = 0x00064001; // SYSCALL R0 100.
        Memory.get()[1] = 0x00064101; // SYSCALL R1 100.
        Memory.get()[2] = 0x00081002; // ADD R0 R1 0x8.
        Memory.get()[3] = 0x00066001; // SYSCALL R0 102.
        Memory.get()[4] = 0x00000004; // SUB R0 R0 0x0.
        Memory.get()[5] = 0x0000A003; // ADDI R0 0xA.
        Memory.get()[6] = 0x00069001; // SYSCALL R0 105.
    }
    State::~State()
    {
        // ...
    }



    //////////////// EMULATOR ////////////////
    // PUBLIC:
    Emulator::Emulator()
    {
        // Надо что-то придумать касательно потенциальных частых "реаллоцирований" Memory в CurrentSate.

        // Инициализировать массив указателей на функции лямбда-выражениями... Я, наверное, совсем поехал... Хотя стандарт 17 позволяет.
        // Код возврата операции влияет на работу.
        InstructionsSet = std::vector<std::function<int (uint32_t, State&)>>
            ({
                [](uint32_t Command, State& CurrentState) // HALT - выключение процессора.
                {
                    #ifdef DEBUG_COUT
                    std::cout << "HALT" << std::endl;
                    #endif
                    
                    return(OP_TERMINATE);
                },
                [](uint32_t Command, State& CurrentState) // SYSCALL - системный вызов.
                {
                    int ReturnCode = OP_OK;

                    uint8_t  R    = (uint8_t)((Command >> 8) & 0xF);
                    uint32_t Code = (uint32_t)(Command >> 12);

                    #ifdef DEBUG_COUT
                    std::cout << "SYSCALL R" << (unsigned int)R << " " << Code << std::endl;
                    #endif

                    switch (Code)
                    {
                        // EXIT - выход.
                        case 0:
                        {
                            ReturnCode = OP_TERMINATE;
                            break;
                        }
                        // SCANINT - запрос целого числа.
                        case 100:
                        {
                            std::cin >> CurrentState.Registers[R];
                            break;
                        }
                        // PRINTINT - вывод целого числа.
                        case 102:
                        {
                            std::cout << CurrentState.Registers[R];
                            break;
                        }
                        // PUTCHAR - вывод символа.
                        case 105:
                        {
                            putchar((uint8_t)(CurrentState.Registers[R]));
                            break;
                        }
                        // Использован неспецифицированный код системного вызова.
                        default:
                        {
                            ReturnCode = OP_ERROR;
                            break;
                        }
                    }
                    return(ReturnCode);
                },
                [](uint32_t Command, State& CurrentState) // ADD - сложение регистров.
                {
                    uint8_t R1 =   (uint8_t)((Command >> 8)  & 0xF);
                    uint8_t R2 =   (uint8_t)((Command >> 12) & 0xF);
                    int32_t Imm = (uint32_t)((Command >> 16) & 0xFFFF);

                    #ifdef DEBUG_COUT
                    std::cout << "ADD R" << (unsigned int)R1 << " R" << (unsigned int)R2 << " " << Imm << std::endl;
                    #endif

                    CurrentState.Registers[R1] += CurrentState.Registers[R2] + Imm;
                    return(OP_OK);
                },
                [](uint32_t Command, State& CurrentState) // ADDI - прибавление к регистру непосредственного операнда.
                {
                    uint8_t R =    (uint8_t)((Command >> 8)  & 0xF);
                    int32_t Imm = (uint32_t)((Command >> 12) & 0xFFFFF);

                    #ifdef DEBUG_COUT
                    std::cout << "ADDI R" << (unsigned int)R << " " << Imm << std::endl;
                    #endif

                    CurrentState.Registers[R] += Imm;
                    return(OP_OK);
                },
                [](uint32_t Command, State& CurrentState) // SUB - разность регистров.
                {
                    uint8_t R1 =   (uint8_t)((Command >> 8)  & 0xF);
                    uint8_t R2 =   (uint8_t)((Command >> 12) & 0xF);
                    int32_t Imm = (uint32_t)((Command >> 16) & 0xFFFF);

                    #ifdef DEBUG_COUT
                    std::cout << "SUB R" << (unsigned int)R1 << " R" << (unsigned int)R2 << " " << Imm << std::endl;
                    #endif

                    CurrentState.Registers[R1] -= CurrentState.Registers[R2] + Imm;
                    return(OP_OK);
                }
            });
    }
    Emulator::~Emulator()
    {
        // ...
    }

    int Emulator::Run()
    {
        int ReturnCode = 0;
        uint32_t Command = 0;
        uint8_t Operation = 0;

        while(!ReturnCode)
        {
            Command = CurrentState.Memory.get()[CurrentState.Registers[15]];
            Operation = (uint8_t)(Command & 0xFF);

            #ifdef DEBUG_COUT
            std::cout << "R15: " << CurrentState.Registers[15] << std::endl;
            std::cout << "Command: " << Command << std::endl;
            #endif

            ReturnCode = InstructionsSet[Operation](Command, CurrentState);
            ++(CurrentState.Registers[15]);
        }
        return(0);
    }

    // PROTECTED:

    // PRIVATE:
}
