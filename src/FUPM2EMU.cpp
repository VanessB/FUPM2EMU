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

        // Тестовые программы:

        // Сумма двух введённых чисел и восьми.
        /*
        setWord(0x01000064, 0); // SYSCALL R0 100.
        setWord(0x01100064, 1); // SYSCALL R1 100.
        setWord(0x0201FFF8, 2); // ADD R0 R1 -0x8.
        setWord(0x01000066, 3); // SYSCALL R0 102.
        setWord(0x04000000, 4); // SUB R0 R0 0x0.
        setWord(0x0300000A, 5); // ADDI R0 0xA.
        setWord(0x01000069, 6); // SYSCALL R0 105.
        */

        // (a * (b+1)) * 3
        /*
        setWord(0x01000064, 0); // SYSCALL R0 100.
        setWord(0x01100064, 1); // SYSCALL R1 100.
        setWord(0x06010001, 2); // MUL R0 R1 0x1.
        setWord(0x07000003, 3); // MULI R0 0x3.
        setWord(0x01000066, 4); // SYSCALL R0 102.
        setWord(0x04000000, 5); // SUB R0 R0 0x0.
        setWord(0x0300000A, 6); // ADDI R0 0xA.
        setWord(0x01000069, 7); // SYSCALL R0 105.
        */

        // (ab / c) / 3
        setWord(0x01000064, 0);  // SYSCALL R0 100.
        setWord(0x01100064, 1);  // SYSCALL R1 100.
        setWord(0x01200064, 2);  // SYSCALL R2 100.
        setWord(0x08020001, 3);  // DIV R0 R2 0x0.
        setWord(0x04110000, 4);  // SUB R1 R1 0x0.
        setWord(0x09000003, 5);  // DIVI R0 0x3.
        setWord(0x01000066, 6);  // SYSCALL R0 102.
        setWord(0x04000000, 7);  // SUB R0 R0 0x0.
        setWord(0x0300000A, 8);  // ADDI R0 0xA.
        setWord(0x01000069, 9);  // SYSCALL R0 105.
        setWord(0x01100066, 10); // SYSCALL R1 102.
        setWord(0x01000069, 11); // SYSCALL R0 105.
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
        InstructionsSet = std::vector<std::function<OpReturnCode (uint32_t, State&)>>
        ({
            [](uint32_t Command, State& CurrentState)->OpReturnCode // HALT - выключение процессора.
            {
                #ifdef DEBUG_OUTPUT
                std::cout << "HALT" << std::endl;
                #endif

                return(OP_TERMINATE);
            },
            [](uint32_t Command, State& CurrentState)->OpReturnCode // SYSCALL - системный вызов.
            {
                OpReturnCode ReturnCode = OP_OK;

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
            [](uint32_t Command, State& CurrentState)->OpReturnCode // ADD - сложение регистров.
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
            [](uint32_t Command, State& CurrentState)->OpReturnCode // ADDI - прибавление к регистру непосредственного операнда.
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
            [](uint32_t Command, State& CurrentState)->OpReturnCode // SUB - разность регистров.
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
            [](uint32_t Command, State& CurrentState)->OpReturnCode // SUBI - вычитание из регистра непосредственного операнда.
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
            [](uint32_t Command, State& CurrentState)->OpReturnCode // MUL - произведение регистров.
            {
                uint8_t R1 = 0;
                uint8_t R2 = 0;
                int32_t Imm = 0;
                ExtractRRargs(Command, R1, R2, Imm);

                #ifdef DEBUG_OUTPUT
                std::cout << "MUL R" << (unsigned int)R1 << " R" << (unsigned int)R2 << " " << Imm << std::endl;
                #endif

                // Результат умножения приведёт к выходу за пределы существующих регистров.
                if (R1 + 1 >= RegistersNumber) { throw(OPEX_INVALIDREG); }

                int64_t Product = int64_t(CurrentState.Registers[R1]) * int64_t(CurrentState.Registers[R2] + Imm);
                CurrentState.Registers[R1] = int32_t(Product & 0xFFFFFFFF);
                CurrentState.Registers[R1 + 1] = int32_t((Product >> BitsInWord) & 0xFFFFFFFF);
                return(OP_OK);
            },
            [](uint32_t Command, State& CurrentState)->OpReturnCode // MULI - произведение регистра на непосредственный операнд.
            {
                uint8_t R = 0;
                int32_t Imm = 0;
                ExtractRIargs(Command, R, Imm);

                #ifdef DEBUG_OUTPUT
                std::cout << "MULI R" << (unsigned int)R << " " << Imm << std::endl;
                #endif

                // Результат умножения приведёт к выходу за пределы существующих регистров.
                if (R + 1 >= RegistersNumber) { throw(OPEX_INVALIDREG); }

                int64_t Product = int64_t(CurrentState.Registers[R]) * int64_t(Imm);
                CurrentState.Registers[R] = int32_t(Product & 0xFFFFFFFF);
                CurrentState.Registers[R + 1] = int32_t((Product >> BitsInWord) & 0xFFFFFFFF);
                return(OP_OK);
            },
            [](uint32_t Command, State& CurrentState)->OpReturnCode // DIV - частное и остаток от деления пары регистров на регистр.
            {
                uint8_t R1 = 0;
                uint8_t R2 = 0;
                int32_t Imm = 0;
                ExtractRRargs(Command, R1, R2, Imm);

                #ifdef DEBUG_OUTPUT
                std::cout << "DIV R" << (unsigned int)R1 << " R" << (unsigned int)R2 << " " << Imm << std::endl;
                #endif

                // Результат деления приведёт к выходу за пределы существующих регистров.
                if (R1 + 1 >= RegistersNumber) { throw(OPEX_INVALIDREG); }
                // Происходит деление на ноль.
                if (!CurrentState.Registers[R2]) { throw(OPEX_DIVBYZERO); }

                int64_t Divident = int64_t(CurrentState.Registers[R1] | (int64_t(CurrentState.Registers[R1 + 1]) << BitsInWord));
                int64_t Divider = int64_t(CurrentState.Registers[R2]);
                int64_t Product = Divident / Divider;

                // Результат деления не помещается в регистр. По спецификации - деление на ноль.
                //std::cout << Product << std::endl;
                if (Product > 0x00000000FFFFFFFF) { throw(OPEX_DIVBYZERO); }

                int64_t Remainder = Divident % Divider;

                CurrentState.Registers[R1] = int32_t(Product & 0xFFFFFFFF);
                CurrentState.Registers[R1 + 1] = int32_t(Remainder & 0xFFFFFFFF);
                return(OP_OK);
            },
            [](uint32_t Command, State& CurrentState)->OpReturnCode // DIVI - частное и остаток от деления пары регистров на непосредственный операнд.
            {
                uint8_t R = 0;
                int32_t Imm = 0;
                ExtractRIargs(Command, R, Imm);

                #ifdef DEBUG_OUTPUT
                std::cout << "DIVI R" << (unsigned int)R1 << " " << Imm << std::endl;
                #endif

                // Результат деления приведёт к выходу за пределы существующих регистров.
                if (R + 1 >= RegistersNumber) { throw(OPEX_INVALIDREG); }
                // Происходит деление на ноль.
                if (!Imm) { throw(OPEX_DIVBYZERO); }

                int64_t Divident = int64_t(CurrentState.Registers[R] | (int64_t(CurrentState.Registers[R + 1]) << BitsInWord));
                int64_t Divider = int64_t(Imm);
                int64_t Product = Divident / Divider;

                // Результат деления не помещается в регистр. По спецификации - деление на ноль.
                //std::cout << Product << std::endl;
                if (Product > 0x00000000FFFFFFFF) { throw(OPEX_DIVBYZERO); }

                int64_t Remainder = Divident % Divider;

                CurrentState.Registers[R] = int32_t(Product & 0xFFFFFFFF);
                CurrentState.Registers[R + 1] = int32_t(Remainder & 0xFFFFFFFF);
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

            try
            {
                ReturnCode = InstructionsSet[Operation](Command, CurrentState);
                ++(CurrentState.Registers[15]);
            }
            catch (OpException Exception)
            {
                switch(Exception)
                {
                    case OPEX_OK: { break; }
                    case OPEX_INVALIDREG:
                    {
                        std::cerr << "[ERROR]: access to an invalid register." << std::endl;
                        break;
                    }
                    case OPEX_DIVBYZERO:
                    {
                        std::cerr << "[ERROR]: division by zero." << std::endl;
                        break;
                    }
                    default: { break; }
                }
                break;
            }
        }
        return(0);
    }

    // PROTECTED:

    // PRIVATE:
}
