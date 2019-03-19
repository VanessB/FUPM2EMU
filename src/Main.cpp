#include"../include/FUPM2EMU.h"
#include<iostream>
#include<string>
#include<fstream>

int main(int argc,  char *argv[])
{
    enum class ArgsException
    {
        OK         = 0, // Все нормально.
        INCOMPARGS = 1, // Несовместимые аргументы.
        NOFILEPATH = 2, // Не указан путь.
    };

    enum class FileMode
    {
        DEFAULT  = 0, // Без загрузки файлов.
        STATE    = 1, // Загрузка состояния памяти.
        ASSEMBLE = 2, // Загрузка и трансляция исходного кода.
    };

    std::string FilePath;
    FileMode InitMode = FileMode::DEFAULT;

    try
    {
        std::string Argument;
        for (int i = 1; i < argc; ++i)
        {
            Argument = argv[i];

            // Загрузка состояния эмулятора из файла.
            if ((Argument == "--load") || (Argument == "-l"))
            {
                if (InitMode != FileMode::DEFAULT) { throw(ArgsException::INCOMPARGS); }
                if (i + 1 >= argc) { throw(ArgsException::NOFILEPATH); }

                InitMode = FileMode::STATE;
                FilePath = argv[i+1];
                ++i;
            }

            // Ассемблирование исходного кода из файла в состояние эмулятора.
            if ((Argument == "--assemble") || (Argument == "-a"))
            {
                if (InitMode != FileMode::DEFAULT) { throw(ArgsException::INCOMPARGS); }
                if (i + 1 >= argc) { throw(ArgsException::NOFILEPATH); }

                InitMode = FileMode::ASSEMBLE;
                FilePath = argv[i+1];
                ++i;
            }
        }
    }
    catch (ArgsException Exception)
    {
        switch(Exception)
        {
            case ArgsException::OK: { break; }
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

    if (!FilePath.empty())
    {
        switch(InitMode)
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
                    std::cerr << "Failed to open file: " << FilePath << std::endl;
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
                    FUPM2.translator.Assemble(FileStream, FUPM2.state);
                    FileStream.close();
                }
                else
                {
                    std::cerr << "Failed to open file: " << FilePath << std::endl;
                }
                break;
            }
        }
    }

    // Запуск эмуляции.
    FUPM2.Run();
    return(0);
}
