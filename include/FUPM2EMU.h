#pragma once
#include<cstdint>    // Целочисленные типы фиксированной длины.
#include<vector>     // vector.
#include<map>        // map.
#include<fstream>    // file stream.


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

    ////////////////

    // Вспомогательные функции.
    inline uint32_t ReadWord(uint8_t *Address);              // Конвертация четырёх uint8_t в uint32_t по указаному адресу.
    inline void WriteWord(uint32_t Value, uint8_t *Address); // Конвертация uint32_t в четыре uint8_t и запись их в нужном порядке по указанному адресу.


    /////////////////  STATE  ////////////////
    // Состояние машины - значение регистров, флагов, указатель на блок памяти.
    class State
    {
    public:
        // Константы.
        static const uint8_t BytesInWord = 4;                // Число байт в машинном слове.
        static const uint8_t BitsInWord  = BytesInWord * 8;  // Число бит в машинном слове.
        static const uint8_t AddressBits = 20;               // Число бит в адресах.
        static const size_t  MemorySize  = 1 << AddressBits; // Размер адресуемой памяти (в словах).
        static const uint8_t RegistersNumber = 16;           // Количество регистров.

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
        int32_t Registers[RegistersNumber]; // Массив регистров (32 бита).
        uint8_t Flags;                      // Регистр флагов (битность не задана спецификацией).
        std::vector<uint8_t> Memory;        // Память эмулируемой машины.

        // Методы.
        State();
        ~State();

        // Загрузка состояния из потока файла.
        int load(std::fstream &FileStream);

        // Удобные и сокращающие длину кода обёртки над ReadWord() и WriteWord(), работающие с Memory.
        inline uint32_t getWord(size_t Address);
        inline void setWord(uint32_t Value, size_t Address);

    protected:

    private:

    };


    //////////////// EXECUTOR ////////////////
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
        inline ReturnCode step(State &OperatedState);

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
        static const uint8_t BitsInCommand = State::BitsInWord; // Число битов на команду (в нашем случае команда - одно слово).
        static const uint8_t BitsInOpCode  = 8; // Число бит на код операции.
        static const uint8_t BitsInRegCode = 4; // Число бит на код регистра.

        // Коды исключений.
        enum class Exception
        {
            OK,         // OK.
            ASSEMBLING, // Ошибка при ассемблировании.
        };

        // Методы.
        Translator();
        ~Translator();

        int Assemble(std::fstream &FileStream, State &OperatedState);    // Ассемблирование кода из файла.
        int Disassemble(State &OperatedState, std::fstream &FileSTream); // Дизассемблирование состояния в файл.

    protected:
        // Данные для трансляции.
        std::map<std::string, int> OpCode;  // Отображение из имени операции в её код.
        std::map<std::string, int> OpType;  // Отображение из имени операции в её тип.
        std::map<std::string, int> RegCode; // Отображение из имени регистра в его код.

        // Целая структура для обработки исключений.
        struct AssemblingException
        {
            // Коды исключений
            enum class Code
            {
                OK,       // OK.
                OPCODE,   // Несуществующий код операции.
                REGCODE,  // Несуществующий код регистра.
                ARGSREG,  // Ожидалось имя регистра.
                ARGSIMM,  // Ожидался численный непосредственный операнд.
                ARGSADDR, // Ожидался адрес.
                ARGSMARK, // Ожидалось имя метки.
                MARK,     // Несуществующее имя метки.
            };

            size_t address; // Адресс срабатывания (адресс записанной операции).
            Code code;      // Код исключения.

            // Все переменные в обязательном порядке инициализируются кконструктором.
            AssemblingException(size_t initAddress, Code initCode);
        };

    private:

    };


    //////////////// EMULATOR ////////////////
    // Эмулятор - интерфейс для работы с исполнителем машинных команд, состоянием машины и транслятором ассемблера.
    class Emulator
    {
    public:
        // Данные.
        State state;           // Текущее состояние машины.
        Executor executor;     // Исполнитель команд.
        Translator translator; // Ассемблер и дисассемблер.

        // Методы.
        Emulator();
        ~Emulator();

        int Run(); // Выполнить текущее состояние.

    protected:

    private:

    };
}
