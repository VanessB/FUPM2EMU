#include"../include/FUPM2EMU.h"
#include<iostream>
#include<iomanip>
#include<string>
#include<fstream>
#include<chrono>

// Глобальные константы для вывода информации.
const std::string Version   = "0.9";
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
    // Возможные состояния инициализации.
    enum class ArgsException
    {
        OK,          // Все нормально.
        UNKNOWNARGS, // Неизвестные аргументы.
        INCOMPARGS,  // Несовместимые аргументы.
        NOFILEPATH,  // Не указан путь.
    };

    enum class FileMode
    {
        DEFAULT,  // Без загрузки файлов.
        STATE,    // Загрузка состояния памяти.
        ASSEMBLE, // Загрузка и трансляция исходного кода.
    };

    // Измерение времени работы.
    bool Benchmark = false;

    // Робота с файлом.
    std::string FilePath;
    FileMode initFileMode = FileMode::DEFAULT;

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
                if (initFileMode != FileMode::DEFAULT) { throw(ArgsException::INCOMPARGS); }
                if (i + 1 >= argc) { throw(ArgsException::NOFILEPATH); }

                initFileMode = FileMode::STATE;
                FilePath = argv[i+1];
                ++i;
            }

            // Ассемблирование исходного кода из файла в состояние эмулятора.
            else if ((Argument == "--assemble") || (Argument == "-a"))
            {
                if (initFileMode != FileMode::DEFAULT) { throw(ArgsException::INCOMPARGS); }
                if (i + 1 >= argc) { throw(ArgsException::NOFILEPATH); }

                initFileMode = FileMode::ASSEMBLE;
                FilePath = argv[i+1];
                ++i;
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
                std::cerr << "[CLI ERROR]: unknown arguments. Try using --help." << std::endl;
                break;
            }
            case ArgsException::INCOMPARGS:
            {
                std::cerr << "[CLI ERROR]: incompatable arguments." << std::endl;
                break;
            }
            case ArgsException::NOFILEPATH:
            {
                std::cerr << "[CLI ERROR]: file path has not been passed." << std::endl;
            }
        }

        return(0);
    }

    // Экземпляр эмулятора.
    FUPM2EMU::Emulator FUPM2;

    if (!FilePath.empty())
    {
        switch(initFileMode)
        {
            case FileMode::DEFAULT:
            {
                break;
            }
            case FileMode::STATE:
            {
                // Загрузка файла состояния, передача потока файла эмулятору.
                std::fstream FileStream;
                FileStream.open(FilePath, std::fstream::in);
                if (FileStream.is_open())
                {
                    FUPM2.state.load(FileStream);
                    FileStream.close();
                }
                else
                {
                    std::cerr << "[CLI ERROR]: Failed to open file: " << FilePath << std::endl;
                }
                break;
            }
            case FileMode::ASSEMBLE:
            {
                // Загрузка файла с исходным кодом, передача потока файла эмулятору.
                std::fstream FileStream;
                FileStream.open(FilePath, std::fstream::in);
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
                    std::cerr << "[CLI ERROR]: Failed to open file: " << FilePath << std::endl;
                }
                break;
            }
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
