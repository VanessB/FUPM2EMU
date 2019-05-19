#include"../include/FUPM2EMU.h"
#include<limits>
#include<iostream>
#include<cstdio>
#include<cstring>

//#define DEBUG_OUTPUT_EXECUTION
//#define DEBUG_OUTPUT_LOADINGSTATE
//#define DEBUG_OUTPUT_ASSEMBLING
//#define DEBUG_EXECUTION_STEPS

namespace FUPM2EMU
{
    /////////////////  STATE  ////////////////
    // Настройки компиляции:
    #define MEMORY_MOD          // Модульная адресация.
    //#define MEMORY_EXCEPTIONS // Исключение при выходе за пределы адресного пространства.

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
                Registers[i] = (uint32_t(Bytes[0]) << 24) | (uint32_t(Bytes[1]) << 16) | (uint32_t(Bytes[2]) << 8) | uint32_t(Bytes[3]);
                #ifdef DEBUG_OUTPUT_LOADINGSTATE
                std::cout << "R" << i << ": " << Registers[i] << std::endl;
                #endif
            }
        }

        // Регистр флагов.
        if (FileStream.get(Bytes[0])) { Flags = uint8_t(Bytes[0]); }
        #ifdef DEBUG_OUTPUT_LOADINGSTATE
        std::cout << "Flags: " << (unsigned int)Flags << std::endl;
        #endif

        // Память.
        size_t Address = 0;
        while(FileStream.get(Bytes[0]) && (Address < (MemorySize * BytesInWord)))
        {
            Memory[Address] = uint8_t(Bytes[0]);
            #ifdef DEBUG_OUTPUT_LOADINGSTATE
            std::cout << Address << ": " << (unsigned int)Memory[Address] << std::endl;
            #endif
            ++Address;
        }

        return(0);
    }


    inline uint32_t State::getWord(size_t Address) const
    {
        // Адресация по модулю.
        #ifdef MEMORY_MOD
        Address %= MemorySize;
        #endif

        // Исключение при выходе за пределы адресного пространства.
        #ifdef MEMORY_EXCEPTIONS
        if (Address > MemorySize) { throw(Exception::MEMORY); }
        #endif

        size_t ByteAddress = Address * BytesInWord;
        return((uint32_t(Memory[ByteAddress]) << 24) | (uint32_t(Memory[ByteAddress + 1]) << 16) | (uint32_t(Memory[ByteAddress + 2]) << 8) | uint32_t(Memory[ByteAddress + 3]));
    }
    inline void State::setWord(uint32_t Value, size_t Address)
    {
        // Адресация по модулю.
        #ifdef MEMORY_MOD
        Address %= MemorySize;
        #endif

        // Исключение при выходе за пределы адресного пространства.
        #ifdef MEMORY_EXCEPTIONS
        if (Address > MemorySize) { throw(Exception::MEMORY); }
        #endif

        size_t ByteAddress = Address * BytesInWord;
        Memory[ByteAddress + 3] = uint8_t(Value & 0xFF); Value >>= 8;
        Memory[ByteAddress + 2] = uint8_t(Value & 0xFF); Value >>= 8;
        Memory[ByteAddress + 1] = uint8_t(Value & 0xFF); Value >>= 8;
        Memory[ByteAddress] = uint8_t(Value & 0xFF);
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

        // Код будет короче, если вычислить все возможные операнды сразу.
        OPERATION_CODE Operation = OPERATION_CODE((Command >> 24) & 0xFF);
        uint8_t R1 = (Command >> 20) & 0xF;
        uint8_t R2 = (Command >> 16) & 0xF;
        int32_t Imm16 = Command & 0x0FFFF;
        int32_t Imm20 = Command & 0xFFFFF;

        ReturnCode ReturnCode = ReturnCode::OK;

        #ifdef DEBUG_OUTPUT_EXECUTION
        std::cout << "OPCODE: " << Operation << std::endl;
        std::cout << "Registers:"
                  << " R" << (unsigned int)R1 << ": " << OperatedState.Registers[R1]
                  << " R" << (unsigned int)R2 << ": " << OperatedState.Registers[R2] << std::endl;
        std::cout << "Immediates: " << "Imm16: " << Imm16 << " Imm20: " << Imm20 << std::endl;
        #endif

        try
        {
            switch(Operation)
            {
                // СИСТЕМНОЕ.
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
                        // SCANDOUBLE - запрос вещественного числа.
                        case 101:
                        {
                            // Результат выполнения команды приведёт к выходу за пределы существующих регистров.
                            if (R1 + 1 >= State::RegistersNumber) { throw(OperationException::INVALIDREG); }

                            double Input = 0.0;
                            std::cin >> Input;
                            *(double*)(OperatedState.Registers + R1) = Input;
                            break;
                        }
                        // PRINTINT - вывод целого числа.
                        case 102:
                        {
                            std::cout << OperatedState.Registers[R1];
                            break;
                        }
                        // PRINTDOUBLE - вывод вещественного числа.
                        case 103:
                        {
                            // Результат выполнения команды приведёт к выходу за пределы существующих регистров.
                            if (R1 + 1 >= State::RegistersNumber) { throw(OperationException::INVALIDREG); }

                            std::cout << *(double*)(OperatedState.Registers + R1);
                            break;
                        }
                        // PUTCHAR - вывод символа.
                        case 105:
                        {
                            putchar(uint8_t(OperatedState.Registers[R1]));
                            break;
                        }
                        // GETCHAR - получение символа.
                        case 106:
                        {
                            OperatedState.Registers[R1] = int32_t(getchar());
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

                // ЦЕЛОЧИСЛЕННАЯ АРИФМЕТИКА.
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
                    if (R1 + 1 >= State::RegistersNumber) { throw(OperationException::INVALIDREG); }

                    int64_t Product = int64_t(OperatedState.Registers[R1]) * int64_t(OperatedState.Registers[R2] + Imm16);
                    OperatedState.Registers[R1] = int32_t(Product & UINT32_MAX);
                    OperatedState.Registers[R1 + 1] = int32_t((Product >> State::BitsInWord) & UINT32_MAX);
                    break;
                }

                // MULI - произведение регистра на непосредственный операнд.
                case MULI:
                {
                    // Результат умножения приведёт к выходу за пределы существующих регистров.
                    if (R1 + 1 >= State::RegistersNumber) { throw(OperationException::INVALIDREG); }

                    int64_t Product = int64_t(OperatedState.Registers[R1]) * int64_t(Imm20);
                    OperatedState.Registers[R1] = int32_t(Product & UINT32_MAX);
                    OperatedState.Registers[R1 + 1] = int32_t((Product >> State::BitsInWord) & UINT32_MAX);
                    break;
                }

                // DIV - частное и остаток от деления пары регистров на регистр.
                case DIV:
                {
                    // Результат деления приведёт к выходу за пределы существующих регистров.
                    if (R1 + 1 >= State::RegistersNumber) { throw(OperationException::INVALIDREG); }
                    // Происходит деление на ноль.
                    if (!OperatedState.Registers[R2]) { throw(OperationException::DIVBYZERO); }

                    int64_t Divident = int64_t(OperatedState.Registers[R1] | (int64_t(OperatedState.Registers[R1 + 1]) << State::BitsInWord));
                    int64_t Divider = int64_t(OperatedState.Registers[R2]);
                    int64_t Product = Divident / Divider;

                    // Результат деления не помещается в регистр. По спецификации - деление на ноль.
                    if (Product > UINT32_MAX) { throw(OperationException::DIVBYZERO); }

                    int64_t Remainder = Divident % Divider;

                    OperatedState.Registers[R1] = int32_t(Product & UINT32_MAX);
                    OperatedState.Registers[R1 + 1] = int32_t(Remainder & UINT32_MAX);
                    break;
                }

                // DIVI - частное и остаток от деления пары регистров на непосредственный операнд.
                case DIVI:
                {
                    // Результат деления приведёт к выходу за пределы существующих регистров.
                    if (R1 + 1 >= State::RegistersNumber) { throw(OperationException::INVALIDREG); }
                    // Происходит деление на ноль.
                    if (!Imm20) { throw(OperationException::DIVBYZERO); }

                    int64_t Divident = int64_t(OperatedState.Registers[R1] | (int64_t(OperatedState.Registers[R1 + 1]) << State::BitsInWord));
                    int64_t Divider = int64_t(Imm20);
                    int64_t Product = Divident / Divider;

                    // Результат деления не помещается в регистр. По спецификации - деление на ноль.
                    if (Product > UINT32_MAX) { throw(OperationException::DIVBYZERO); }

                    int64_t Remainder = Divident % Divider;

                    OperatedState.Registers[R1] = int32_t(Product & UINT32_MAX);
                    OperatedState.Registers[R1 + 1] = int32_t(Remainder & UINT32_MAX);
                    break;
                }

                // КОПИРОВАНИЕ В РЕГИСТРЫ.
                // LC - загрузка константы в регистр.
                case LC:
                {
                    OperatedState.Registers[R1] = Imm20;
                    break;
                }

                // MOV - пересылка из одного регистра в другой.
                case MOV:
                {
                    OperatedState.Registers[R1] = OperatedState.Registers[R2] + Imm16;
                    break;
                }

                // СДВИГИ.
                // SHL - сдвиг влево на занчение регистра.
                case SHL:
                {
                    OperatedState.Registers[R1] <<= OperatedState.Registers[R2] + Imm16;
                    break;
                }

                // SHLI - сдвиг влево на непосредственный операнд.
                case SHLI:
                {
                    OperatedState.Registers[R1] <<= Imm20;
                    break;
                }

                // SHR - сдвиг вправо на занчение регистра.
                case SHR:
                {
                    OperatedState.Registers[R1] >>= OperatedState.Registers[R2] + Imm16;
                    break;
                }

                // SHRI - сдвиг вправо на непосредственный операнд.
                case SHRI:
                {
                    OperatedState.Registers[R1] >>= Imm20;
                    break;
                }

                // ЛОГИЕСКИЕ ОПЕРАЦИИ.
                // AND - побитовое И между регистрами.
                case AND:
                {
                    OperatedState.Registers[R1] &= OperatedState.Registers[R2] + Imm16;
                    break;
                }

                // ANDI - побитовое И между регистром и непосредственным операндом.
                case ANDI:
                {
                    OperatedState.Registers[R1] &= Imm20;
                    break;
                }

                // OR - побитовое ИЛИ между регистрами.
                case OR:
                {
                    OperatedState.Registers[R1] |= OperatedState.Registers[R2] + Imm16;
                    break;
                }

                // ORI - побитовое ИЛИ между регистром и непосредственным операндом.
                case ORI:
                {
                    OperatedState.Registers[R1] |= Imm20;
                    break;
                }

                // XOR - побитовое ИСКЛЮЧАЮЩЕЕ ИЛИ между регистрами.
                case XOR:
                {
                    OperatedState.Registers[R1] ^= OperatedState.Registers[R2] + Imm16;
                    break;
                }

                // XORI - побитовое ИСКЛЮЧАЮЩЕЕ ИЛИ между регистром и непосредственным операндом.
                case XORI:
                {
                    OperatedState.Registers[R1] ^= Imm20;
                    break;
                }

                // NOT - побитовое НЕ.
                case NOT:
                {
                    OperatedState.Registers[R1] = ~(OperatedState.Registers[R1]);
                    break;
                }

                // ВЕЩЕСТВЕННАЯ АРИФМЕТИКА.
                // ADDD - сложение двух вещественных чисел.
                case ADDD:
                {
                    // Результат выполнения команды приведёт к выходу за пределы существующих регистров.
                    if ((R1 + 1 >= State::RegistersNumber) || (R2 + 1 >= State::RegistersNumber)) { throw(OperationException::INVALIDREG); }

                    *(double*)(OperatedState.Registers + R1) += *(double*)(OperatedState.Registers + R2);
                    break;
                }

                // SUBD - разность двух вещественных чисел.
                case SUBD:
                {
                    // Результат выполнения команды приведёт к выходу за пределы существующих регистров.
                    if ((R1 + 1 >= State::RegistersNumber) || (R2 + 1 >= State::RegistersNumber)) { throw(OperationException::INVALIDREG); }

                    *(double*)(OperatedState.Registers + R1) -= *(double*)(OperatedState.Registers + R2);
                    break;
                }

                // MULD - произведение двух вещественных чисел.
                case MULD:
                {
                    // Результат выполнения команды приведёт к выходу за пределы существующих регистров.
                    if ((R1 + 1 >= State::RegistersNumber) || (R2 + 1 >= State::RegistersNumber)) { throw(OperationException::INVALIDREG); }

                    *(double*)(OperatedState.Registers + R1) *= *(double*)(OperatedState.Registers + R2);
                    break;
                }

                // DIVD - частное от деления двух вещественных чисел.
                case DIVD:
                {
                    // Результат выполнения команды приведёт к выходу за пределы существующих регистров.
                    if ((R1 + 1 >= State::RegistersNumber) || (R2 + 1 >= State::RegistersNumber)) { throw(OperationException::INVALIDREG); }

                    *(double*)(OperatedState.Registers + R1) /= *(double*)(OperatedState.Registers + R2);
                    break;
                }

                // ITOD - преобразование целого числа в вещественное.
                case ITOD:
                {
                    // Результат выполнения команды приведёт к выходу за пределы существующих регистров.
                    if (R1 + 1 >= State::RegistersNumber) { throw(OperationException::INVALIDREG); }

                    *(double*)(OperatedState.Registers + R1) = (double)(OperatedState.Registers[R2]);
                    break;
                }

                // DTOI - преобразование целого числа в вещественное.
                case DTOI:
                {
                    // Результат выполнения команды приведёт к выходу за пределы существующих регистров.
                    if (R2 + 1 >= State::RegistersNumber) { throw(OperationException::INVALIDREG); }

                    // Требуется вызвать исключение, если значение вещественного числа не помещается в регистр.
                    if ( (*(double*)(OperatedState.Registers + R2) > (double)(INT32_MAX)) ||
                         (*(double*)(OperatedState.Registers + R2) < (double)(-INT32_MAX)) )
                    { throw(OperationException::REGOVERFLOW); }

                    OperatedState.Registers[R1] = int32_t(*(double*)(OperatedState.Registers + R2));
                    break;
                }

                // СРАВНЕНИЕ.
                // CMP - сравнение двух регистров.
                case CMP:
                {
                    // Сброс флагов.
                    OperatedState.Flags &= ~(State::FlagsBits::EQUALITY);
                    OperatedState.Flags &= ~(State::FlagsBits::MAJORITY);
                    // Судя по дизассемблеру, эти две строки при текущем выборе положения бит соптимизируется в эту: OperatedState.Flags &= ~(0b11);

                    // Установка флагов.
                    OperatedState.Flags |= (OperatedState.Registers[R1] == OperatedState.Registers[R2]) << State::FlagsBits::EQUALITY_POS;
                    OperatedState.Flags |= (OperatedState.Registers[R1] <  OperatedState.Registers[R2]) << State::FlagsBits::MAJORITY_POS;
                    break;
                }

                // CMPI - сравнение регистра и константы.
                case CMPI:
                {
                    // Сброс флагов.
                    OperatedState.Flags &= ~(State::FlagsBits::EQUALITY);
                    OperatedState.Flags &= ~(State::FlagsBits::MAJORITY);

                    // Установка флагов.
                    OperatedState.Flags |= (OperatedState.Registers[R1] == Imm20) << State::FlagsBits::EQUALITY_POS;
                    OperatedState.Flags |= (OperatedState.Registers[R1] <  Imm20) << State::FlagsBits::MAJORITY_POS;
                    break;
                }

                // СТЕК.
                // PUSH - помещение значения регистра в стек.
                case PUSH:
                {
                    --OperatedState.Registers[14];
                    OperatedState.setWord(OperatedState.Registers[R1] + Imm20, OperatedState.Registers[14]);
                    break;
                }

                // POP - извлечение значения из стека.
                case POP:
                {
                    OperatedState.Registers[R1] = OperatedState.getWord(OperatedState.Registers[14]) + Imm20;
                    ++OperatedState.Registers[14];
                    break;
                }

                // ФУНКЦИИ.
                // CALL - вызвать функцию по адресу из регистра.
                case CALL:
                {
                    // Запоминаем адрес следубщей команды.
                    --OperatedState.Registers[14];
                    OperatedState.setWord(OperatedState.Registers[15] + 1, OperatedState.Registers[14]);

                    // Передаём управление.
                    OperatedState.Registers[15] = OperatedState.Registers[R1] + Imm20 - 1; // "-1" - костыль, связанный с тем, что после выполнения любой команды (даже CALL) R15 увеличивается на 1.
                    break;
                }

                // CALL - вызвать функцию по адресу из непосредственного операнда.
                case CALLI:
                {
                    // Запоминаем адрес следубщей команды.
                    --OperatedState.Registers[14];
                    OperatedState.setWord(OperatedState.Registers[15] + 1, OperatedState.Registers[14]);

                    // Передаём управление.
                    OperatedState.Registers[15] = Imm20 - 1;
                    break;
                }

                // RET - возврат из функции.
                case RET:
                {
                    // Получаем адрес возврата.
                    OperatedState.Registers[15] = OperatedState.getWord(OperatedState.Registers[14]) - 1;
                    ++OperatedState.Registers[14];

                    // Убираем из стека аргументы функции.
                    OperatedState.Registers[14] += Imm20;
                    break;
                }

                // ПЕРЕХОДЫ.
                // JMP - безусловный переход.
                case JMP:
                {
                    OperatedState.Registers[15] = Imm20 - 1; // "-1" - костыль, связанный с тем, что после выполнения любой команды (даже JMP) R15 увеличивается на 1.
                    break;
                }

                // JNE - переход при флаге неравенства (!=).
                case JNE:
                {
                    if (!(OperatedState.Flags & State::FlagsBits::EQUALITY)) { OperatedState.Registers[15] = Imm20 - 1; }
                    break;
                }

                // JEQ - переход при флаге равенства (==).
                case JEQ:
                {
                    if (OperatedState.Flags & State::FlagsBits::EQUALITY) { OperatedState.Registers[15] = Imm20 - 1; }
                    break;
                }

                // JLE - переход при флаге "левый операнд меньше либо равен правому" (<=).
                case JLE:
                {
                    if ((OperatedState.Flags & State::FlagsBits::MAJORITY) || (OperatedState.Flags & State::FlagsBits::EQUALITY)) { OperatedState.Registers[15] = Imm20 - 1; }
                    break;
                }

                // JL - переход при флаге "левый операнд меньше правого" (<).
                case JL:
                {
                    if ((OperatedState.Flags & State::FlagsBits::MAJORITY) && !(OperatedState.Flags & State::FlagsBits::MAJORITY)){ OperatedState.Registers[15] = Imm20 - 1; }
                    break;
                }

                // JGE - переход при флаге "левый операнд больше либо равен правому" (>=).
                case JGE:
                {
                    if (!(OperatedState.Flags & State::FlagsBits::MAJORITY) || (OperatedState.Flags & State::FlagsBits::EQUALITY)) { OperatedState.Registers[15] = Imm20 - 1; }
                    break;
                }

                // JG - переход при флаге "левый операнд больше правого" (>).
                case JG:
                {
                    if (!(OperatedState.Flags & State::FlagsBits::MAJORITY) && !(OperatedState.Flags & State::FlagsBits::EQUALITY)) { OperatedState.Registers[15] = Imm20 - 1; }
                    break;
                }

                // РАБОТА С ПАМЯТЬЮ.
                // LOAD - загрузка значения из памяти по указанному непосредственно адресу в регистр.
                case LOAD:
                {
                    OperatedState.Registers[R1] = OperatedState.getWord(Imm20);
                    break;
                }

                // STORE - выгрузка значения из регистра в память по указанному непосредственно адресу.
                case STORE:
                {
                    OperatedState.setWord(OperatedState.Registers[R1], Imm20);
                    break;
                }

                // LOAD2 - загрузка значения из памяти по указанному непосредственно адресу в пару регистров.
                case LOAD2:
                {
                    // Результат выполнения команды приведёт к выходу за пределы существующих регистров.
                    if (R1 + 1 >= State::RegistersNumber) { throw(OperationException::INVALIDREG); }

                    OperatedState.Registers[R1] = OperatedState.getWord(Imm20);
                    OperatedState.Registers[R1 + 1] = OperatedState.getWord(Imm20 + 1);
                    break;
                }

                // STORE2 - выгрузка значения из пары регистров в память по указанному непосредственно адресу.
                case STORE2:
                {
                    // Результат выполнения команды приведёт к выходу за пределы существующих регистров.
                    if (R1 + 1 >= State::RegistersNumber) { throw(OperationException::INVALIDREG); }

                    OperatedState.setWord(OperatedState.Registers[R1], Imm20);
                    OperatedState.setWord(OperatedState.Registers[R1 + 1], Imm20 + 1);
                    break;
                }

                // LOADR - загрузка значения из памяти по указанному во втором регистре адресу в первый регистр.
                case LOADR:
                {
                    try { OperatedState.Registers[R1] = OperatedState.getWord(OperatedState.Registers[R2] + Imm16); }
                    catch(State::Exception Exception) { throw(OperationException::INVALIDMEM); }
                    break;
                }

                // STORER - выгрузка значения из регистра в память по указанному во втором регистре адресу.
                case STORER:
                {
                    try { OperatedState.setWord(OperatedState.Registers[R1], OperatedState.Registers[R2] + Imm16); }
                    catch(State::Exception Exception) { throw(OperationException::INVALIDMEM); }
                    break;
                }

                // LOADR2 - загрузка значения из памяти по указанному во втором регистре адресу в пару регистров.
                case LOADR2:
                {
                    // Результат выполнения команды приведёт к выходу за пределы существующих регистров.
                    if (R1 + 1 >= State::RegistersNumber) { throw(OperationException::INVALIDREG); }

                    try
                    {
                        OperatedState.Registers[R1] = OperatedState.getWord(OperatedState.Registers[R2] + Imm16);
                        OperatedState.Registers[R1 + 1] = OperatedState.getWord(OperatedState.Registers[R2] + Imm16 + 1);
                    }
                    catch(State::Exception Exception) { throw(OperationException::INVALIDMEM); }
                    break;
                }

                // STORER2 - выгрузка значения из пары регистров в память по указанному во втором регистре адресу.
                case STORER2:
                {
                    // Результат выполнения команды приведёт к выходу за пределы существующих регистров.
                    if (R1 + 1 >= State::RegistersNumber) { throw(OperationException::INVALIDREG); }

                    try
                    {
                        OperatedState.setWord(OperatedState.Registers[R1], OperatedState.Registers[R2] + Imm16);
                        OperatedState.setWord(OperatedState.Registers[R1 + 1], OperatedState.Registers[R2] + Imm16 + 1);
                    }
                    catch(State::Exception Exception) { throw(OperationException::INVALIDMEM); }
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
                case OperationException::INVALIDMEM:
                {
                    std::cerr << "[EXECUTION ERROR]: access to an invalid address." << std::endl;
                    throw(Exception::INVALIDSTATE);
                    break;
                }
                case OperationException::DIVBYZERO:
                {
                    std::cerr << "[EXECUTION ERROR]: division by zero." << std::endl;
                    throw(Exception::MACHINE);
                    break;
                }
                case OperationException::REGOVERFLOW:
                {
                    std::cerr << "[EXECUTION ERROR]: register overflow." << std::endl;
                    throw(Exception::MACHINE);
                    break;
                }
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
        std::vector<std::tuple<std::string, OPERATION_CODE, OPERATION_TYPE>> TranslationTable =
        {
            // Системное.
            {"halt",    HALT,    RI},
            {"syscall", SYSCALL, RI},

            // Целочисленная арифметика.
            {"add",     ADD,     RR},
            {"addi",    ADDI,    RI},
            {"sub",     SUB,     RR},
            {"subi",    SUBI,    RI},
            {"mul",     MUL,     RR},
            {"muli",    MULI,    RI},
            {"div",     DIV,     RR},
            {"divi",    DIVI,    RI},

            // Копирование в регистры.
            {"lc",      LC,      RI},
            {"mov",     MOV,     RR},

            // Сдвиги.
            {"shl",     SHL,     RR},
            {"shli",    SHLI,    RI},
            {"shr",     SHR,     RR},
            {"shri",    SHRI,    RI},

            // Логические операции.
            {"and",     AND,     RR},
            {"andi",    ANDI,    RI},
            {"or",      OR,      RR},
            {"ori",     ORI,     RI},
            {"xor",     XOR,     RR},
            {"xori",    XORI,    RI},
            {"not",     NOT,     RI},

            // Вещественная арифметика.
            {"addd",    ADDD,    RR},
            {"subd",    SUBD,    RR},
            {"muld",    MULD,    RR},
            {"divd",    DIVD,    RR},
            {"itod",    ITOD,    RR},
            {"dtoi",    DTOI,    RR},

            // Стек.
            {"push",    PUSH,    RI},
            {"pop",     POP,     RI},

            // Функции
            {"call",    CALL,    RM},
            {"calli",   CALLI,   Me},
            {"ret",     RET,     Im},

            // Сравнение.
            {"cmp",     CMP,     RR},
            {"cmpi",    CMPI,    RI},

            // Переходы.
            {"jmp",     JMP,     Me},
            {"jne",     JNE,     Me},
            {"jeq",     JEQ,     Me},
            {"jle",     JLE,     Me},
            {"jl",      JL,      Me},
            {"jge",     JGE,     Me},
            {"jg",      JG,      Me},

            // Работа с памятью.
            {"load",    LOAD,    RM},
            {"store",   STORE,   RM},
            {"load2",   LOAD2,   RM},
            {"store2",  STORE2,  RM},
            {"loadr",   LOADR,   RR},
            {"storer",  STORER,  RR},
            {"loadr2",  LOADR2,  RR},
            {"storer2", STORER2, RR}
        };

        // Заполнение отображений.
        for (size_t i = 0; i < TranslationTable.size(); ++i)
        {
            // Ассемблирование.
            OpCode.insert({ std::get<0>(TranslationTable[i]), std::get<1>(TranslationTable[i]) });
            OpType.insert({ std::get<0>(TranslationTable[i]), std::get<2>(TranslationTable[i]) });

            // Дизассемблирование.
            CodeOp.insert({ std::get<1>(TranslationTable[i]), std::get<0>(TranslationTable[i]) });
            CodeType.insert({ std::get<1>(TranslationTable[i]), std::get<2>(TranslationTable[i]) });
        }

        // Таблица, связывающая имя регистра и его код.
        std::vector<std::tuple<std::string, int>> RegistersTable =
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

        // Заполнение отображений.
        for (size_t i = 0; i < RegistersTable.size(); ++i)
        {
            // Ассемблирование.
            RegCode.insert({ std::get<0>(RegistersTable[i]), std::get<1>(RegistersTable[i]) });

            // Дизассемблирование.
            CodeReg.insert({ std::get<1>(RegistersTable[i]), std::get<0>(RegistersTable[i]) });
        }
    }
    Translator::~Translator()
    {
        // ...
    }

    int Translator::Assemble(std::istream &InputStream, FUPM2EMU::State &OperatedState) const
    {
        size_t WriteAddress = 0; // Адрес текущего записываемого слова в OperatedState.
        std::string Input;       // Строка для считывания слов из входного файла.
        std::map<std::string, size_t> MarksAddresses;          // Адреса объявленных меток (название - адрес).
        std::vector<std::pair<size_t, std::string>> UsedMarks; // Адреса команд, использовавших метки, и имена этих меток.

        enum class ParserState
        {
            IN_CODE    = 0,
            IN_COMMENT = 1,
        };
        ParserState CurrentState = ParserState::IN_CODE;

        try
        {
            // Чтение идёт до конца файла.
            while(!InputStream.eof())
            {
                switch(CurrentState)
                {
                    // В коде.
                    case ParserState::IN_CODE:
                    {
                        // Считываем строку-слово.
                        if (!(InputStream >> Input)) { break; } // Проверка на успешность чтения.
                        if (!Input.size()) { break; }          // Проверка строки на пустоту.

                        #ifdef DEBUG_OUTPUT_ASSEMBLING
                        std::cout << "Command/mark:" << Input << std::endl;
                        #endif

                        // Начинается на ";" - входим в состояние "в комментарии" (до новой строки)
                        if (Input[0] == ';')
                        {
                            CurrentState = ParserState::IN_COMMENT;
                            break;
                        }

                        // Если слово оканчивается на ':', оно является меткой.
                        if (Input[Input.size() - 1] == ':')
                        {
                            Input.pop_back(); // Удаляем последний символ - ':'.
                            MarksAddresses.insert({ Input, WriteAddress });
                            break;
                        }

                        // Если встретилась директива "word", просто оставляем слово по текущему адресу свободным.
                        if (Input == "word")
                        {
                            ++WriteAddress;
                            break;
                        }

                        // Если встретилась директива "end", запоминаем метку старта программы. Так как эта директива обязана быть в конце программы,
                        // к моменту её чтения метка уже точно должна существовать. Тогда можно сразу проинициализировать нужным значением регистр R15.
                        if (Input == "end")
                        {
                            if(!(InputStream >> Input)) { throw(AssemblingException(WriteAddress, AssemblingException::Code::MARK_EXPECTED)); } // Чтение имени метки.
                            try { OperatedState.Registers[15] = MarksAddresses.at(Input); }
                            catch (std::out_of_range) { throw(AssemblingException(WriteAddress, AssemblingException::Code::UNDECLARED_MARK)); }
                            break;
                        }


                        // К этому моменту уже точно известно, что считанное слово должно быть именем операции. Тогда начинаем разбирать её и её аргументы.
                        uint32_t Command = 0; // Создаём переменную для записываемого слова команды.

                        // Парсим операцию.
                        try {Command |= OpCode.at(Input) << (BitsInCommand - BitsInOpCode); }
                        catch (std::out_of_range) { throw(AssemblingException(WriteAddress, AssemblingException::Code::OP_CODE)); }

                        // Парсим аргументы.
                        switch(OpType.at(Input))
                        {
                            // Регистр и непосредственный операнд.
                            case RI:
                            {
                                // Регистр.
                                if(!(InputStream >> Input)) { throw(AssemblingException(WriteAddress, AssemblingException::Code::REG_EXPECTED)); }
                                try { Command |= RegCode.at(Input) << (BitsInCommand - BitsInOpCode - BitsInRegCode); }
                                catch (std::out_of_range) { throw(AssemblingException(WriteAddress, AssemblingException::Code::REG_CODE)); }

                                // Непосредственный операнд.
                                if(!(InputStream >> Input)) { throw(AssemblingException(WriteAddress, AssemblingException::Code::IMM_EXPECTED)); }

                                // Проверка, число это, или метка. Если число, сразу подставляем значение, если метка - запоминаем адрес команды для последующей подстановки адреса метки.
                                if (Input.find_first_not_of("0123456789") == std::string::npos)
                                {
                                    try { Command |= (std::stoi(Input) & 0xFFFFF); }
                                    catch (std::out_of_range) { throw(AssemblingException::Code::BIG_IMM); };
                                }
                                else { UsedMarks.push_back(std::pair<size_t, std::string>(WriteAddress, Input)); }
                                break;
                            }

                            // Два регистра и короткий непосредственный операнд.
                            case RR:
                            {
                                // Первый регистр.
                                if(!(InputStream >> Input)) { throw(AssemblingException(WriteAddress, AssemblingException::Code::REG_EXPECTED)); }
                                try { Command |= RegCode.at(Input) << (BitsInCommand - BitsInOpCode - BitsInRegCode); }
                                catch (std::out_of_range) { throw(AssemblingException(WriteAddress, AssemblingException::Code::REG_CODE)); }

                                // Второй регистр.
                                if(!(InputStream >> Input)) { throw(AssemblingException(WriteAddress, AssemblingException::Code::REG_EXPECTED)); }
                                try { Command |= RegCode.at(Input) << (BitsInCommand - BitsInOpCode - BitsInRegCode - BitsInRegCode); }
                                catch (std::out_of_range) { throw(AssemblingException(WriteAddress, AssemblingException::Code::REG_CODE)); }

                                // Непосредственный операнд.
                                if(!(InputStream >> Input)) { throw(AssemblingException(WriteAddress, AssemblingException::Code::IMM_EXPECTED)); }
                                // Перевод ввода в число и запись в конец слова.
                                try { Command |= (std::stoi(Input) & 0x0FFFF); } // 16 бит на короткий Imm.
                                catch (std::out_of_range) { throw(AssemblingException::Code::BIG_IMM); };
                                break;
                            }

                            // Регистр и адрес.
                            case RM:
                            {
                                // Регистр.
                                if (!(InputStream >> Input)) { throw(AssemblingException(WriteAddress, AssemblingException::Code::REG_EXPECTED)); }
                                try { Command |= RegCode.at(Input) << (BitsInCommand - BitsInOpCode - BitsInRegCode); }
                                catch (std::out_of_range) { throw(AssemblingException(WriteAddress, AssemblingException::Code::REG_CODE)); }

                                // Непосредственный операнд - адрес.
                                if (!(InputStream >> Input)) { throw(AssemblingException(WriteAddress, AssemblingException::Code::ADDR_EXPECTED)); }

                                // Проверка, число это, или метка. Если число, сразу подставляем адрес, если метка - запоминаем адрес команды для последующей подстановки адреса метки.
                                if (Input.find_first_not_of("0123456789") == std::string::npos)
                                {
                                    try { Command |= (std::stoi(Input) & 0xFFFFF); }
                                    catch (std::out_of_range) { throw(AssemblingException::Code::BIG_ADDR); };
                                }
                                else { UsedMarks.push_back(std::pair<size_t, std::string>(WriteAddress, Input)); }
                                break;
                            }

                            // Адрес.
                            case Me:
                            {
                                // Непосредственный операнд - адрес.
                                if (!(InputStream >> Input)) { throw(AssemblingException(WriteAddress, AssemblingException::Code::ADDR_EXPECTED)); }

                                // Проверка, число это, исли метка. Если число, сразу подставляем адрес, если метка - запоминаем адрес команды для последующей подстановки адреса метки.
                                if (Input.find_first_not_of("0123456789") == std::string::npos)
                                {
                                    try { Command |= (std::stoi(Input) & 0xFFFFF); }
                                    catch (std::out_of_range) { throw(AssemblingException::Code::BIG_ADDR); };
                                }
                                else { UsedMarks.push_back(std::pair<size_t, std::string>(WriteAddress, Input)); }
                                break;
                            }

                            // Непосредственный операнд.
                            case Im:
                            {
                                // Непосредственный операнд - число.
                                if (!(InputStream >> Input)) { throw(AssemblingException(WriteAddress, AssemblingException::Code::IMM_EXPECTED)); }

                                // Проверка, число это, исли метка. Если число, сразу подставляем значение, если метка - запоминаем адрес команды для последующей подстановки адреса метки.
                                if (Input.find_first_not_of("0123456789") == std::string::npos)
                                {
                                    try { Command |= (std::stoi(Input) & 0xFFFFF); }
                                    catch (std::out_of_range) { throw(AssemblingException::Code::BIG_IMM); };
                                }
                                else { UsedMarks.push_back(std::pair<size_t, std::string>(WriteAddress, Input)); }
                                break;
                            }
                        }

                        // Запись слова.
                        OperatedState.setWord(Command, WriteAddress);
                        ++WriteAddress;
                        break;
                    }

                    // В комментарии.
                    case ParserState::IN_COMMENT:
                    {
                        InputStream.ignore (std::numeric_limits<std::streamsize>::max(), '\n'); // Пропускаем ввод до первого символа новой строки.
                        CurrentState = ParserState::IN_CODE;
                        break;
                    }
                }
            }

            // Теперь проходим по всем использованным меткам и подставляем адреса.
            for(size_t i = 0; i < UsedMarks.size(); ++i)
            {
                // Читаем слово-команду, вставляем адрес и записываем.
                uint32_t Word = OperatedState.getWord(UsedMarks[i].first);
                try { Word |= MarksAddresses.at(UsedMarks[i].second); }
                catch (std::out_of_range) { throw(AssemblingException(UsedMarks[i].first, AssemblingException::Code::UNDECLARED_MARK)); }
                OperatedState.setWord(Word, UsedMarks[i].first);
            }
        }
        catch(AssemblingException Exception)
        {
            std::cerr << "[TRANSLATOR ERROR]: error assembling command " << Exception.address + 1 << "." << std::endl;
            switch(Exception.code)
            {
                case AssemblingException::Code::OK: { break; }
                case AssemblingException::Code::OP_CODE:
                {
                    std::cerr << "Unknown operation code." << std::endl;
                    break;
                }
                case AssemblingException::Code::REG_CODE:
                {
                    std::cerr << "Unknown register name." << std::endl;
                    break;
                }
                case AssemblingException::Code::REG_EXPECTED:
                {
                    std::cerr << "Register argument was expected but was not specified." << std::endl;
                    break;
                }
                case AssemblingException::Code::IMM_EXPECTED:
                {
                    std::cerr << "Immediate argument was expected but was not specified." << std::endl;
                    break;
                }
                case AssemblingException::Code::ADDR_EXPECTED:
                {
                    std::cerr << "Address was expected but was not specified." << std::endl;
                    break;
                }
                case AssemblingException::Code::MARK_EXPECTED:
                {
                    std::cerr << "Mark argument was expected but was not specified." << std::endl;
                    break;
                }
                case AssemblingException::Code::UNDECLARED_MARK:
                {
                    std::cerr << "Undeclared mark was used." << std::endl;
                    break;
                }
                case AssemblingException::Code::BIG_IMM:
                {
                    std::cerr << "Given immediate operand is too big." << std::endl;
                    break;
                }
                case AssemblingException::Code::BIG_ADDR:
                {
                    std::cerr << "Given address is too big." << std::endl;
                    break;
                }
            }

            throw(Exception::ASSEMBLING);
        }

        OperatedState.Registers[14] = State::MemorySize - 1; // Размещение стека в конце памяти.
        return(0);
    }

    int Translator::Disassemble(const State &OperatedState, std::ostream &OutputStream) const
    {
        size_t Address = 0; // Текущий адрес.
        uint32_t Word = 0;  // Считанное слово.

        // Проход по всей памяти.
        while (Address < State::MemorySize)
        {
            // Цикл вывода непустой части памяти.
            while (Address < State::MemorySize)
            {
                // Чтение слова по текущему адресу.
                Word = OperatedState.getWord(Address);

                // Код будет короче, если вычислить все возможные операнды сразу.
                OPERATION_CODE Operation = OPERATION_CODE((Word >> 24) & 0xFF);
                uint8_t R1 = (Word >> 20) & 0xF;
                uint8_t R2 = (Word >> 16) & 0xF;
                int32_t Imm16 = Word & 0x0FFFF;
                int32_t Imm20 = Word & 0xFFFFF;

                // Два варианта: либо считана команда, либо нет.
                auto Iterator = CodeOp.find(Operation);
                if (Iterator != CodeOp.end())
                {
                    // Вывод имени команды.
                    OutputStream << Iterator->second;

                    // Вывод аргументов.
                    switch (CodeType.at(Operation))
                    {
                        case RI:
                        {
                            OutputStream << " " << CodeReg.at(R1) << " " << Imm20;
                            break;
                        }
                        case RR:
                        {
                            OutputStream << " " << CodeReg.at(R1) << " " << CodeReg.at(R2) << " " << Imm16;
                            break;
                        }
                        case RM:
                        {
                            OutputStream << " " << CodeReg.at(R1) << " " << Imm20;
                            break;
                        }
                        case Me:
                        {
                            OutputStream << " " << Imm20;
                            break;
                        }
                        case Im:
                        {
                            OutputStream << " " << Imm20;
                            break;
                        }
                    }

                    // Конец вывода.
                    OutputStream << std::endl;
                }
                else
                {
                    // Вывод слова.
                    OutputStream << Word << std::endl;
                }

                ++Address;
                // Выход из цикла после первого пустого слова.
                if (!Word) { break; }
            }

            // Цикл пропуска пустой части памяти.
            while (Address < State::MemorySize)
            {
                // Чтение слова по текущему адресу.
                Word = OperatedState.getWord(Address);

                // Если слово вдруг оказалось непустым, выходим из цикла.
                if (Word) { break; }
                ++Address;
            }
        }

        return(0);
    }

    // PROTECTED:

    //////// ASSEMBLING EXCEPTION ////////
    Translator::AssemblingException::AssemblingException(size_t initAddress, Translator::AssemblingException::Code initCode) // Инициализация экземпляра исключения.
    {
        address = initAddress;
        code = initCode;
    }


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
        while (ReturnCode == Executor::ReturnCode::OK)
        {
            try
            {
                ReturnCode = executor.step(state);

                #ifdef DEBUG_EXECUTION_STEPS
                getchar();
                #endif
            }
            catch (Executor::Exception Exception)
            {
                switch (Exception)
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
                }
                std::cerr << "FUPM2EMU has encountered a critical error. Shutting down." << std::endl;
                break;
            }

            #ifdef DEBUG_OUTPUT_EXECUTION
            std::cout << "ReturnCode: " << int(ReturnCode) << std::endl;
            #endif
        }
        return(0);
    }

    // PROTECTED:

    // PRIVATE:
}
