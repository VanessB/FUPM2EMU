#include"../include/FUPM2EMU.h"
#include<iostream>
#include<cstdio>
#include<cstring>

//#define DEBUG_OUTPUT

namespace FUPM2EMU
{
    // Вспомогательные функции.
    // Конвертация четырёх uint8_t в uint32_t по указаному адресу.
    inline uint32_t ReadWord(uint8_t *Address)
    {
        return(uint32_t(Address[0]) | (uint32_t(Address[1]) << 8) | (uint32_t(Address[2]) << 16) | (uint32_t(Address[3]) << 24));
    }

    // Конвертация uint32_t в четыре uint8_t и запись их в нужном порядке по указанному адресу.
    inline void WriteWord(uint32_t Value, uint8_t *Address)
    {
        Address[0] = uint8_t(Value & 0xFF); Value >>= 8;
        Address[1] = uint8_t(Value & 0xFF); Value >>= 8;
        Address[2] = uint8_t(Value & 0xFF); Value >>= 8;
        Address[3] = uint8_t(Value & 0xFF);
    }


    // Внутренние вспомогательные функции.
    // Получение аргументов команды типа RI.
    inline void ExtractRIargs (uint32_t Command, uint8_t &R, int32_t &Imm)
    {
        R = uint8_t((Command >> 20) & 0xF);
        Imm = Command & 0xFFFFF;
        if (Imm & (1 << 19)) { Imm |= 0xFFF00000; } // Обработка случая с отрицательным Imm.
    }
    inline void ExtractRRargs (uint32_t Command, uint8_t &R1, uint8_t &R2, int32_t &Imm)
    {
        R1 = uint8_t((Command >> 20) & 0xF);
        R2 = uint8_t((Command >> 16) & 0xF);
        Imm = Command & 0xFFFF;
        if (Imm & (1 << 15)) { Imm |= 0xFFFF0000; } // Обработка случая с отрицательным Imm.
    }



    //////////////// STATE ////////////////
    State::State()
    {
        // Заполнение нулями регистров.
        std::memset(Registers, 0, RegistersNumber * sizeof(uint32_t));

        // Обнуление регистра флагов.
        Flags = 0;

        // Создание и заполнение нулями блока памяти.
        Memory = std::vector<uint8_t>(MemorySize * BytesInWord, 0);

        // Тестовая программа:
        setWord(0x01000064, 0); // SYSCALL R0 100.
        setWord(0x01100064, 1); // SYSCALL R1 100.
        setWord(0x0201FFF8, 2); // ADD R0 R1 -0x8.
        setWord(0x01000066, 3); // SYSCALL R0 102.
        setWord(0x04000000, 4); // SUB R0 R0 0x0.
        setWord(0x0300000A, 5); // ADDI R0 0xA.
        setWord(0x01000069, 6); // SYSCALL R0 105.
    }
    State::~State()
    {
        // ...
    }

    uint32_t State::getWord(size_t Address)
    {
        return(ReadWord(&(Memory[Address * BytesInWord])));
    }
    void State::setWord(uint32_t Value, size_t Address)
    {
        WriteWord(Value, &(Memory[Address * BytesInWord]));
    }



    //////////////// EMULATOR ////////////////
    // PUBLIC:
    Emulator::Emulator()
    {
        // !!! TODO !!!: разобраться с отрицательными Imm.

        // Инициализация массива инструкций (используется возможность преобразования лямбда-выражений к указателю на оператор ()).
        // Каждая инструкция возвращает код возврата. Он влияет на дальнейшую работу эмулятора (например, OP_TERMINATE завершает выполнение).
        InstructionsSet = std::vector<std::function<int (uint32_t, State&)>>
            ({
                [](uint32_t Command, State& CurrentState) // HALT - выключение процессора.
                {
                    #ifdef DEBUG_OUTPUT
                    std::cout << "HALT" << std::endl;
                    #endif

                    return(OP_TERMINATE);
                },
                [](uint32_t Command, State& CurrentState) // SYSCALL - системный вызов.
                {
                    int ReturnCode = OP_OK;

                    uint8_t R = 0;
                    int32_t Code = 0;
                    ExtractRIargs(Command, R, Code);

                    #ifdef DEBUG_OUTPUT
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
                            putchar(uint8_t(CurrentState.Registers[R]));
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
                    uint8_t R1 = 0;
                    uint8_t R2 = 0;
                    int32_t Imm = 0;
                    ExtractRRargs(Command, R1, R2, Imm);

                    #ifdef DEBUG_OUTPUT
                    std::cout << "ADD R" << (unsigned int)R1 << " R" << (unsigned int)R2 << " " << Imm << std::endl;
                    #endif

                    CurrentState.Registers[R1] += CurrentState.Registers[R2] + Imm;
                    return(OP_OK);
                },
                [](uint32_t Command, State& CurrentState) // ADDI - прибавление к регистру непосредственного операнда.
                {
                    uint8_t R = 0;
                    int32_t Imm = 0;
                    ExtractRIargs(Command, R, Imm);

                    #ifdef DEBUG_OUTPUT
                    std::cout << "ADDI R" << (unsigned int)R << " " << Imm << std::endl;
                    #endif

                    CurrentState.Registers[R] += Imm;
                    return(OP_OK);
                },
                [](uint32_t Command, State& CurrentState) // SUB - разность регистров.
                {
                    uint8_t R1 = 0;
                    uint8_t R2 = 0;
                    int32_t Imm = 0;
                    ExtractRRargs(Command, R1, R2, Imm);

                    #ifdef DEBUG_OUTPUT
                    std::cout << "SUB R" << (unsigned int)R1 << " R" << (unsigned int)R2 << " " << Imm << std::endl;
                    #endif

                    CurrentState.Registers[R1] -= CurrentState.Registers[R2] + Imm;
                    return(OP_OK);
                },
                [](uint32_t Command, State& CurrentState) // SUBI - вычитание из регистра непосредственного операнда.
                {
                    uint8_t R = 0;
                    int32_t Imm = 0;
                    ExtractRIargs(Command, R, Imm);

                    #ifdef DEBUG_OUTPUT
                    std::cout << "SUBI R" << (unsigned int)R << " " << Imm << std::endl;
                    #endif

                    CurrentState.Registers[R] -= Imm;
                    return(OP_OK);
                },
            });
    }
    Emulator::~Emulator()
    {
        // ...
    }

    int Emulator::Run()
    {
        int ReturnCode = 0;    // Код возврата операции.
        uint32_t Command = 0;  // 32 бита под команду.
        uint8_t Operation = 0; // 8 бит под операцию.

        // Пока все хорошо.
        while(ReturnCode == OP_OK)
        {
            Command = CurrentState.getWord(CurrentState.Registers[15]); // Чтение слова по адресу в R15 (в байтах - по адресу R15 * 4).
            Operation = uint8_t((Command >> 24) & 0xFF);

            #ifdef DEBUG_OUTPUT
            std::cout << "R15: " << CurrentState.Registers[15] << std::endl;
            std::cout << "Command: 0x" << std::hex << Command << std::dec <<  std::endl;
            #endif

            ReturnCode = InstructionsSet[Operation](Command, CurrentState);
            ++(CurrentState.Registers[15]);
        }
        return(0);
    }

    // PROTECTED:

    // PRIVATE:
}
