#include <limits>
#include <iostream>
#include <cstdio>
#include <cstring>

#include "FUPM2EMU.hpp"

//#define DEBUG_OUTPUT_EXECUTION
//#define DEBUG_OUTPUT_LOADINGSTATE
//#define DEBUG_OUTPUT_ASSEMBLING
//#define DEBUG_EXECUTION_STEPS

namespace FUPM2EMU
{
    ////////////////      State      ///////////////
    // Настройки компиляции:
    #define MEMORY_MOD          // Модульная адресация.
    //#define MEMORY_EXCEPTIONS // Исключение при выходе за пределы адресного пространства.

    State::State()
    {
        // Заполнение нулями регистров.
        std::memset(registers, 0, registers_number * sizeof(uint32_t));

        // Обнуление регистра флагов.
        flags = 0;

        // Создание и заполнение нулями блока памяти.
        memory = std::vector<uint8_t>(memory_size * bytes_in_word, 0);
    }
    State::~State()
    {
        // ...
    }

    int State::load(std::istream& input_stream)
    {
        // Первые 16 * 4 + 1 байт - регистры + регистр флагов, остальное до конца файла - память.
        char bytes[bytes_in_word];

        // Регистры.
        for (size_t reg = 0; reg < registers_number; ++reg)
        {
            if (input_stream.read(bytes, bytes_in_word))
            {
                registers[reg] = (static_cast<uint32_t>(bytes[0]) << 24) |
                                 (static_cast<uint32_t>(bytes[1]) << 16) |
                                 (static_cast<uint32_t>(bytes[2]) << 8)  |
                                  static_cast<uint32_t>(bytes[3]);
                #ifdef DEBUG_OUTPUT_LOADINGSTATE
                std::cout << "R" << reg << ": " << registers[reg] << std::endl;
                #endif
            }
        }

        // Регистр флагов.
        if (input_stream.get(bytes[0])) { flags = uint8_t(bytes[0]); }
        #ifdef DEBUG_OUTPUT_LOADINGSTATE
        std::cout << "flags: " << static_cast<unsigned int>(flags) << std::endl;
        #endif

        // Память.
        size_t address = 0;
        while (input_stream.get(bytes[0]) && (address < (memory_size * bytes_in_word)))
        {
            memory[address] = static_cast<uint8_t>(bytes[0]);
            #ifdef DEBUG_OUTPUT_LOADINGSTATE
            std::cout << address << ": " << static_cast<unsigned int>(memory[address]) << std::endl;
            #endif
            ++address;
        }

        return 0;
    }


    inline uint32_t State::get_word(size_t address) const
    {
        // Адресация по модулю.
        #ifdef MEMORY_MOD
        address %= memory_size;
        #endif

        // Исключение при выходе за пределы адресного пространства.
        #ifdef MEMORY_EXCEPTIONS
        if (address > memory_size) { throw Exception::MEMORY; }
        #endif

        size_t byte_address = address * bytes_in_word;
        return (static_cast<uint32_t>(memory[byte_address]) << 24)     |
               (static_cast<uint32_t>(memory[byte_address + 1]) << 16) |
               (static_cast<uint32_t>(memory[byte_address + 2]) << 8)  |
                static_cast<uint32_t>(memory[byte_address + 3]);
    }
    inline void State::set_word(uint32_t value, size_t address)
    {
        // Адресация по модулю.
        #ifdef MEMORY_MOD
        address %= memory_size;
        #endif

        // Исключение при выходе за пределы адресного пространства.
        #ifdef MEMORY_EXCEPTIONS
        if (address > memory_size) { throw Exception::MEMORY; }
        #endif

        size_t byte_address = address * bytes_in_word;
        memory[byte_address + 3] = static_cast<uint8_t>(value & 0xFF); value >>= 8;
        memory[byte_address + 2] = static_cast<uint8_t>(value & 0xFF); value >>= 8;
        memory[byte_address + 1] = static_cast<uint8_t>(value & 0xFF); value >>= 8;
        memory[byte_address] =     static_cast<uint8_t>(value & 0xFF);
    }



