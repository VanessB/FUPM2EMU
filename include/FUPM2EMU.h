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
        RI = 0,
        RR = 1,
        RM = 2,
        J  = 3
    };

    ////////////////

    // Константы.
    const unsigned char BitsInWord = 32;        // Число бит в машинном слове.
    const unsigned char BytesInWord = 4;        // Число байт в машинном слове.

    const unsigned char OpCodeBits = 8;         // Число бит на код операции.
    const unsigned char RegCodeBits = 4;        // Число бит на код регистра.
    const unsigned char AddressBits = 20;       // Число бит в адресах.

    const unsigned char RegistersNumber = 16;   // Количество регистров.
    const unsigned int InstructionsNumber = 72; // Количество инструкций.
    const size_t MemorySize = 1 << AddressBits; // Размер адресуемой памяти.

    // Вспомогательные функции.
    inline uint32_t ReadWord(uint8_t *Address);              // Конвертация четырёх uint8_t в uint32_t по указаному адресу.
    inline void WriteWord(uint32_t Value, uint8_t *Address); // Конвертация uint32_t в четыре uint8_t и запись их в нужном порядке по указанному адресу.


    /////////////////  STATE  ////////////////
    // Состояние машины - значение регистров, флагов, указатель на блок памяти.
    class State
    {
    public:
        int32_t Registers[RegistersNumber]; // Массив регистров (32 бита).
        uint8_t Flags;                      // Регистр флагов (битность не задана спецификацией).
        std::vector<uint8_t> Memory;        // Память эмулируемой машины.

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
            OK           = 0, // OK.
            MACHINE      = 1, // Исключение, сгенерированное эмулируемой машиной.
            INVALIDSTATE = 2, // Исключение, вызванное невалидным состоянием эмулируемой машины.
        };

        // Коды, возвращаемые исполнителем эмулятору.
        enum class ReturnCode
        {
            OK        = 0, // Продолжение работы.
            TERMINATE = 1, // Штатное завершение.
            WARNING   = 2, // Не описанная в спецификации потенциально опасная работа.
            ERROR     = 3, // Критическая ошибка.
        };

        Executor();
        ~Executor();

        // Выполнение команды.
        inline ReturnCode step(State &OperatedState);

    protected:
        // Коды испключений при выполнении операции.
        enum class OperationException
        {
            OK         = 0, // OK.
            INVALIDREG = 1, // Доступ к несуществующим регистрам.
            DIVBYZERO  = 2, // Деление на ноль.
        };

    private:

    };


    /////////////// TRANSLATOR ///////////////
    // Транслятор из языка assembler в машинный код и обратно.
    class Translator
    {
    public:
        enum class Exception
        {
            OK         = 0, // OK.
            ASSEMBLING = 1, // Ошибка при ассемблировании.
        };

        Translator();
        ~Translator();

        int Assemble(std::fstream &FileStream, State &OperatedState);    // Ассемблирование кода из файла.
        int Disassemble(State &OperatedState, std::fstream &FileSTream); // Дизассемблирование состояния в файл.

    protected:
        //std::vector<std::tuple<std::string, int, int>> TranslationTable; // Таблица, связывающая имя операции, её код и тип.
        std::map<std::string, int> OpCode;  // Отображение из имени операции в её код.
        std::map<std::string, int> OpType;  // Отображение из имени операции в её тип.
        std::map<std::string, int> RegCode; // Отображение из имени регистра в его код.

        // Целая структура для обработки исключений.
        struct AssemblingException
        {
            // Коды исключений
            enum class Code
            {
                OK       = 0, // OK.
                OPCODE   = 1, // Несуществующий код операции.
                REGCODE  = 2, // Несуществующий код регистра.
                ARGSREG  = 3, // Ожидалось имя регистра.
                ARGSIMM  = 4, // Ожидался численный непосредственный операнд.
                ARGSMARK = 5, // Ожидалось имя метки.
                MARK     = 6, // Несуществующее имя метки.
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
        State state;           // Текущее состояние машины.
        Executor executor;     // Исполнитель команд.
        Translator translator; // Ассемблер и дисассемблер.

        Emulator();
        ~Emulator();

        int Run(); // Выполнить текущее состояние.

        //int LoadState(std::fstream &FileStream);     // Загрузка состояния из потока файла.
        //int AssembleState(std::fstream &FileStream); // Перевести код на языке assembler в готовое к выполнению состояние и сделать это состояние текущим.

    protected:

    private:

    };
}
