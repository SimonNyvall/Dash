#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <string>
#include <termios.h>
#include <regex>
#include "IO.hpp"
#include "../history/commandHistory.hpp"
#include "../shell.hpp"
#include "../prompt/prompt.hpp"

void enableRawMode(struct termios &orig_termios)
{
    struct termios raw;

    tcgetattr(STDIN_FILENO, &orig_termios);
    raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void disableRawMode(struct termios &orig_termios)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void refreshLine(const char *promt, std::string buffer, int position)
{
    std::cout << "\r\033[K"; // Clear the current line
    std::cout << promt << buffer;

    Prompt &prompt = Prompt::getInstance();
    int visiableLength = prompt.length();

    int len = buffer.size();

    if (position < len)
    {
        for (int i = len; i > position; --i)
        {
            std::cout << "\b";
        }
    }
}

std::string handleEscChars(std::string buffer, int *position, int *historyPosition)
{
    char seq[3];
    seq[0] = getchar();
    seq[1] = getchar();

    Prompt &prompt = Prompt::getInstance();

    if (seq[0] != '[')
    {
        return buffer;
    }

    if (seq[1] == 'A') // Up arrow
    {
        if (globalCommandHistory.size() == *historyPosition)
        {
            return buffer;
        }

        *historyPosition = *historyPosition + 1;

        const char *lastCommand = globalCommandHistory.getCommand(*historyPosition);

        if (!lastCommand)
        {
            return buffer;
        }

        std::cout << "\r\033[K"; // Clear the current line
        prompt.printPrompt();

        std::cout << lastCommand;

        buffer = lastCommand;

        *position = buffer.size();

        return buffer;
    }

    if (seq[1] == 'B') // Down arrow
    {
        if (*historyPosition == 1) //? This should not clear the last command but give me the buffer written before accessing the history
        {
            std::cout << "\r\033[K"; // Clear the current line
            prompt.printPrompt();
            *historyPosition = *historyPosition - 1;

            return buffer;
        }

        *historyPosition = *historyPosition - 1;

        const char *lastCommand = globalCommandHistory.getCommand(*historyPosition);

        if (!lastCommand)
        {
            return buffer;
        }

        std::cout << "\r\033[K"; // Clear the current line
        prompt.printPrompt();

        std::cout << lastCommand;

        buffer = lastCommand;

        *position = buffer.size();

        return buffer;
    }

    if (seq[1] == 'D') //* Left arrow
    {
        if (*position > 0)
        {
            std::cout << "\033[D"; // Move cursor left
            (*position)--;
        }
    }

    if (seq[1] == 'C') // Right arrow
    {
        if (*position < buffer.size())
        {
            std::cout << buffer[*position];
            (*position)++;
        }
    }

    if (seq[1] == '1') //* Home key
    {
        char t = getchar();

        if (t == '~')
        {
            *position = 0;

            std::cout << "\r\033[K"; // Clear the current line
            prompt.printPrompt();
            std::cout << buffer;

            for (int i = 0; i < buffer.size(); i++)
            {
                std::cout << '\b';
            }
        }
    }

    if (seq[1] == '4') //* End key
    {
        char t = getchar();

        if (t == '~')
        {
            *position = buffer.size();

            std::cout << "\r\033[K"; // Clear the current line
            prompt.printPrompt();
            std::cout << buffer;
        }
    }

    return buffer;
}

std::string readLine()
{
    std::string buffer;
    int c;
    struct termios orig_termios;
    int position = 0;
    int historyPosition = 0;

    enableRawMode(orig_termios);

    Prompt &promptInstance = Prompt::getInstance();

    std::string prompt = promptInstance.getPrompt();
    std::cout << prompt;

    while (true)
    {
        c = getchar();

        if (c == EOF || c == '\n') //* Check for EOF or Enter key
        {
            if (buffer.size() == 0)
            {
                std::cout << '\n';
            }

            std::cout << '\n';

            disableRawMode(orig_termios);

            if (buffer.empty())
            {
                return std::string();
            }

            char *command = strdup(buffer.c_str());
            globalCommandHistory.addCommand(command);

            historyPosition = 0;

            return buffer;
        }
        else if (c == '\033') //* ESC key
        {
            buffer = handleEscChars(buffer, &position, &historyPosition);
        }
        else if (c == 127) //* Backspace key
        {
            if (!buffer.empty() && position > 0)
            {
                buffer.erase(position - 1, 1);
                position--;

                refreshLine(prompt.c_str(), buffer, position);
            }
        }
        else if (c == 9) //* Tab key
        {
            std::regex cdRegex(R"(cd\s+)");

            if (std::regex_search(buffer, cdRegex))
            {
                int cdPos = buffer.find("cd ");
                std::string bufferCopy = buffer.substr(cdPos + buffer.size() - cdPos);

                std::vector<std::string> paths = tabCdHandler(buffer);

                if (paths.size() == 1)
                {
                    buffer = paths[0];
                    position = buffer.size();

                    refreshLine(prompt.c_str(), buffer, position);
                }
                else
                {
                    printCdPaths(paths);

                    std::cout << prompt << buffer;
                }
            }
            else
            {
                std::vector<std::string> commands = tabCommandHandler(buffer, position);

                if (commands.size() == 1)
                {
                    buffer = commands[0];
                    position = buffer.size();

                    refreshLine(prompt.c_str(), buffer, position);
                }
                else
                {
                    printCommands(commands);

                    std::cout << prompt << buffer;
                }
            }
        }
        else
        {
            buffer.insert(position, 1, static_cast<char>(c));
            position++;

            refreshLine(prompt.c_str(), buffer, position);
        }
    }
}