    ////////////////    Executor    ////////////////
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
    inline Executor::ReturnCode Executor::step(State& state, std::istream& input_stream, std::ostream& output_stream)
    {
        // Извлечение следующией команды.
        uint32_t command = state.get_word(state.registers[State::CIR]);

        // Код будет короче, если вычислить все возможные операнды сразу.
        OPERATION_CODE operation = static_cast<OPERATION_CODE>((command >> 24) & 0xFF);
        uint8_t R1 = (command >> 20) & 0xF;
        uint8_t R2 = (command >> 16) & 0xF;
        int32_t imm16 = command & 0x0FFFF;
        int32_t imm20 = command & 0xFFFFF;

        ReturnCode return_code = ReturnCode::OK;

        #ifdef DEBUG_OUTPUT_EXECUTION
        std::cout << "OPCODE: " << operation << std::endl;
        std::cout << "registers:"
                  << " R" << static_cast<unsigned int>(R1) << ": " << state.registers[R1]
                  << " R" << static_cast<unsigned int>(R2) << ": " << state.registers[R2] << std::endl;
        std::cout << "Immediates: " << "imm16: " << imm16 << " imm20: " << imm20 << std::endl;
        #endif

        try
        {
            switch(operation)
            {
                // СИСТЕМНОЕ.
                // HALT - выключение процессора.
                case HALT:
                {
                    return_code = ReturnCode::TERMINATE;
                    break;
                }

                // SYSCALL - системный вызов.
                case SYSCALL:
                {
                    switch (imm20)
                    {
                        // EXIT - выход.
                        case 0:
                        {
                            return_code = ReturnCode::TERMINATE;
                            break;
                        }
                        // SCANINT - запрос целого числа.
                        case 100:
                        {
                            input_stream >> state.registers[R1];
                            break;
                        }
                        // SCANDOUBLE - запрос вещественного числа.
                        case 101:
                        {
                            // Результат выполнения команды приведёт к выходу за пределы существующих регистров.
                            if (R1 + 1 >= State::registers_number) { throw OperationException::INVALIDREG; }

                            double input = 0.0;
                            input_stream >> input;
                            *reinterpret_cast<double*>(state.registers + R1) = input;
                            break;
                        }
                        // PRINTINT - вывод целого числа.
                        case 102:
                        {
                            output_stream << state.registers[R1];
                            break;
                        }
                        // PRINTDOUBLE - вывод вещественного числа.
                        case 103:
                        {
                            // Результат выполнения команды приведёт к выходу за пределы существующих регистров.
                            if (R1 + 1 >= State::registers_number) { throw OperationException::INVALIDREG; }

                            output_stream << *reinterpret_cast<double*>(state.registers + R1);
                            break;
                        }
                        // PUTCHAR - вывод символа.
                        case 105:
                        {
                            output_stream.put(static_cast<uint8_t>(state.registers[R1]));
                            break;
                        }
                        // GETCHAR - получение символа.
                        case 106:
                        {
                            state.registers[R1] = static_cast<int32_t>(getchar());
                            break;
                        }
                        // Использован неспецифицированный код системного вызова.
                        default:
                        {
                            return_code = ReturnCode::ERROR;
                            break;
                        }
                    }
                    break;
                }

                // ЦЕЛОЧИСЛЕННАЯ АРИФМЕТИКА.
                // ADD - сложение регистров.
                case ADD:
                {
                    state.registers[R1] += state.registers[R2] + imm16;
                    break;
                }

                // ADDI - прибавление к регистру непосредственного операнда.
                case ADDI:
                {
                    state.registers[R1] += imm20;
                    break;
                }

                // SUB - разность регистров.
                case SUB:
                {
                    state.registers[R1] -= state.registers[R2] + imm16;
                    break;
                }

                // SUBI - вычитание из регистра непосредственного операнда.
                case SUBI:
                {
                    state.registers[R1] -= imm20;
                    break;
                }

                // MUL - произведение регистров.
                case MUL:
                {
                    // Результат умножения приведёт к выходу за пределы существующих регистров.
                    if (R1 + 1 >= State::registers_number) { throw OperationException::INVALIDREG; }

                    int64_t product = static_cast<int64_t>(state.registers[R1]) * static_cast<int64_t>(state.registers[R2] + imm16);
                    state.registers[R1] = int32_t(product & UINT32_MAX);
                    state.registers[R1 + 1] = static_cast<int32_t>((product >> State::bits_in_word) & UINT32_MAX);
                    break;
                }

                // MULI - произведение регистра на непосредственный операнд.
                case MULI:
                {
                    // Результат умножения приведёт к выходу за пределы существующих регистров.
                    if (R1 + 1 >= State::registers_number) { throw OperationException::INVALIDREG; }

                    int64_t product = static_cast<int64_t>(state.registers[R1]) * static_cast<int64_t>(imm20);
                    state.registers[R1] = static_cast<int32_t>(product & UINT32_MAX);
                    state.registers[R1 + 1] = static_cast<int32_t>((product >> State::bits_in_word) & UINT32_MAX);
                    break;
                }

                // DIV - частное и остаток от деления пары регистров на регистр.
                case DIV:
                {
                    // Результат деления приведёт к выходу за пределы существующих регистров.
                    if (R1 + 1 >= State::registers_number) { throw OperationException::INVALIDREG; }
                    // Происходит деление на ноль.
                    if (!state.registers[R2]) { throw OperationException::DIVBYZERO; }

                    int64_t divident = static_cast<int64_t>(state.registers[R1] | (static_cast<int64_t>(state.registers[R1 + 1]) << State::bits_in_word));
                    int64_t divider = static_cast<int64_t>(state.registers[R2]);
                    int64_t product = divident / divider;

                    // Результат деления не помещается в регистр. По спецификации - деление на ноль.
                    if (product > UINT32_MAX) { throw OperationException::DIVBYZERO; }

                    int64_t remainder = divident % divider;

                    state.registers[R1] = static_cast<int32_t>(product & UINT32_MAX);
                    state.registers[R1 + 1] = static_cast<int32_t>(remainder & UINT32_MAX);
                    break;
                }

                // DIVI - частное и остаток от деления пары регистров на непосредственный операнд.
                case DIVI:
                {
                    // Результат деления приведёт к выходу за пределы существующих регистров.
                    if (R1 + 1 >= State::registers_number) { throw OperationException::INVALIDREG; }
                    // Происходит деление на ноль.
                    if (!imm20) { throw OperationException::DIVBYZERO; }

                    int64_t divident = static_cast<int64_t>(state.registers[R1] | (static_cast<int64_t>(state.registers[R1 + 1]) << State::bits_in_word));
                    int64_t divider = static_cast<int64_t>(imm20);
                    int64_t product = divident / divider;

                    // Результат деления не помещается в регистр. По спецификации - деление на ноль.
                    if (product > UINT32_MAX) { throw OperationException::DIVBYZERO; }

                    int64_t remainder = divident % divider;

                    state.registers[R1] = static_cast<int32_t>(product & UINT32_MAX);
                    state.registers[R1 + 1] = static_cast<int32_t>(remainder & UINT32_MAX);
                    break;
                }

                // КОПИРОВАНИЕ В РЕГИСТРЫ.
                // LC - загрузка константы в регистр.
                case LC:
                {
                    state.registers[R1] = imm20;
                    break;
                }

                // MOV - пересылка из одного регистра в другой.
                case MOV:
                {
                    state.registers[R1] = state.registers[R2] + imm16;
                    break;
                }

                // СДВИГИ.
                // SHL - сдвиг влево на занчение регистра.
                case SHL:
                {
                    state.registers[R1] <<= state.registers[R2] + imm16;
                    break;
                }

                // SHLI - сдвиг влево на непосредственный операнд.
                case SHLI:
                {
                    state.registers[R1] <<= imm20;
                    break;
                }

                // SHR - сдвиг вправо на занчение регистра.
                case SHR:
                {
                    state.registers[R1] >>= state.registers[R2] + imm16;
                    break;
                }

                // SHRI - сдвиг вправо на непосредственный операнд.
                case SHRI:
                {
                    state.registers[R1] >>= imm20;
                    break;
                }

                // ЛОГИЕСКИЕ ОПЕРАЦИИ.
                // AND - побитовое И между регистрами.
                case AND:
                {
                    state.registers[R1] &= state.registers[R2] + imm16;
                    break;
                }

                // ANDI - побитовое И между регистром и непосредственным операндом.
                case ANDI:
                {
                    state.registers[R1] &= imm20;
                    break;
                }

                // OR - побитовое ИЛИ между регистрами.
                case OR:
                {
                    state.registers[R1] |= state.registers[R2] + imm16;
                    break;
                }

                // ORI - побитовое ИЛИ между регистром и непосредственным операндом.
                case ORI:
                {
                    state.registers[R1] |= imm20;
                    break;
                }

                // XOR - побитовое ИСКЛЮЧАЮЩЕЕ ИЛИ между регистрами.
                case XOR:
                {
                    state.registers[R1] ^= state.registers[R2] + imm16;
                    break;
                }

                // XORI - побитовое ИСКЛЮЧАЮЩЕЕ ИЛИ между регистром и непосредственным операндом.
                case XORI:
                {
                    state.registers[R1] ^= imm20;
                    break;
                }

                // NOT - побитовое НЕ.
                case NOT:
                {
                    state.registers[R1] = ~(state.registers[R1]);
                    break;
                }

                // ВЕЩЕСТВЕННАЯ АРИФМЕТИКА.
                // ADDD - сложение двух вещественных чисел.
                case ADDD:
                {
                    // Результат выполнения команды приведёт к выходу за пределы существующих регистров.
                    if ((R1 + 1 >= State::registers_number) || (R2 + 1 >= State::registers_number)) { throw OperationException::INVALIDREG; }

                    *reinterpret_cast<double*>(state.registers + R1) += *reinterpret_cast<double*>(state.registers + R2);
                    break;
                }

                // SUBD - разность двух вещественных чисел.
                case SUBD:
                {
                    // Результат выполнения команды приведёт к выходу за пределы существующих регистров.
                    if ((R1 + 1 >= State::registers_number) || (R2 + 1 >= State::registers_number)) { throw OperationException::INVALIDREG; }

                    *reinterpret_cast<double*>(state.registers + R1) -= *reinterpret_cast<double*>(state.registers + R2);
                    break;
                }

                // MULD - произведение двух вещественных чисел.
                case MULD:
                {
                    // Результат выполнения команды приведёт к выходу за пределы существующих регистров.
                    if ((R1 + 1 >= State::registers_number) || (R2 + 1 >= State::registers_number)) { throw OperationException::INVALIDREG; }

                    *reinterpret_cast<double*>(state.registers + R1) *= *reinterpret_cast<double*>(state.registers + R2);
                    break;
                }

                // DIVD - частное от деления двух вещественных чисел.
                case DIVD:
                {
                    // Результат выполнения команды приведёт к выходу за пределы существующих регистров.
                    if ((R1 + 1 >= State::registers_number) || (R2 + 1 >= State::registers_number)) { throw OperationException::INVALIDREG; }

                    *reinterpret_cast<double*>(state.registers + R1) /= *reinterpret_cast<double*>(state.registers + R2);
                    break;
                }

                // ITOD - преобразование целого числа в вещественное.
                case ITOD:
                {
                    // Результат выполнения команды приведёт к выходу за пределы существующих регистров.
                    if (R1 + 1 >= State::registers_number) { throw OperationException::INVALIDREG; }

                    *reinterpret_cast<double*>(state.registers + R1) = static_cast<double>(state.registers[R2]);
                    break;
                }

                // DTOI - преобразование целого числа в вещественное.
                case DTOI:
                {
                    // Результат выполнения команды приведёт к выходу за пределы существующих регистров.
                    if (R2 + 1 >= State::registers_number) { throw OperationException::INVALIDREG; }

                    // Требуется вызвать исключение, если значение вещественного числа не помещается в регистр.
                    if ( (*reinterpret_cast<double*>(state.registers + R2) > static_cast<double>(INT32_MAX)) ||
                         (*reinterpret_cast<double*>(state.registers + R2) < static_cast<double>(-INT32_MAX)) )
                    { throw OperationException::REGOVERFLOW; }

                    state.registers[R1] = static_cast<int32_t>(*reinterpret_cast<double*>(state.registers + R2));
                    break;
                }

                // СРАВНЕНИЕ.
                // CMP - сравнение двух регистров.
                case CMP:
                {
                    // Сброс флагов.
                    state.flags &= ~(State::FlagsBits::EQUALITY);
                    state.flags &= ~(State::FlagsBits::MAJORITY);
                    // Судя по дизассемблеру, эти две строки при текущем выборе положения бит соптимизируется в эту: state.flags &= ~(0b11);

                    // Установка флагов.
                    state.flags |= (state.registers[R1] == state.registers[R2]) << State::FlagsBits::EQUALITY_POS;
                    state.flags |= (state.registers[R1] <  state.registers[R2]) << State::FlagsBits::MAJORITY_POS;
                    break;
                }

                // CMPI - сравнение регистра и константы.
                case CMPI:
                {
                    // Сброс флагов.
                    state.flags &= ~(State::FlagsBits::EQUALITY);
                    state.flags &= ~(State::FlagsBits::MAJORITY);

                    // Установка флагов.
                    state.flags |= (state.registers[R1] == imm20) << State::FlagsBits::EQUALITY_POS;
                    state.flags |= (state.registers[R1] <  imm20) << State::FlagsBits::MAJORITY_POS;
                    break;
                }

                // СТЕК.
                // PUSH - помещение значения регистра в стек.
                case PUSH:
                {
                    --state.registers[State::SR];
                    state.set_word(state.registers[R1] + imm20, state.registers[State::SR]);
                    break;
                }

                // POP - извлечение значения из стека.
                case POP:
                {
                    state.registers[R1] = state.get_word(state.registers[State::SR]) + imm20;
                    ++state.registers[State::SR];
                    break;
                }

                // ФУНКЦИИ.
                // CALL - вызвать функцию по адресу из регистра.
                case CALL:
                {
                    // Запоминаем адрес следубщей команды.
                    --state.registers[State::SR];
                    state.set_word(state.registers[State::CIR] + 1, state.registers[State::SR]);

                    // Передаём управление.
                    state.registers[State::CIR] = state.registers[R1] + imm20 - 1; // "-1" - костыль, связанный с тем, что после выполнения любой команды (даже CALL) R15 увеличивается на 1.
                    break;
                }

                // CALL - вызвать функцию по адресу из непосредственного операнда.
                case CALLI:
                {
                    // Запоминаем адрес следубщей команды.
                    --state.registers[State::SR];
                    state.set_word(state.registers[State::CIR] + 1, state.registers[State::SR]);

                    // Передаём управление.
                    state.registers[State::CIR] = imm20 - 1;
                    break;
                }

                // RET - возврат из функции.
                case RET:
                {
                    // Получаем адрес возврата.
                    state.registers[State::CIR] = state.get_word(state.registers[State::SR]) - 1;
                    ++state.registers[State::SR];

                    // Убираем из стека аргументы функции.
                    state.registers[State::SR] += imm20;
                    break;
                }

                // ПЕРЕХОДЫ.
                // JMP - безусловный переход.
                case JMP:
                {
                    state.registers[State::CIR] = imm20 - 1; // "-1" - костыль, связанный с тем, что после выполнения любой команды (даже JMP) R15 увеличивается на 1.
                    break;
                }

                // JNE - переход при флаге неравенства (!=).
                case JNE:
                {
                    if (!(state.flags & State::FlagsBits::EQUALITY)) { state.registers[State::CIR] = imm20 - 1; }
                    break;
                }

                // JEQ - переход при флаге равенства (==).
                case JEQ:
                {
                    if (state.flags & State::FlagsBits::EQUALITY) { state.registers[State::CIR] = imm20 - 1; }
                    break;
                }

                // JLE - переход при флаге "левый операнд меньше либо равен правому" (<=).
                case JLE:
                {
                    if ((state.flags & State::FlagsBits::MAJORITY) || (state.flags & State::FlagsBits::EQUALITY)) { state.registers[State::CIR] = imm20 - 1; }
                    break;
                }

                // JL - переход при флаге "левый операнд меньше правого" (<).
                case JL:
                {
                    if ((state.flags & State::FlagsBits::MAJORITY) && !(state.flags & State::FlagsBits::MAJORITY)){ state.registers[State::CIR] = imm20 - 1; }
                    break;
                }

                // JGE - переход при флаге "левый операнд больше либо равен правому" (>=).
                case JGE:
                {
                    if (!(state.flags & State::FlagsBits::MAJORITY) || (state.flags & State::FlagsBits::EQUALITY)) { state.registers[State::CIR] = imm20 - 1; }
                    break;
                }

                // JG - переход при флаге "левый операнд больше правого" (>).
                case JG:
                {
                    if (!(state.flags & State::FlagsBits::MAJORITY) && !(state.flags & State::FlagsBits::EQUALITY)) { state.registers[State::CIR] = imm20 - 1; }
                    break;
                }

                // РАБОТА С ПАМЯТЬЮ.
                // LOAD - загрузка значения из памяти по указанному непосредственно адресу в регистр.
                case LOAD:
                {
                    state.registers[R1] = state.get_word(imm20);
                    break;
                }

                // STORE - выгрузка значения из регистра в память по указанному непосредственно адресу.
                case STORE:
                {
                    state.set_word(state.registers[R1], imm20);
                    break;
                }

                // LOAD2 - загрузка значения из памяти по указанному непосредственно адресу в пару регистров.
                case LOAD2:
                {
                    // Результат выполнения команды приведёт к выходу за пределы существующих регистров.
                    if (R1 + 1 >= State::registers_number) { throw OperationException::INVALIDREG; }

                    state.registers[R1] = state.get_word(imm20);
                    state.registers[R1 + 1] = state.get_word(imm20 + 1);
                    break;
                }

                // STORE2 - выгрузка значения из пары регистров в память по указанному непосредственно адресу.
                case STORE2:
                {
                    // Результат выполнения команды приведёт к выходу за пределы существующих регистров.
                    if (R1 + 1 >= State::registers_number) { throw OperationException::INVALIDREG; }

                    state.set_word(state.registers[R1], imm20);
                    state.set_word(state.registers[R1 + 1], imm20 + 1);
                    break;
                }

                // LOADR - загрузка значения из памяти по указанному во втором регистре адресу в первый регистр.
                case LOADR:
                {
                    try { state.registers[R1] = state.get_word(state.registers[R2] + imm16); }
                    catch (State::Exception exception) { throw OperationException::INVALIDMEM; }
                    break;
                }

                // STORER - выгрузка значения из регистра в память по указанному во втором регистре адресу.
                case STORER:
                {
                    try { state.set_word(state.registers[R1], state.registers[R2] + imm16); }
                    catch (State::Exception exception) { throw OperationException::INVALIDMEM; }
                    break;
                }

                // LOADR2 - загрузка значения из памяти по указанному во втором регистре адресу в пару регистров.
                case LOADR2:
                {
                    // Результат выполнения команды приведёт к выходу за пределы существующих регистров.
                    if (R1 + 1 >= State::registers_number) { throw OperationException::INVALIDREG; }

                    try
                    {
                        state.registers[R1] = state.get_word(state.registers[R2] + imm16);
                        state.registers[R1 + 1] = state.get_word(state.registers[R2] + imm16 + 1);
                    }
                    catch (State::Exception exception) { throw OperationException::INVALIDMEM; }
                    break;
                }

                // STORER2 - выгрузка значения из пары регистров в память по указанному во втором регистре адресу.
                case STORER2:
                {
                    // Результат выполнения команды приведёт к выходу за пределы существующих регистров.
                    if (R1 + 1 >= State::registers_number) { throw OperationException::INVALIDREG; }

                    try
                    {
                        state.set_word(state.registers[R1], state.registers[R2] + imm16);
                        state.set_word(state.registers[R1 + 1], state.registers[R2] + imm16 + 1);
                    }
                    catch (State::Exception exception) { throw OperationException::INVALIDMEM; }
                    break;
                }

                default:
                {
                    return_code = ReturnCode::ERROR;
                    break;
                }
            }
        }
        catch (OperationException exception)
        {
            switch(exception)
            {
                case OperationException::OK: { break; }
                case OperationException::INVALIDREG:
                {
                    std::cerr << "[EXECUTION ERROR]: access to an invalid register." << std::endl;
                    throw Exception::INVALIDSTATE;
                    break;
                }
                case OperationException::INVALIDMEM:
                {
                    std::cerr << "[EXECUTION ERROR]: access to an invalid address." << std::endl;
                    throw Exception::INVALIDSTATE;
                    break;
                }
                case OperationException::DIVBYZERO:
                {
                    std::cerr << "[EXECUTION ERROR]: division by zero." << std::endl;
                    throw Exception::MACHINE;
                    break;
                }
                case OperationException::REGOVERFLOW:
                {
                    std::cerr << "[EXECUTION ERROR]: register overflow." << std::endl;
                    throw Exception::MACHINE;
                    break;
                }
            }
        }

        ++(state.registers[State::CIR]);
        return return_code;
    }

