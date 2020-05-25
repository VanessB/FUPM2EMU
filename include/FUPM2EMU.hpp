#ifndef FUPM2EMU_HPP
#define FUPM2EMU_HPP

#include <cstdint>    // Целочисленные типы фиксированной длины.
#include <vector>     // vector.
#include <map>        // map.
#include <iostream>   // file stream.


// НЕБОЛЬШОЙ КОММЕНТАРИЙ КАСАТЕЛЬНО РАБОТЫ С ПАМЯТЬЮ.
// Память реализована как массив uint8_t. Слово-команда передаётся в uint32_t. Это крайне удобно для передачи аргументов операции, однако требует конвертации из четырёх uint_8t в uint32_t.

namespace FUPM2EMU
{
    // Коды операций.
    enum OPERATION_CODE
    {
        HALT    = 0,
        SYSCALL = 1,
        ADD     = 2,
        ADDI    = 3,
        SUB     = 4,
        SUBI    = 5,
        MUL     = 6,
        MULI    = 7,
        DIV     = 8,
        DIVI    = 9,
        LC      = 12,
        SHL     = 13,
        SHLI    = 14,
        SHR     = 15,
        SHRI    = 16,
        AND     = 17,
        ANDI    = 18,
        OR      = 19,
        ORI     = 20,
        XOR     = 21,
        XORI    = 22,
        NOT     = 23,
        MOV     = 24,
        ADDD    = 32,
        SUBD    = 33,
        MULD    = 34,
        DIVD    = 35,
        ITOD    = 36,
        DTOI    = 37,
        PUSH    = 38,
        POP     = 39,
        CALL    = 40,
        CALLI   = 41,
        RET     = 42,
        CMP     = 43,
        CMPI    = 44,
        CMPD    = 45,
        JMP     = 46,
        JNE     = 47,
        JEQ     = 48,
        JLE     = 49,
        JL      = 50,
        JGE     = 51,
        JG      = 52,
        LOAD    = 64,
        STORE   = 65,
        LOAD2   = 66,
        STORE2  = 67,
        LOADR   = 68,
        LOADR2  = 69,
        STORER  = 70,
        STORER2 = 71
    };

    // Типы операций.
    enum OPERATION_TYPE
    {
        RI = 0, // Регистр - непосредственный операнд.
        RR = 1, // Регистр - регистр.
        RM = 2, // Регистр - адрес.
        Me = 3, // Адрес.
        Im = 4, // Непосредственный операнд.
    };


    ////////////////      State      ///////////////
    // Состояние машины: значение регистров, флагов, указатель на блок памяти.
    class State
    {
    public:
        // Константы.
        static const uint8_t bytes_in_word = 4;                 // Число байт в машинном слове.
        static const uint8_t bits_in_word  = bytes_in_word * 8; // Число бит в машинном слове.
        static const uint8_t address_bits  = 20;                // Число бит в адресах.
        static const size_t  memory_size   = 1 << address_bits; // Размер адресуемой памяти (в словах).
        static const uint8_t registers_number = 16;             // Количество регистров.
        static const uint8_t CIR = 15; // Current instruction register - номер текущей инструкции.
        static const uint8_t SR  = 14; // Stack register - адрес стека.

        // Коды исключений.
        enum class Exception
        {
            OK,     // OK.
            MEMORY, // Выход за пределы адресуемой памяти.
        };

        // Биты регистра флагов.
        struct FlagsBits // Невозможность использовать namespace внутри class крайне бесит.
        {
            enum Positions
            {
                EQUALITY_POS = 0, // Позиция бита равенства.
                MAJORITY_POS = 1, // Позиция бита преобладания.
            };

            enum Masks
            {
                EQUALITY = 1 << EQUALITY_POS, // Бит равенства. 0 - не равны, 1 - равны.
                MAJORITY = 1 << MAJORITY_POS, // Бит преобладания. 0 - левый операнд больше правого, 1 - меньше.
            };
        };

        // Данные состояния.
        int32_t registers[registers_number]; // Массив регистров (32 бита).
        uint8_t flags;                       // Регистр флагов (разрядность не задана спецификацией).
        std::vector<uint8_t> memory;         // Память эмулируемой машины.

        // Методы.
        State();
        ~State();

        // Загрузка состояния из потока.
        int load(std::istream& input_stream);

        // Удобные и сокращающие длину кода обёртки над read_word() и write_word(), работающие с memory.
        inline uint32_t get_word(size_t address) const;
        inline void set_word(uint32_t value, size_t address);

