#include"../include/FUPM2EMU.h"
#include<iostream>
#include<cstdio>
#include<cstring>

// #define DEBUG_OUTPUT

namespace FUPM2EMU
{
    // Вспомогательные функции.
    // Конвертация четырёх uint8_t в uint32_t по указаному адресу.
    inline uint32_t ReadWord(uint8_t *Address)
    {
        return((uint32_t(Address[0]) << 24) | (uint32_t(Address[1]) << 16) | (uint32_t(Address[2]) << 8) | uint32_t(Address[3]));
    }

    // Конвертация uint32_t в четыре uint8_t и запись их в нужном порядке по указанному адресу.
    inline void WriteWord(uint32_t Value, uint8_t *Address)
    {
        Address[3] = uint8_t(Value & 0xFF); Value >>= 8;
        Address[2] = uint8_t(Value & 0xFF); Value >>= 8;
        Address[1] = uint8_t(Value & 0xFF); Value >>= 8;
        Address[0] = uint8_t(Value & 0xFF);
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
            // HALT - выключение процессора.
            [](uint32_t Command, State& CurrentState)->OpReturnCode
            {
                #ifdef DEBUG_OUTPUT
                std::cout << "HALT" << std::endl;
                #endif

                return(OP_TERMINATE);
            },

            // SYSCALL - системный вызов.
            [](uint32_t Command, State& CurrentState)->OpReturnCode
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

            // ADD - сложение регистров.
            [](uint32_t Command, State& CurrentState)->OpReturnCode
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

            // ADDI - прибавление к регистру непосредственного операнда.
            [](uint32_t Command, State& CurrentState)->OpReturnCode
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

            // SUB - разность регистров.
            [](uint32_t Command, State& CurrentState)->OpReturnCode
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

            // SUBI - вычитание из регистра непосредственного операнда.
            [](uint32_t Command, State& CurrentState)->OpReturnCode
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

            // MUL - произведение регистров.
            [](uint32_t Command, State& CurrentState)->OpReturnCode
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

            // MULI - произведение регистра на непосредственный операнд.
            [](uint32_t Command, State& CurrentState)->OpReturnCode
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

            // DIV - частное и остаток от деления пары регистров на регистр.
            [](uint32_t Command, State& CurrentState)->OpReturnCode
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

            // DIVI - частное и остаток от деления пары регистров на непосредственный операнд.
            [](uint32_t Command, State& CurrentState)->OpReturnCode
            {
                uint8_t R = 0;
                int32_t Imm = 0;
                ExtractRIargs(Command, R, Imm);

                #ifdef DEBUG_OUTPUT
                std::cout << "DIVI R" << (unsigned int)R << " " << Imm << std::endl;
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

            // Отсутствующие операции.
            [](uint32_t Command, State& CurrentState)->OpReturnCode { return(OP_ERROR); },
            [](uint32_t Command, State& CurrentState)->OpReturnCode { return(OP_ERROR); },

            // LC - загрузка константы в регистр.
            [](uint32_t Command, State& CurrentState)->OpReturnCode
            {
                uint8_t R = 0;
                int32_t Imm = 0;
                ExtractRIargs(Command, R, Imm);

                #ifdef DEBUG_OUTPUT
                std::cout << "LC R" << (unsigned int)R << " " << Imm << std::endl;
                #endif

                CurrentState.Registers[R] = Imm;
                return(OP_OK);
            }
        });

        // Эта штука превращается в монстра... Надо что-то придумать, так как половина кода тут повторяется.
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
                std::cerr << "FUPM2EMU has encountered a critical error. Shutting down." << std::endl;
                break;
            }
        }
        return(0);
    }

    int Emulator::LoadState(std::fstream &FileStream)
    {
        // Первые 16 * 4 + 1 байт - регистры + регистр флагов, остальное до конца файла - память.
        char Bytes[BytesInWord];

        // Регистры.
        for(size_t i = 0; i < RegistersNumber; ++i)
        {
            if(FileStream.read(Bytes, BytesInWord))
            {
                CurrentState.Registers[i] = ReadWord((uint8_t*)Bytes);
                #ifdef DEBUG_OUTPUT
                std::cout << "R" << i << ": " << CurrentState.Registers[i] << std::endl;
                #endif
            }
        }

        // Регистр флагов.
        if (FileStream.get(Bytes[0])) { CurrentState.Flags = uint8_t(Bytes[0]); }
        #ifdef DEBUG_OUTPUT
        std::cout << "Flags: " << (unsigned int)CurrentState.Flags << std::endl;
        #endif

        // Память.
        size_t Address = 0;
        while(FileStream.get(Bytes[0]) && (Address < (MemorySize * BytesInWord)) )
        {
            CurrentState.Memory[Address] = uint8_t(Bytes[0]);
            #ifdef DEBUG_OUTPUT
            std::cout << Address << ": " << (unsigned int)CurrentState.Memory[Address] << std::endl;
            #endif
            ++Address;
        }

        return(0);
    }

    // PROTECTED:

    // PRIVATE:
}
