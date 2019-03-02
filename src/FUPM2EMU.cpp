#include"../include/FUPM2EMU.h"
#include<iostream>
#include<cstdio>
#include<cstring>

//#define DEBUG_COUT

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



    //////////////// STATE ////////////////
    State::State()
    {
        // Заполнение нулями регистров.
        std::memset(Registers, 0, RegistersNumber * sizeof(uint32_t));

        // Обнуление регистра флагов.
        Flags = 0;

        // Создание и заполнение нулями блока памяти.
        Memory = std::vector<uint8_t>(MemorySize * BytesInWord, 0);
        //std::memset(Memory.get(), 0, MemorySize * sizeof(uint8_t) * BytesInWord);

        // Тестовая программа:
        WriteWord(0x01000064, &(Memory[0 * BytesInWord])); // SYSCALL R0 100.
        WriteWord(0x01100064, &(Memory[1 * BytesInWord])); // SYSCALL R1 100.
        WriteWord(0x02010008, &(Memory[2 * BytesInWord])); // ADD R0 R1 0x8.
        WriteWord(0x01000066, &(Memory[3 * BytesInWord])); // SYSCALL R0 102.
        WriteWord(0x04000000, &(Memory[4 * BytesInWord])); // SUB R0 R0 0x0.
        WriteWord(0x0300000A, &(Memory[5 * BytesInWord])); // ADDI R0 0xA.
        WriteWord(0x01000069, &(Memory[6 * BytesInWord])); // SYSCALL R0 105.
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
        // Инициализация массива инструкций (используется возможность преобразования лямбда-выражений к указателю на оператор ()).
        // Каждая инструкция возвращает код возврата. Он влияет на дальнейшую работу эмулятора (например, OP_TERMINATE завершает выполнение).
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

                    uint8_t R = uint8_t((Command >> 20) & 0xF);
                    uint32_t Code = Command & 0xFFFFF;

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
                    uint8_t R1 =   uint8_t((Command >> 20) & 0xF);
                    uint8_t R2 =   uint8_t((Command >> 16) & 0xF);
                    int32_t Imm = Command & 0xFFFF;

                    #ifdef DEBUG_COUT
                    std::cout << "ADD R" << (unsigned int)R1 << " R" << (unsigned int)R2 << " " << Imm << std::endl;
                    #endif

                    CurrentState.Registers[R1] += CurrentState.Registers[R2] + Imm;
                    return(OP_OK);
                },
                [](uint32_t Command, State& CurrentState) // ADDI - прибавление к регистру непосредственного операнда.
                {
                    uint8_t R = uint8_t((Command >> 20) & 0xF);
                    int32_t Imm = Command & 0xFFFFF;

                    #ifdef DEBUG_COUT
                    std::cout << "ADDI R" << (unsigned int)R << " " << Imm << std::endl;
                    #endif

                    CurrentState.Registers[R] += Imm;
                    return(OP_OK);
                },
                [](uint32_t Command, State& CurrentState) // SUB - разность регистров.
                {
                    uint8_t R1 =   uint8_t((Command >> 20) & 0xF);
                    uint8_t R2 =   uint8_t((Command >> 16) & 0xF);
                    int32_t Imm = Command & 0xFFFF;

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
        int ReturnCode = 0;    // Код возврата операции.
        uint32_t Command = 0;  // 32 бита под команду.
        uint8_t Operation = 0; // 8 бит под операцию.

        // Пока все хорошо.
        while(ReturnCode == OP_OK)
        {
            Command = CurrentState.getWord(CurrentState.Registers[15]); // Чтение слова по адресу в R15 (в байтах - по адресу R15 * 4).
            Operation = uint8_t((Command >> 24) & 0xFF);

            #ifdef DEBUG_COUT
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
