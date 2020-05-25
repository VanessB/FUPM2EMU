#include <iostream>
#include <iomanip>
#include <string>
#include <fstream>
#include <chrono>

#include "FUPM2EMU.hpp"

// Глобальные константы для вывода информации.
const std::string version   = "0.93";
const std::string title_text = "FUPM2EMU version " + version;
const std::string help_text  =
R"(
Arguments:
  --help, -h                    Show help reference
  --load, -l         <file>     Get machine's state from the file and run it.
  --assemble, -a     <file>     Translate assembler code from the file and run the result
  --disassemble, -d             Disassemble current machine's state.
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
    bool benchmark = false;

    // Инициализация из файла.
    std::string init_file_path;
    InitFileModes init_file_mode = InitFileModes::DEFAULT;

    // Дизассемблирование в файл.
    //std::string DisassemblyFilePath;
    bool disassemble = false;

    try
    {
        std::string argument;
        for (int i = 1; i < argc; ++i)
        {
            argument = argv[i];

            // Справка о работе программы.
            if ((argument == "--help") || (argument == "-h"))
            {
                if (i == 1) { std::cout << title_text << std::endl << help_text << std::endl; }
                else { throw ArgsException::INCOMPARGS; } // Справка вызывается первым аргументом.
                return(0);
            }

            // Загрузка состояния эмулятора из файла.
            else if ((argument == "--load") || (argument == "-l"))
            {
                if (init_file_mode != InitFileModes::DEFAULT) { throw ArgsException::INCOMPARGS; }
                if (i + 1 >= argc) { throw ArgsException::NOFILEPATH; }

                init_file_mode = InitFileModes::STATE;
                init_file_path = argv[i+1];
                ++i;
            }

            // Ассемблирование исходного кода из файла в состояние эмулятора.
            else if ((argument == "--assemble") || (argument == "-a"))
            {
                if (init_file_mode != InitFileModes::DEFAULT) { throw ArgsException::INCOMPARGS; }
                if (i + 1 >= argc) { throw ArgsException::NOFILEPATH; }

                init_file_mode = InitFileModes::ASSEMBLE;
                init_file_path = argv[i+1];
                ++i;
            }

            // Дизассемблирование состояния эмулятора.
            else if ((argument == "--disassemble") || (argument == "-d"))
            {
                disassemble = true;
            }

            // Измерение времени компиляции (если была) и работы эмулируемой программы.
            else if ((argument == "--benchmark") || (argument == "-b"))
            {
                benchmark = true;
            }

            // Неизвестные аргументы.
            else
            {
                throw ArgsException::UNKNOWNARGS;
            }
        }
    }
    catch (ArgsException exception)
    {
        switch(exception)
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

    if (!init_file_path.empty())
    {
        switch(init_file_mode)
        {
            case InitFileModes::DEFAULT:
            {
                break;
            }
            case InitFileModes::STATE:
            {
                // Загрузка файла состояния, передача потока файла эмулятору.
                std::fstream file_stream;
                file_stream.open(init_file_path, std::fstream::in);
                if (file_stream.is_open())
                {
                    FUPM2.state.load(file_stream);
                    file_stream.close();
                }
                else
                {
                    std::cerr << "Error: failed to open file: " << init_file_path << std::endl;
                }
                break;
            }
            case InitFileModes::ASSEMBLE:
            {
                // Загрузка файла с исходным кодом, передача потока файла эмулятору.
                std::fstream file_stream;
                file_stream.open(init_file_path, std::fstream::in);
                if (file_stream.is_open())
                {
                    if (benchmark)
                    {
                        std::clock_t start_assembling = std::clock();
                        FUPM2.translator.assemble(file_stream, FUPM2.state);
                        std::clock_t end_assembling = std::clock();
                        std::cout << std::fixed << std::setprecision(2)
                                  << "[BENCHMARK]: Assembling CPU time used: "
                                  << 1000.0 * (end_assembling - start_assembling) / CLOCKS_PER_SEC << "ms" << std::endl
                                  << std::defaultfloat;
                    }
                    else
                    {
                        FUPM2.translator.assemble(file_stream, FUPM2.state);
                    }
                    file_stream.close();
                }
                else
                {
                    std::cerr << "Error: failed to open file: " << init_file_path << std::endl;
                }
                break;
            }
        }
    }

    // Дизассемблирование.
    if (disassemble)
    {
        std::fstream file_stream;
        file_stream.open("a.asm", std::fstream::out);
        if (file_stream.is_open())
        {
            FUPM2.translator.disassemble(FUPM2.state, file_stream);
        }
        else
        {
            std::cerr << "Error: failed to write disassembly file" << std::endl;
        }
    }

    // Запуск эмуляции.
    if (benchmark)
    {
        std::clock_t start_execution = std::clock();
        FUPM2.run(std::cin, std::cout);
        std::clock_t end_execution = std::clock();
        std::cout << std::fixed << std::setprecision(2)
                  << "[BENCHMARK]: Execution CPU time used: "
                  << 1000.0 * (end_execution - start_execution) / CLOCKS_PER_SEC << "ms" << std::endl
                  << std::defaultfloat;
    }
    else
    {
        FUPM2.run(std::cin, std::cout);
    }
    return 0;
}
