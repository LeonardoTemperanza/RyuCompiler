
#include "memory_management.cpp"
#include "base.cpp"

#include <windows.h>
#include <stdio.h>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

const char* testsPath = "../Project/Tests/";

int RunCompilerAndProgram(const char* sourcePath, int optLevel, bool* generatedFile)
{
    Assert(generatedFile);
    
    char cmdStr[1024];
    snprintf(cmdStr, 1024, "ryu.exe %s -O %d -o tmp.exe", sourcePath, optLevel);
    defer(remove("tmp.exe"));
    
    {
        STARTUPINFO startupInfo = {0};
        startupInfo.cb = sizeof(startupInfo);
        startupInfo.hStdInput  = nullptr;
        startupInfo.hStdOutput = nullptr;
        
        PROCESS_INFORMATION processInfo;
        if(!CreateProcess(0, cmdStr, 0, 0, false, 0, 0, 0, &startupInfo, &processInfo))
        {
            fprintf(stderr, "Could not launch compiler");
            *generatedFile = false;
        }
        
        WaitForSingleObject(processInfo.hProcess, INFINITE);
        CloseHandle(processInfo.hProcess);
        CloseHandle(processInfo.hThread);
    }
    
    FILE* toCheck = fopen("tmp.exe", "r");
    if(!toCheck)
    {
        *generatedFile = false;
        return 0;
    }
    fclose(toCheck);
    
    *generatedFile = true;
    
    DWORD ret = 0;
    {
        STARTUPINFO startupInfo = {0};
        startupInfo.cb = sizeof(startupInfo);
        startupInfo.hStdInput  = nullptr;
        startupInfo.hStdOutput = nullptr;
        
        PROCESS_INFORMATION processInfo;
        if(!CreateProcess(0, "tmp.exe", 0, 0, false, 0, 0, 0, &startupInfo, &processInfo))
        {
            fprintf(stderr, "Could not launch compiler");
            *generatedFile = false;
        }
        
        WaitForSingleObject(processInfo.hProcess, INFINITE);
        GetExitCodeProcess(processInfo.hProcess, &ret);
        CloseHandle(processInfo.hProcess);
        CloseHandle(processInfo.hThread);
    }
    
    return ret;
}

int main()
{
    printf("Ryu Tester\n");
    fflush(stdout);
    
    for(const fs::directory_entry& entry : fs::recursive_directory_iterator(testsPath))
    {
        fs::path path = entry.path();
        if(entry.is_regular_file() && path.extension() == ".ryu")
        {
            std::cout << path.extension() << '\n';
            
            char* contents = ReadEntireFileIntoMemoryAndNullTerminate(path.string().c_str());
        }
    }
    
#if 0
    for(int i = 0; i < StArraySize(programs); ++i)
    {
        printf("    file %d> ", i+1);
        
        const char* fileName = "tmp.ryu";
        FILE* programFile = fopen(fileName, "w+b");
        if(!programFile)
        {
            fprintf(stderr, "Could not open file which contains the tests\n");
            return 1;
        }
        
        fwrite(programs[i], strlen(programs[i]) + 1, 1, programFile);
        fclose(programFile);
        
        int optLevels[] = {0, 1};
        bool generatedFile = false;
        int ret = RunCompilerAndProgram(fileName, 0, &generatedFile);
        
        if(!generatedFile)
            printf("X -- The compiler did not generate an executable file\n");
        else if(ret == expectedReturns[i])
            printf("V\n");
        
        // Remove the file
        remove(fileName);
    }
#endif
    
    return 0;
}