    // PROTECTED:

    // PRIVATE:


    /////////////// TRANSLATOR ///////////////
    // PUBLIC:

    Translator::Translator()
    {
        // Таблица, связывающая имя операции, её код и тип.
        std::vector<std::tuple<std::string, OPERATION_CODE, OPERATION_TYPE>> translation_table =
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
        for (size_t index = 0; index < translation_table.size(); ++index)
        {
            // Ассемблирование.
            op_code.insert({ std::get<0>(translation_table[index]), std::get<1>(translation_table[index]) });
            op_type.insert({ std::get<0>(translation_table[index]), std::get<2>(translation_table[index]) });

            // Дизассемблирование.
            code_op.insert({ std::get<1>(translation_table[index]), std::get<0>(translation_table[index]) });
            code_type.insert({ std::get<1>(translation_table[index]), std::get<2>(translation_table[index]) });
        }

        // Таблица, связывающая имя регистра и его код.
        std::vector<std::tuple<std::string, int>> registers_table =
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
        for (size_t index = 0; index < registers_table.size(); ++index)
        {
            // Ассемблирование.
            reg_code.insert({ std::get<0>(registers_table[index]), std::get<1>(registers_table[index]) });

            // Дизассемблирование.
            code_reg.insert({ std::get<1>(registers_table[index]), std::get<0>(registers_table[index]) });
        }
    }
    Translator::~Translator()
    {
        // ...
    }