    protected:

    private:

    };


    ////////////////    Executor    ////////////////
    // Исполнитель машинных команд.
    class Executor
    {
    public:
        // Коды исключений исполнителя.
        enum class Exception
        {
            OK,           // OK.
            MACHINE,      // Исключение, сгенерированное эмулируемой машиной.
            INVALIDSTATE, // Исключение, вызванное невалидным состоянием эмулируемой машины.
        };

        // Коды, возвращаемые исполнителем эмулятору.
        enum class ReturnCode
        {
            OK,        // Продолжение работы.
            TERMINATE, // Штатное завершение.
            WARNING,   // Не описанная в спецификации потенциально опасная работа.
            ERROR,     // Критическая ошибка.
        };

        // Методы.
        Executor();
        ~Executor();

        // Выполнение команды.
        inline ReturnCode step(State& state, std::istream& input_stream, std::ostream& output_stream);

    protected:
        // Коды испключений при выполнении операции.
        enum class OperationException
        {
            OK,          // OK.
            INVALIDREG,  // Доступ к несуществующим регистрам.
            INVALIDMEM,  // Выход за пределы доступной памяти.
            DIVBYZERO,   // Деление на ноль.
            REGOVERFLOW, // Переполнение регистра.
        };

    private:

    };


    /////////////// TRANSLATOR ///////////////
    // Транслятор из языка assembler в машинный код и обратно.
    class Translator
    {
    public:
        // Константы.
        static const uint8_t bits_in_command = State::bits_in_word; // Число битов на команду (в нашем случае команда - одно слово).
        static const uint8_t bits_in_op_code  = 8; // Число бит на код операции.
        static const uint8_t bits_in_reg_code = 4; // Число бит на код регистра.

        // Коды исключений.
        enum class Exception
        {
            OK,            // OK.
            ASSEMBLING,    // Ошибка при ассемблировании.
            DISASSEMBLING, // Ошибка при дизассемблировании.
        };

        // Методы.
        Translator();
        ~Translator();

        // Ассемблирование кода из файла.
        int assemble(std::istream& input_stream, State& state) const;

        // Дизассемблирование состояния в файл.
        int disassemble(const State& state, std::ostream& output_sream) const;

    protected:
        // Данные для трансляции.
        std::map<std::string, OPERATION_CODE> op_code; // Отображение из имени операции в её код.
        std::map<std::string, OPERATION_TYPE> op_type; // Отображение из имени операции в её тип.
        std::map<std::string, int> reg_code; // Отображение из имени регистра в его код.

        std::map<OPERATION_CODE, std::string> code_op;      // Отображение из кода операции в её имя.
        std::map<OPERATION_CODE, OPERATION_TYPE> code_type; // Отображение из кода операции в её тип.
        std::map<int, std::string> code_reg; // Отображение из кода регистра в его имя.

        // Структура для обработки исключений при ассемблировании.
        struct AssemblingException
        {
            // Коды исключений
            enum class Code
            {
                OK,              // OK.
                OP_CODE,         // Несуществующий код операции.
                REG_CODE,        // Несуществующий код регистра.
                REG_EXPECTED,    // Ожидалось имя регистра.
                IMM_EXPECTED,    // Ожидался численный непосредственный операнд.
                ADDR_EXPECTED,   // Ожидался адрес.
                MARK_EXPECTED,   // Ожидалось имя метки.
                UNDECLARED_MARK, // Несуществующее имя метки.
                BIG_IMM,         // Непосредственный операнд слишком большой.
                BIG_ADDR,        // Аддрес слишком большой.
            };

            size_t address; // Адресс срабатывания (адресс записанной операции).
            Code code;      // Код исключения.

            // Все переменные в обязательном порядке инициализируются кконструктором.
            AssemblingException(size_t init_address, Code init_code);
        };

    private:

    };


    ////////////////    Emulator    ////////////////
    // Эмулятор - интерфейс для работы с исполнителем машинных команд, состоянием машины и транслятором ассемблера.
    class Emulator
    {
    public:
        // Данные.
        State state;           // Текущее состояние машины.
        Executor executor;     // Исполнитель команд.
        Translator translator; // Ассемблер и дизассемблер.

        // Методы.
        Emulator();
        ~Emulator();

        int run(std::istream& input_stream, std::ostream& output_stream); // Выполнить текущее состояние.

    protected:

    private:

    };
}

#endif
