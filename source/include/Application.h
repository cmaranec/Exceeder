#ifndef EXCDR_APPLICATION_H
#define EXCDR_APPLICATION_H

#include "Singleton.h"

class Application
{
    public:
        Application();
        ~Application();

        void Init(const wchar_t* cmdline);
        bool Initialized() { return m_init; };
        void Run();

#ifndef _WIN32
        int* argc;
        char** argv;
#endif

    private:
        bool m_init;
};

#define sApplication Singleton<Application>::instance()

#endif
