#include"../include/FUPM2EMU.h"
#include<iostream>
#include<iomanip>
#include<string>
#include<fstream>
#include<chrono>

// Глобальные константы для вывода информации.
const std::string Version   = "0.92";
const std::string TitleText = "FUPM2EMU version " + Version;
const std::string HelpText  =
R"(
Arguments:
  --help, -h                    Show help reference
  --load, -l         <file>     Get machine's state from the file and run it.
  --assemble, -a     <file>     Translate assembler code from the file and run the result
  --benchmark, -b               Run the program with execution time beeing measured
)";

int main(int argc,  char *argv[])
{
    // Исключения загрузчика.
    enum class ArgsException
    {
        OK,          // Все нормально.
        UNKNOWNARGS, // Неизвестные аргументы.
        INCOMPARGS,  // Несовместимые аргументы.
        NOFILEPATH,  // Не указан путь.
    };

    // Режимы обработки файла инициализации.
    enum class InitFileModes
    {
        DEFAULT,  // Без загрузки файлов.
        STATE,    // Загрузка состояния памяти.
        ASSEMBLE, // Загрузка и трансляция исходного кода.
    };

    // Измерение времени работы.
    bool Benchmark = false;

    // Инициализация из файла.
    std::string InitFilePath;
    InitFileModes InitFileMode = InitFileModes::DEFAULT;

    // Дизассемблирование в файл.
    //std::string DisassemblyFilePath;
    bool Disassemble = false;

    try
    {
        std::string Argument;
        for (int i = 1; i < argc; ++i)
        {
            Argument = argv[i];

            // Справка о работе программы.
            if ((Argument == "--help") || (Argument == "-h"))
            {
                if (i == 1) { std::cout << TitleText << std::endl <<  HelpText << std::endl; }
                else { throw(ArgsException::INCOMPARGS); } // Справка вызывается первым аргументом.
                return(0);
            }

            // Загрузка состояния эмулятора из файла.
            else if ((Argument == "--load") || (Argument == "-l"))
            {
                if (InitFileMode != InitFileModes::DEFAULT) { throw(ArgsException::INCOMPARGS); }
                if (i + 1 >= argc) { throw(ArgsException::NOFILEPATH); }

                InitFileMode = InitFileModes::STATE;
                InitFilePath = argv[i+1];
                ++i;
            }

            // Ассемблирование исходного кода из файла в состояние эмулятора.
            else if ((Argument == "--assemble") || (Argument == "-a"))
            {
                if (InitFileMode != InitFileModes::DEFAULT) { throw(ArgsException::INCOMPARGS); }
                if (i + 1 >= argc) { throw(ArgsException::NOFILEPATH); }

                InitFileMode = InitFileModes::ASSEMBLE;
                InitFilePath = argv[i+1];
                ++i;
            }

            // Дизассемблирование состояния эмулятора.
            else if ((Argument == "--disassemble") || (Argument == "-d"))
            {
                Disassemble = true;
            }

            // Измерение времени компиляции (если была) и работы эмулируемой программы.
            else if ((Argument == "--benchmark") || (Argument == "-b"))
            {
                Benchmark = true;
            }

            // Неизвестные аргументы.
            else
            {
                throw(ArgsException::UNKNOWNARGS);
            }
        }
    }
    catch (ArgsException Exception)
    {
        switch(Exception)
        {
            case ArgsException::OK: { break; }
            case ArgsException::UNKNOWNARGS:
            {
                std::cerr << "Error: unknown arguments. Try using --help." << std::endl;
                break;
            }
            case ArgsException::INCOMPARGS:
            {
                std::cerr << "Error: incompatable arguments." << std::endl;
                break;
            }
            case ArgsException::NOFILEPATH:
            {
                std::cerr << "Error: file path has not been passed." << std::endl;
            }
        }

        return(0);
    }

    // Экземпляр эмулятора.
    FUPM2EMU::Emulator FUPM2;

    if (!InitFilePath.empty())
    {
        switch(InitFileMode)
        {
            case InitFileModes::DEFAULT:
            {
                break;
            }
            case InitFileModes::STATE:
            {
                // Загрузка файла состояния, передача потока файла эмулятору.
                std::fstream FileStream;
                FileStream.open(InitFilePath, std::fstream::in);
                if (FileStream.is_open())
                {
                    FUPM2.state.load(FileStream);
                    FileStream.close();
                }
                else
                {
                    std::cerr << "Error: failed to open file: " << InitFilePath << std::endl;
                }
                break;
            }
            case InitFileModes::ASSEMBLE:
            {
                // Загрузка файла с исходным кодом, передача потока файла эмулятору.
                std::fstream FileStream;
                FileStream.open(InitFilePath, std::fstream::in);
                if (FileStream.is_open())
                {
                    if (Benchmark)
                    {
                        std::clock_t StartAssembling = std::clock();
                        FUPM2.translator.Assemble(FileStream, FUPM2.state);
                        std::clock_t EndAssembling = std::clock();
                        std::cout << std::fixed << std::setprecision(2)
                                  << "[BENCHMARK]: Assembling CPU time used: "
                                  << 1000.0 * (EndAssembling - StartAssembling) / CLOCKS_PER_SEC << "ms" << std::endl
                                  << std::defaultfloat;
                    }
                    else
                    {
                        FUPM2.translator.Assemble(FileStream, FUPM2.state);
                    }
                    FileStream.close();
                }
                else
                {
                    std::cerr << "Error: failed to open file: " << InitFilePath << std::endl;
                }
                break;
            }
        }
    }

    // Дизассемблирование.
    if (Disassemble)
    {
        std::fstream FileStream;
        FileStream.open("a.asm", std::fstream::out);
        if (FileStream.is_open())
        {
            FUPM2.translator.Disassemble(FUPM2.state, FileStream);
        }
        else
        {
            std::cerr << "Error: failed to write disassembly file" << std::endl;
        }
    }

    // Запуск эмуляции.
    if (Benchmark)
    {
        std::clock_t StartExecution = std::clock();
        FUPM2.Run();
        std::clock_t EndExecution = std::clock();
        std::cout << std::fixed << std::setprecision(2)
                  << "[BENCHMARK]: Execution CPU time used: "
                  << 1000.0 * (EndExecution - StartExecution) / CLOCKS_PER_SEC << "ms" << std::endl
                  << std::defaultfloat;
    }
    else
    {
        FUPM2.Run();
    }
    return(0);
}
