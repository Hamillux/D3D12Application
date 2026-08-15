#include "Application.h"

#include <Windows.h>

#include <cstdlib>
#include <exception>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    try
    {
        Application application(instance, showCommand);
        return application.Run();
    }
    catch (const std::exception& error)
    {
        MessageBoxA(nullptr, error.what(), "D3D12 Application", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }
}
