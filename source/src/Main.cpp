#include "Global.h"
#include "Application.h"
#include "Log.h"
#include "Storage.h"
#include "Presentation.h"

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
#else
int main(int argc, char** argv)
#endif
{
#ifdef _WIN32
    sApplication->Init(ToWideString(lpCmdLine));
#else
    char* line = new char[1024];
    memset(line, 0, sizeof(char)*1024);
    for (uint32 i = 1; i <= argc; i++)
        sprintf(line, "%s %s", line, argv[0]);

    sApplication->Init(ToWideString((const char*)line));
#endif

    if (!sApplication->Initialized())
        return -1;

    sApplication->Run();

    return 0;
}

Application::Application()
{
    m_init = false;
}

Application::~Application()
{
}

void Application::Init(const wchar_t *cmdline)
{
    std::vector<wchar_t> option;
    option.clear();

    bool endWithQuotes = false;
    if (cmdline[0] == '\"')
        endWithQuotes = true;

    for (uint32 i = endWithQuotes?1:0; i <= wcslen(cmdline); i++)
    {
        if ((endWithQuotes && cmdline[i] == '\"') || (!endWithQuotes && (cmdline[i] == ' ' || cmdline[i] == '\0')))
        {
            // recognize option
            option.push_back('\0');
            const wchar_t* opt = CharVectorToString(&option);
            if (opt)
            {
                // parameter is file
                if (opt[0] != '-')
                {
                    // init error file
                    wchar_t* fpath = ExtractFolderFromPath(opt);
                    if (fpath)
                    {
#ifdef _WIN32
                        _wchdir(fpath);
#else
                        chdir(ToMultiByteString(fpath));
#endif
                        wchar_t newpath[256];
                        swprintf(&newpath[0], 99999, L"%s\\err.log", fpath);
                        sLog->InitErrorFile(newpath);
                    }
                    else
                        sLog->InitErrorFile(L"./err.log");

                    if (!sStorage->ReadInputSupfile(opt))
                        return;

                    if (!sStorage->ParseInputFiles())
                        return;
                }
            }

            option.clear();
        }
        else
            option.push_back(cmdline[i]);
    }

    m_init = true;
}

void Application::Run()
{
    if (!m_init)
        return;

    if (!sPresentation->Init())
        return;

#ifdef _WIN32
    sPresentation->Run();
#endif
}