    int Translator::assemble(std::istream& input_stream, FUPM2EMU::State& state) const
    {
        size_t write_address = 0; // Адрес текущего записываемого слова в state.
        std::string input;        // Строка для считывания слов из входного файла.
        std::map<std::string, size_t> marks_addresses;          // Адреса объявленных меток (название - адрес).
        std::vector<std::pair<size_t, std::string>> used_marks; // Адреса команд, использовавших метки, и имена этих меток.

        enum class ParserState
        {
            IN_CODE    = 0,
            IN_COMMENT = 1,
        };
        ParserState current_state = ParserState::IN_CODE;

        try
        {
            // Чтение идёт до конца файла.
            while (!input_stream.eof())
            {
                switch(current_state)
                {
                    // В коде.
                    case ParserState::IN_CODE:
                    {
                        // Считываем строку-слово.
                        if (!(input_stream >> input)) { break; } // Проверка на успешность чтения.
                        if (!input.size()) { break; }            // Проверка строки на пустоту.

                        #ifdef DEBUG_OUTPUT_ASSEMBLING
                        std::cout << "Command/mark:" << input << std::endl;
                        #endif

                        // Начинается на ";" - входим в состояние "в комментарии" (до новой строки)
                        if (input[0] == ';')
                        {
                            current_state = ParserState::IN_COMMENT;
                            break;
                        }

                        // Если слово оканчивается на ':', оно является меткой.
                        if (input[input.size() - 1] == ':')
                        {
                            input.pop_back(); // Удаляем последний символ - ':'.
                            marks_addresses.insert({ input, write_address });
                            break;
                        }

                        // Если встретилась директива "word", просто оставляем слово по текущему адресу свободным.
                        if (input == "word")
                        {
                            ++write_address;
                            break;
                        }

                        // Если встретилась директива "end", запоминаем метку старта программы. Так как эта директива обязана быть в конце программы,
                        // к моменту её чтения метка уже точно должна существовать. Тогда можно сразу проинициализировать нужным значением регистр R15.
                        if (input == "end")
                        {
                            if (!(input_stream >> input)) { throw AssemblingException(write_address, AssemblingException::Code::MARK_EXPECTED); } // Чтение имени метки.
                            try { state.registers[State::CIR] = marks_addresses.at(input); }
                            catch (const std::out_of_range&) { throw AssemblingException(write_address, AssemblingException::Code::UNDECLARED_MARK); }
                            break;
                        }


                        // К этому моменту уже точно известно, что считанное слово должно быть именем операции. Тогда начинаем разбирать её и её аргументы.
                        uint32_t command = 0; // Создаём переменную для записываемого слова команды.

                        // Парсим операцию.
                        try {command |= op_code.at(input) << (bits_in_command - bits_in_op_code); }
                        catch (const std::out_of_range&) { throw AssemblingException(write_address, AssemblingException::Code::OP_CODE); }

                        // Парсим аргументы.
                        switch(op_type.at(input))
                        {
                            // Регистр и непосредственный операнд.
                            case RI:
                            {
                                // Регистр.
                                if (!(input_stream >> input)) { throw AssemblingException(write_address, AssemblingException::Code::REG_EXPECTED); }
                                try { command |= reg_code.at(input) << (bits_in_command - bits_in_op_code - bits_in_reg_code); }
                                catch (const std::out_of_range&) { throw AssemblingException(write_address, AssemblingException::Code::REG_CODE); }

                                // Непосредственный операнд.
                                if (!(input_stream >> input)) { throw AssemblingException(write_address, AssemblingException::Code::IMM_EXPECTED); }

                                // Проверка, число это, или метка.
                                // Если число, сразу подставляем значение, если метка - запоминаем адрес команды для последующей подстановки адреса метки.
                                if (input.find_first_not_of("0123456789") == std::string::npos)
                                {
                                    try { command |= (std::stoi(input) & 0xFFFFF); }
                                    catch (const std::out_of_range&) { throw AssemblingException::Code::BIG_IMM; };
                                }
                                else { used_marks.push_back(std::pair<size_t, std::string>(write_address, input)); }
                                break;
                            }

                            // Два регистра и короткий непосредственный операнд.
                            case RR:
                            {
                                // Первый регистр.
                                if (!(input_stream >> input)) { throw AssemblingException(write_address, AssemblingException::Code::REG_EXPECTED); }
                                try { command |= reg_code.at(input) << (bits_in_command - bits_in_op_code - bits_in_reg_code); }
                                catch (const std::out_of_range&) { throw AssemblingException(write_address, AssemblingException::Code::REG_CODE); }

                                // Второй регистр.
                                if (!(input_stream >> input)) { throw AssemblingException(write_address, AssemblingException::Code::REG_EXPECTED); }
                                try { command |= reg_code.at(input) << (bits_in_command - bits_in_op_code - bits_in_reg_code - bits_in_reg_code); }
                                catch (const std::out_of_range&) { throw AssemblingException(write_address, AssemblingException::Code::REG_CODE); }

                                // Непосредственный операнд.
                                if (!(input_stream >> input)) { throw AssemblingException(write_address, AssemblingException::Code::IMM_EXPECTED); }
                                // Перевод ввода в число и запись в конец слова.
                                try { command |= (std::stoi(input) & 0x0FFFF); } // 16 бит на короткий Imm.
                                catch (const std::out_of_range&) { throw AssemblingException::Code::BIG_IMM; };
                                break;
                            }

                            // Регистр и адрес.
                            case RM:
                            {
                                // Регистр.
                                if (!(input_stream >> input)) { throw AssemblingException(write_address, AssemblingException::Code::REG_EXPECTED); }
                                try { command |= reg_code.at(input) << (bits_in_command - bits_in_op_code - bits_in_reg_code); }
                                catch (const std::out_of_range&) { throw AssemblingException(write_address, AssemblingException::Code::REG_CODE); }

                                // Непосредственный операнд - адрес.
                                if (!(input_stream >> input)) { throw AssemblingException(write_address, AssemblingException::Code::ADDR_EXPECTED); }

                                // Проверка, число это, или метка.
                                // Если число, сразу подставляем адрес, если метка - запоминаем адрес команды для последующей подстановки адреса метки.
                                if (input.find_first_not_of("0123456789") == std::string::npos)
                                {
                                    try { command |= (std::stoi(input) & 0xFFFFF); }
                                    catch (const std::out_of_range&) { throw AssemblingException::Code::BIG_ADDR; };
                                }
                                else { used_marks.push_back(std::pair<size_t, std::string>(write_address, input)); }
                                break;
                            }

                            // Адрес.
                            case Me:
                            {
                                // Непосредственный операнд - адрес.
                                if (!(input_stream >> input)) { throw AssemblingException(write_address, AssemblingException::Code::ADDR_EXPECTED); }

                                // Проверка, число это, исли метка.
                                // Если число, сразу подставляем адрес, если метка - запоминаем адрес команды для последующей подстановки адреса метки.
                                if (input.find_first_not_of("0123456789") == std::string::npos)
                                {
                                    try { command |= (std::stoi(input) & 0xFFFFF); }
                                    catch (const std::out_of_range&) { throw AssemblingException::Code::BIG_ADDR; };
                                }
                                else { used_marks.push_back(std::pair<size_t, std::string>(write_address, input)); }
                                break;
                            }

                            // Непосредственный операнд.
                            case Im:
                            {
                                // Непосредственный операнд - число.
                                if (!(input_stream >> input)) { throw AssemblingException(write_address, AssemblingException::Code::IMM_EXPECTED); }

                                // Проверка, число это, исли метка.
                                // Если число, сразу подставляем значение, если метка - запоминаем адрес команды для последующей подстановки адреса метки.
                                if (input.find_first_not_of("0123456789") == std::string::npos)
                                {
                                    try { command |= (std::stoi(input) & 0xFFFFF); }
                                    catch (const std::out_of_range&) { throw AssemblingException::Code::BIG_IMM; };
                                }
                                else { used_marks.push_back(std::pair<size_t, std::string>(write_address, input)); }
                                break;
                            }
                        }

                        // Запись слова.
                        state.set_word(command, write_address);
                        ++write_address;
                        break;
                    }

                    // В комментарии.
                    case ParserState::IN_COMMENT:
                    {
                        input_stream.ignore (std::numeric_limits<std::streamsize>::max(), '\n'); // Пропускаем ввод до первого символа новой строки.
                        current_state = ParserState::IN_CODE;
                        break;
                    }
                }
            }

            // Теперь проходим по всем использованным меткам и подставляем адреса.
            for (size_t index = 0; index < used_marks.size(); ++index)
            {
                // Читаем слово-команду, вставляем адрес и записываем.
                uint32_t word = state.get_word(used_marks[index].first);
                try { word |= marks_addresses.at(used_marks[index].second); }
                catch (const std::out_of_range&) { throw AssemblingException(used_marks[index].first, AssemblingException::Code::UNDECLARED_MARK); }
                state.set_word(word, used_marks[index].first);
            }
        }
        catch (AssemblingException exception)
        {
            std::cerr << "[TRANSLATOR ERROR]: error assembling command " << exception.address + 1 << "." << std::endl;
            switch(exception.code)
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
                    std::cerr << "address was expected but was not specified." << std::endl;
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

            throw Exception::ASSEMBLING;
        }

        state.registers[State::SR] = State::memory_size - 1; // Размещение стека в конце памяти.
        return 0;
    }

