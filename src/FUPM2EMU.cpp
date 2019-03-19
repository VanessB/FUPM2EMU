#include"../include/FUPM2EMU.h"
#include<limits>
#include<iostream>
#include<cstdio>
#include<cstring>

//#define EXECUTION_DEBUG_OUTPUT
//#define LODINGSTATE_DEBUG_OUTPUT

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



    /////////////////  STATE  ////////////////
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

    int State::load(std::fstream &FileStream)
    {
        // Первые 16 * 4 + 1 байт - регистры + регистр флагов, остальное до конца файла - память.
        char Bytes[BytesInWord];

        // Регистры.
        for(size_t i = 0; i < RegistersNumber; ++i)
        {
            if(FileStream.read(Bytes, BytesInWord))
            {
                Registers[i] = ReadWord((uint8_t*)Bytes);
                #ifdef LOADINGSTATE_DEBUG_OUTPUT
                std::cout << "R" << i << ": " << Registers[i] << std::endl;
                #endif
            }
        }

        // Регистр флагов.
        if (FileStream.get(Bytes[0])) { Flags = uint8_t(Bytes[0]); }
        #ifdef LOADINGSTATE_DEBUG_OUTPUT
        std::cout << "Flags: " << (unsigned int)Flags << std::endl;
        #endif

        // Память.
        size_t Address = 0;
        while(FileStream.get(Bytes[0]) && (Address < (MemorySize * BytesInWord)))
        {
            Memory[Address] = uint8_t(Bytes[0]);
            #ifdef LOADINGSTATE_DEBUG_OUTPUT
            std::cout << Address << ": " << (unsigned int)Memory[Address] << std::endl;
            #endif
            ++Address;
        }

        return(0);
    }

    uint32_t State::getWord(size_t Address)
    {
        return(ReadWord(&(Memory[Address * BytesInWord])));
    }
    void State::setWord(uint32_t Value, size_t Address)
    {
        WriteWord(Value, &(Memory[Address * BytesInWord]));
    }



    //////////////// EXECUTOR ////////////////
    // PUBLIC:
    Executor::Executor()
    {
        // ...
    }
    Executor::~Executor()
    {
        // ...
    }

    // Выполнение комманды.
    inline Executor::ReturnCode Executor::step(State &OperatedState)
    {
        // Извлечение следующией команды.
        uint32_t Command = OperatedState.getWord(OperatedState.Registers[15]);

        // Быстрее вычислить все возможные операнды сразу, чем использовать if else.
        OPERATION_CODE Operation = OPERATION_CODE((Command >> 24) & 0xFF);
        uint8_t R1 = (Command >> 20) & 0xF;
        uint8_t R2 = (Command >> 16) & 0xF;
        int32_t Imm16 = Command & 0x0FFFF;
        int32_t Imm20 = Command & 0xFFFFF;

        ReturnCode ReturnCode = ReturnCode::OK;

        #ifdef EXECUTION_DEBUG_OUTPUT
        std::cout << "OPCODE: " << Operation << std::endl;
        std::cout << "Registers: " << "R" << (unsigned int)R1 << " R" << (unsigned int)R2 << std::endl;
        std::cout << "Immediates: " << "Imm16: " << Imm16 << " Imm20: " << Imm20 << std::endl;
        #endif

        try
        {
            switch(Operation)
            {
                // HALT - выключение процессора.
                case HALT:
                {
                    ReturnCode = ReturnCode::TERMINATE;
                    break;
                }

                // SYSCALL - системный вызов.
                case SYSCALL:
                {
                    switch (Imm20)
                    {
                        // EXIT - выход.
                        case 0:
                        {
                            ReturnCode = ReturnCode::TERMINATE;
                            break;
                        }
                        // SCANINT - запрос целого числа.
                        case 100:
                        {
                            std::cin >> OperatedState.Registers[R1];
                            break;
                        }
                        // PRINTINT - вывод целого числа.
                        case 102:
                        {
                            std::cout << OperatedState.Registers[R1];
                            break;
                        }
                        // PUTCHAR - вывод символа.
                        case 105:
                        {
                            putchar(uint8_t(OperatedState.Registers[R1]));
                            break;
                        }
                        // Использован неспецифицированный код системного вызова.
                        default:
                        {
                            ReturnCode = ReturnCode::ERROR;
                            break;
                        }
                    }
                    break;
                }

                // ADD - сложение регистров.
                case ADD:
                {
                    OperatedState.Registers[R1] += OperatedState.Registers[R2] + Imm16;
                    break;
                }

                // ADDI - прибавление к регистру непосредственного операнда.
                case ADDI:
                {
                    OperatedState.Registers[R1] += Imm20;
                    break;
                }

                // SUB - разность регистров.
                case SUB:
                {
                    OperatedState.Registers[R1] -= OperatedState.Registers[R2] + Imm16;
                    break;
                }

                // SUBI - вычитание из регистра непосредственного операнда.
                case SUBI:
                {
                    OperatedState.Registers[R1] -= Imm20;
                    break;
                }

                // MUL - произведение регистров.
                case MUL:
                {
                    // Результат умножения приведёт к выходу за пределы существующих регистров.
                    if (R1 + 1 >= RegistersNumber) { throw(OperationException::INVALIDREG); }

                    int64_t Product = int64_t(OperatedState.Registers[R1]) * int64_t(OperatedState.Registers[R2] + Imm16);
                    OperatedState.Registers[R1] = int32_t(Product & 0xFFFFFFFF);
                    OperatedState.Registers[R1 + 1] = int32_t((Product >> BitsInWord) & 0xFFFFFFFF);
                    break;
                }

                // MULI - произведение регистра на непосредственный операнд.
                case MULI:
                {
                    // Результат умножения приведёт к выходу за пределы существующих регистров.
                    if (R1 + 1 >= RegistersNumber) { throw(OperationException::INVALIDREG); }

                    int64_t Product = int64_t(OperatedState.Registers[R1]) * int64_t(Imm20);
                    OperatedState.Registers[R1] = int32_t(Product & 0xFFFFFFFF);
                    OperatedState.Registers[R1 + 1] = int32_t((Product >> BitsInWord) & 0xFFFFFFFF);
                    break;
                }

                // DIV - частное и остаток от деления пары регистров на регистр.
                case DIV:
                {
                    // Результат деления приведёт к выходу за пределы существующих регистров.
                    if (R1 + 1 >= RegistersNumber) { throw(OperationException::INVALIDREG); }
                    // Происходит деление на ноль.
                    if (!OperatedState.Registers[R2]) { throw(OperationException::DIVBYZERO); }

                    int64_t Divident = int64_t(OperatedState.Registers[R1] | (int64_t(OperatedState.Registers[R1 + 1]) << BitsInWord));
                    int64_t Divider = int64_t(OperatedState.Registers[R2]);
                    int64_t Product = Divident / Divider;

                    // Результат деления не помещается в регистр. По спецификации - деление на ноль.
                    //std::cout << Product << std::endl;
                    if (Product > 0x00000000FFFFFFFF) { throw(OperationException::DIVBYZERO); }

                    int64_t Remainder = Divident % Divider;

                    OperatedState.Registers[R1] = int32_t(Product & 0xFFFFFFFF);
                    OperatedState.Registers[R1 + 1] = int32_t(Remainder & 0xFFFFFFFF);
                    break;
                }

                // DIVI - частное и остаток от деления пары регистров на непосредственный операнд.
                case DIVI:
                {
                    // Результат деления приведёт к выходу за пределы существующих регистров.
                    if (R1 + 1 >= RegistersNumber) { throw(OperationException::INVALIDREG); }
                    // Происходит деление на ноль.
                    if (!Imm20) { throw(OperationException::DIVBYZERO); }

                    int64_t Divident = int64_t(OperatedState.Registers[R1] | (int64_t(OperatedState.Registers[R1 + 1]) << BitsInWord));
                    int64_t Divider = int64_t(Imm20);
                    int64_t Product = Divident / Divider;

                    // Результат деления не помещается в регистр. По спецификации - деление на ноль.
                    //std::cout << Product << std::endl;
                    if (Product > 0x00000000FFFFFFFF) { throw(OperationException::DIVBYZERO); }

                    int64_t Remainder = Divident % Divider;

                    OperatedState.Registers[R1] = int32_t(Product & 0xFFFFFFFF);
                    OperatedState.Registers[R1 + 1] = int32_t(Remainder & 0xFFFFFFFF);
                    break;
                }

                // LC - загрузка константы в регистр.
                case LC:
                {
                    OperatedState.Registers[R1] = Imm20;
                    break;
                }
                default:
                {
                    ReturnCode = ReturnCode::ERROR;
                    break;
                }
            }
        }
        catch (OperationException Exception)
        {
            switch(Exception)
            {
                case OperationException::OK: { break; }
                case OperationException::INVALIDREG:
                {
                    std::cerr << "[EXECUTION ERROR]: access to an invalid register." << std::endl;
                    throw(Exception::INVALIDSTATE);
                    break;
                }
                case OperationException::DIVBYZERO:
                {
                    std::cerr << "[EXECUTION ERROR]: division by zero." << std::endl;
                    throw(Exception::MACHINE);
                    break;
                }
                default: { break; }
            }
        }

        ++(OperatedState.Registers[15]);
        return(ReturnCode);
    }

    // PROTECTED:

    // PRIVATE:


    /////////////// TRANSLATOR ///////////////
    // PUBLIC:
    Translator::Translator()
    {
        // Таблица, связывающая имя операции, её код и тип.
        std::vector<std::tuple<std::string, int, int>> TranslationTable =
        {
            {"halt",    HALT,    RI},
            {"syscall", SYSCALL, RI},
            {"add",     ADD,     RR},
            {"addi",    ADDI,    RI},
            {"sub",     SUB,     RR},
            {"subi",    SUBI,    RI},
            {"mul",     MUL,     RR},
            {"muli",    MULI,    RI},
            {"div",     DIV,     RR},
            {"divi",    DIVI,    RI},
            {"lc",      LC,      RI}
        };

        // Заполнение отображений.
        for (size_t i = 0; i < TranslationTable.size(); ++i)
        {
            OpCode.insert({ std::get<0>(TranslationTable[i]), std::get<1>(TranslationTable[i]) });
            OpType.insert({ std::get<0>(TranslationTable[i]), std::get<2>(TranslationTable[i]) });
        }

        // Инициализация отображения имён регистров в их коды.
        RegCode =
        {
            {"r0",  0 },
            {"r1",  1 },
            {"r2",  2 },
            {"r3",  3 },
            {"r4",  4 },
            {"r5",  5 },
            {"r6",  6 },
            {"r7",  7 },
            {"r8",  8 },
            {"r9",  9 },
            {"r10", 10},
            {"r11", 11},
            {"r12", 12},
            {"r13", 13},
            {"r14", 14},
            {"r15", 15}
        };
    }
    Translator::~Translator()
    {
        // ...
    }

    int Translator::Assemble(std::fstream &FileStream, FUPM2EMU::State &OperatedState)
    {
        size_t WriteAddress = 0; // Адрес текущего записываемого слова в OperatedState.
        std::string Input;       // Строка для считывания слов из входного файла.

        enum class ParserState
        {
            IN_CODE    = 0,
            IN_COMMENT = 1,
        };
        ParserState CurrentState = ParserState::IN_CODE;

        // Чтение идёт до конца файла.
        while(!FileStream.eof())
        {
            switch(CurrentState)
            {
                case ParserState::IN_CODE:
                {
                    if (!(FileStream >> Input)) { break; } // Проверка на успешность чтения.
                    if (!Input.size()) { continue; }       // Проверка строки на пустоту.

                    if (Input[0] == ';')
                    {
                        CurrentState = ParserState::IN_COMMENT;
                        break;
                    }

                    uint32_t Word = 0; // Переменная для записываемого слова команды.

                    // Парсим операцию.
                    Word |= OpCode.at(Input) << (BitsInWord - OpCodeBits);

                    // Парсим аргументы.
                    switch(OpType.at(Input))
                    {
                        // Регистр и непосредственный операнд.
                        case RI:
                        {
                            // Регистр.
                            FileStream >> Input;
                            Word |= RegCode.at(Input) << (BitsInWord - OpCodeBits - RegCodeBits);

                            // Непосредственный операнд.
                            FileStream >> Input;

                            // Проверка, число ли это.
                            if (Input.find_first_not_of("0123456789") == std::string::npos)
                            {
                                // Перевод ввода в число и запись в конец слова.
                                Word |= (std::stoi(Input) & 0xFFFFFF); // 24 бита на длинный Imm24.
                            }
                            break;
                        }

                        // Два регистра и короткий непосредственный операнд.
                        case RR:
                        {
                            // Первый регистр.
                            FileStream >> Input;
                            Word |= RegCode.at(Input) << (BitsInWord - OpCodeBits - RegCodeBits);

                            // Второй регистр.
                            FileStream >> Input;
                            Word |= RegCode.at(Input) << (BitsInWord - OpCodeBits - RegCodeBits - RegCodeBits);

                            // Непосредственный операнд.
                            FileStream >> Input;
                            // Перевод ввода в число и запись в конец слова.
                            Word |= (std::stoi(Input) & 0xFFFFF); // 20 бит на короткий Imm20.
                            break;
                        }

                        // Регистр и адрес.
                        case RM:
                        {
                            break;
                        }

                        // Адрес.
                        case J:
                        {
                            break;
                        }
                    }

                    // Запись слова.
                    OperatedState.setWord(Word, WriteAddress);
                    ++WriteAddress;
                    break;
                }
                case ParserState::IN_COMMENT:
                {
                    FileStream.ignore (std::numeric_limits<std::streamsize>::max(), '\n');
                    CurrentState = ParserState::IN_CODE;
                    // while (!FileStream.eof() || (FileStream.get() != '\n')); // Пока не кончилась строка или файл - пропускаем...
                    break;
                }
            }
        }
        return(0);
    }

    // PROTECTED:

    // PRIVATE:


    //////////////// EMULATOR ////////////////
    // PUBLIC:
    Emulator::Emulator()
    {
        // ...
    }
    Emulator::~Emulator()
    {
        // ...
    }

    int Emulator::Run()
    {
        Executor::ReturnCode ReturnCode = Executor::ReturnCode::OK; // Код возврата операции.

        // Пока все хорошо.
        while(ReturnCode == Executor::ReturnCode::OK)
        {
            try
            {
                ReturnCode = executor.step(state);
            }
            catch (Executor::Exception Exception)
            {
                switch(Exception)
                {
                    case Executor::Exception::OK: { break; }
                    case Executor::Exception::MACHINE:
                    {
                        std::cerr << "[EMULATOR ERROR]: emulated machine has thrown an exception." << std::endl;
                        break;
                    }
                    case Executor::Exception::INVALIDSTATE:
                    {
                        std::cerr << "[EMULATOR ERROR]: machine state has become invalid." << std::endl;
                        break;
                    }
                    default: { break; }
                }
                std::cerr << "FUPM2EMU has encountered a critical error. Shutting down." << std::endl;
                break;
            }

            #ifdef EXECUTION_DEBUG_OUTPUT
            std::cout << "ReturnCode: " << ReturnCode << std::endl;
            #endif
        }
        return(0);
    }

    // PROTECTED:

    // PRIVATE:
}
