#pragma once
#include<cstdint>    // Целочисленные типы фиксированной длины.
#include<memory>     // Умные указатели.
#include<vector>     // vector
#include<functional> // function
#include<fstream>    // file stream


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

    // Коды, возвращаемые инструкциями эмулятору.
    enum OP_RETURNCODE
    {
        OP_OK        = 0, // Продолжение работы.
        OP_TERMINATE = 1, // Штатное завершение.
        OP_WARNING   = 2, // Не описанная в спецификации потенциально опасная работа.
        OP_ERROR     = 3, // Критическая ошибка.
    };

    // Коды испключений при выполнении операции.
    enum OP_EXCEPTION
    {
        OPEX_OK         = 0, // OK.
        OPEX_INVALIDREG = 1, // Доступ к несуществующим регистрам.
        OPEX_DIVBYZERO  = 2, // Деление на ноль.
    };

    // Коды исключений исполнителя.
    enum EXECUTOR_EXCEPTION
    {
        EXEX_OK           = 0, // OK.
        EXEX_MACHINE      = 1, // Исключение, сгенерированное эмулируемой машиной.
        EXEX_INVALIDSTATE = 2, // Исключение, вызванное невалидным состоянием эмулируемой машины.
    };

    ////////////////

    // Константы.
    const unsigned char BitsInWord = 32;        // Число бит в машинном слове.
    const unsigned char BytesInWord = 4;        // Число байт в машинном слове.
    const unsigned char AddressBits = 20;       // Число бит в адресах.
    const unsigned char RegistersNumber = 16;   // Количество регистров.
    const unsigned int InstructionsNumber = 72; // Количество инструкций.
    const size_t MemorySize = 1 << AddressBits; // Размер адресуемой памяти.

    // Вспомогательные функции.
    inline uint32_t ReadWord(uint8_t *Address);              // Конвертация четырёх uint8_t в uint32_t по указаному адресу.
    inline void WriteWord(uint32_t Value, uint8_t *Address); // Конвертация uint32_t в четыре uint8_t и запись их в нужном порядке по указанному адресу.


    /////////////////  STATE  ////////////////
    // Состояние машины - значение регистров, флагов, указатель на блок памяти.
    struct State
    {
        int32_t Registers[RegistersNumber]; // Массив регистров (32 бита).
        uint8_t Flags;                      // Регистр флагов (битность не задана спецификацией).
        std::vector<uint8_t> Memory;        // Память эмулируемой машины.

        // Удобные и сокращающие длину кода обёртки над ReadWord() и WriteWord(), работающие с Memory.
        inline uint32_t getWord(size_t Address);
        inline void setWord(uint32_t Value, size_t Address);

        State();
        ~State();
    };


    //////////////// EXECUTOR ////////////////
    // Исполнитель машинных команд.
    class Executor
    {
    public:
        Executor(State* initState);
        ~Executor();

        // Выполнение комманды.
        inline OP_RETURNCODE operator [](uint32_t Command);

    protected:
        State* OperatedState; // Прикреплённое состояние эмулируемой машины (что-то мне эта иерархическая паутина уже не нравится).

    private:

    };


    //////////////// EMULATOR ////////////////
    // Эмулятор - создание и выполнение состояния.
    class Emulator
    {
    public:
        Emulator();
        ~Emulator();

        int Translate(); // Перевести код на языке assembler в готовое к выполнению состояние и сделать это состояние текущим.
        int Run();       // Выполнить текущее состояние.

        int LoadState(std::fstream &FileStream); // Загрузка состояния из потока файла.

    protected:
        State CurrentState; // Текущее состояние машины.
        Executor Execute;   // Исполнитель команд.

    private:

    };
}