    int Translator::disassemble(const State& state, std::ostream& output_stream) const
    {
        size_t address = 0; // Текущий адрес.
        uint32_t word = 0;  // Считанное слово.

        // Проход по всей памяти.
        while (address < State::memory_size)
        {
            // Цикл вывода непустой части памяти.
            while (address < State::memory_size)
            {
                // Чтение слова по текущему адресу.
                word = state.get_word(address);

                // Код будет короче, если вычислить все возможные операнды сразу.
                OPERATION_CODE operation = OPERATION_CODE((word >> 24) & 0xFF);
                uint8_t R1 = (word >> 20) & 0xF;
                uint8_t R2 = (word >> 16) & 0xF;
                int32_t imm16 = word & 0x0FFFF;
                int32_t imm20 = word & 0xFFFFF;

                // Два варианта: либо считана команда, либо нет.
                auto iterator = code_op.find(operation);
                if (iterator != code_op.end())
                {
                    // Вывод имени команды.
                    output_stream << iterator->second;

                    // Вывод аргументов.
                    switch (code_type.at(operation))
                    {
                        case RI:
                        {
                            output_stream << " " << code_reg.at(R1) << " " << imm20;
                            break;
                        }
                        case RR:
                        {
                            output_stream << " " << code_reg.at(R1) << " " << code_reg.at(R2) << " " << imm16;
                            break;
                        }
                        case RM:
                        {
                            output_stream << " " << code_reg.at(R1) << " " << imm20;
                            break;
                        }
                        case Me:
                        {
                            output_stream << " " << imm20;
                            break;
                        }
                        case Im:
                        {
                            output_stream << " " << imm20;
                            break;
                        }
                    }

                    // Конец вывода.
                    output_stream << std::endl;
                }
                else
                {
                    // Вывод слова.
                    output_stream << word << std::endl;
                }

                ++address;
                // Выход из цикла после первого пустого слова.
                if (!word) { break; }
            }

            // Цикл пропуска пустой части памяти.
            while (address < State::memory_size)
            {
                // Чтение слова по текущему адресу.
                word = state.get_word(address);

                // Если слово вдруг оказалось непустым, выходим из цикла.
                if (word) { break; }
                ++address;
            }
        }

        return 0;
    }

    // PROTECTED:

    //////// ASSEMBLING EXCEPTION ////////
    Translator::AssemblingException::AssemblingException(size_t init_address, Translator::AssemblingException::Code init_code) // Инициализация экземпляра исключения.
    {
        address = init_address;
        code = init_code;
    }


    // PRIVATE:


    ////////////////    Emulator    ////////////////
    // PUBLIC:
    Emulator::Emulator()
    {
        // ...
    }
    Emulator::~Emulator()
    {
        // ...
    }

    int Emulator::run(std::istream& input_stream, std::ostream& output_stream)
    {
        Executor::ReturnCode return_code = Executor::ReturnCode::OK; // Код возврата операции.

        // Пока все хорошо.
        while (return_code == Executor::ReturnCode::OK)
        {
            try
            {
                return_code = executor.step(state, input_stream, output_stream);

                #ifdef DEBUG_EXECUTION_STEPS
                getchar();
                #endif
            }
            catch (Executor::Exception exception)
            {
                switch (exception)
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
            std::cout << "return_code: " << static_cast<int>(return_code) << std::endl;
            #endif
        }
        return 0;
    }

    // PROTECTED:

    // PRIVATE:
}
