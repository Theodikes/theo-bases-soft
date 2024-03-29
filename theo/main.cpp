﻿#include "utils.hpp"
#include "commands.hpp"

/* Добавляет директорию, где находится исполняемый файл theo.exe, в Windows PATH, чтобы софт можно было
* запускать откуда угодно с помозью консоли, просто вызвав команду 'theo' с аргументами */
static DWORD addExecutablePathToWindowsRegisrty();

static const char* const usages[] = {
    "theo [command] [-command options] [command args]",
    NULL,
};

int main(int argc, const char** argv) {

    struct argparse argparse;
    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_GROUP(commandsDescription),
        OPT_END(),
    };
    argparse_init(&argparse, options, usages, ARGPARSE_STOP_AT_NON_OPTION);
    argparse_describe(&argparse, "\nFastest and most efficient database processing soft.", "\nCreated by Theo <Telegram: @just_temp | https://github.com/Theodikes>");
    argc = argparse_parse(&argparse, argc, argv);
    if (argc < 1) {
        argparse_usage(&argparse);
        if (addExecutablePathToWindowsRegisrty() == ERROR_SUCCESS)
            cout << "\nProgram has been successfully added to the Windows PATH, now it can be called from anywhere by writing 'theo' in cmd!\n" << endl;
        system("pause");
        return -1;
    }

    /* Try to run command with args provided. */
    struct cmd_struct* cmd = NULL;
    for (int i = 0; i < _countof(commands); i++) {
        if (!strcmp(commands[i].cmd, argv[0])) {
            cmd = &commands[i];
        }
    }
    if (cmd) {
        return cmd->fn(argc, argv);
    }
    return 0;
}

static DWORD addExecutablePathToWindowsRegisrty() {
    wstring programName = L"THEO_SOFT"; // Имя программы в user env, чтобы искать по нему при повторных запусках
    // Папка, в которой располагается исполняемый файл программы, на которую будет ссылаться Path
    wstring pathToDirectoryWithExecutable = getWorkingDirectoryPath();
    HKEY registryHkey; // Ключ регистра для доступа к редактированию и чтению user env, находящемуся в Windows Registry
    DWORD ALREADY_DONE = 14; // Код возврата на тот случай, если исполняемый файл софта уже добавлен в PATH

    // Проверяем, есть ли доступ к редактированию и чтению PATH из реестра, получаем ключ доступа registryKey
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_ALL_ACCESS, &registryHkey) != ERROR_SUCCESS) {
        // Если не получилось - показываем пользователю, что у софта недостаточно прав для работы с реестром
        cout << "Cannot get access to Windows Registry" << endl;
        return ERROR_ACCESS_DENIED;
    }

    // Проверяем, может, софт уже был ранее добавлен в PATH, если да, больше ничего делать не надо, выходим из функции
    if (RegQueryValueExW(registryHkey, programName.c_str(), nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) return ALREADY_DONE;

    // Получаем текущее значение пользовательской переменной PATH (одну строку со всеми путями) из регистра
    DWORD environmentPATHVariableRealLength = _MAX_ENV;
    wchar_t bufferForPATHValue[_MAX_ENV + 1];
    if (RegQueryValueExW(registryHkey, L"Path", nullptr, nullptr, (LPBYTE) bufferForPATHValue, &environmentPATHVariableRealLength) != ERROR_SUCCESS) {
        cout << "Cannot get current value of PATH variable in user environment" << endl;
        return ERROR_ACCESS_DENIED;
    }
    /* Ставим указатель на конец строки, поделив длину на размер wchar_t, поскольку длина указывается
    * в байтах, а в каждом символе wchar 2 байта по уомлчанию */
    bufferForPATHValue[environmentPATHVariableRealLength / sizeof(wchar_t)] = '\0';   
    
    // Полный PATH пользователя в environment, добавляем в него текущую директорию
    wstring pathVariableWithTheoDirectory = bufferForPATHValue + wstring(L";") + pathToDirectoryWithExecutable;

    // Пытаемся установить текущую директорию, в которой находится исполняемый файл софта, в Windows PATH
    if (RegSetValueExW(registryHkey, programName.c_str(), 0, REG_SZ, (LPBYTE) pathToDirectoryWithExecutable.c_str(), static_cast<DWORD>(pathToDirectoryWithExecutable.length() * sizeof(wchar_t))) != ERROR_SUCCESS or RegSetValueExW(registryHkey, L"Path", 0, REG_SZ, (LPBYTE)pathVariableWithTheoDirectory.c_str(), static_cast<DWORD>(pathVariableWithTheoDirectory.length() * sizeof(wchar_t))) != ERROR_SUCCESS) {
        cout << "Cannot add program to PATH (to Windows regisrty)" << endl;
        return ERROR_ACCESS_DENIED;
    }

    /* Принудительно обновляем регистр, чтобы можно было без перезагрузки компьютера вызывать программу,
     * только что добавленную в PATH, из других окон консоли */
    SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)"Environment", SMTO_BLOCK, 100, NULL);

    // Закрываем реестр после работы
    RegCloseKey(registryHkey);
    return ERROR_SUCCESS;
}