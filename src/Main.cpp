#include"../include/FUPM2EMU.h"
#include<iostream>
#include<string>
#include<fstream>

enum CLIException
{
    CLI_OK         = 0,
    CLI_DOUBLELOAD = 1,
    CLI_NOFILEPATH = 2,
};

int main(int argc,  char *argv[])
{
    std::string StateFilePath;
    try
    {
        std::string Argument;
        for (int i = 1; i < argc; ++i)
        {
            Argument = argv[i];

            // Загрузка состояния эмулятора из файла.
            if ((Argument == "--load") || (Argument == "-l"))
            {
                if (!StateFilePath.empty()) { throw(CLI_DOUBLELOAD); }
                if (i + 1 >= argc) { throw(CLI_NOFILEPATH); }

                StateFilePath = argv[i+1];
                ++i;
            }
        }
    }
    catch (CLIException Exception)
    {
        switch(Exception)
        {
            case CLI_OK: { break; }
            case CLI_DOUBLELOAD:
            {
                std::cerr << "Error: state file path has been already defined." << std::endl;
                break;
            }
            case CLI_NOFILEPATH:
            {
                std::cerr << "Error: state file path was not passed." << std::endl;
            }
        }

        return(0);
    }

    // Экземпляр эмулятора.
    FUPM2EMU::Emulator FUPM2;

    if (!StateFilePath.empty())
    {
        // Загрузка файла состояния, передача потока файла эмулятору.
        std::fstream FileStream;
        FileStream.open(StateFilePath, std::fstream::in);
        if (FileStream.is_open())
        {
            FUPM2.LoadState(FileStream);
            FileStream.close();
        }
        else
        {
            std::cerr << "Failed to open file: " << StateFilePath << std::endl;
        }
    }

    FUPM2.Run();
    return(0);
}
