#include "utils.hpp"
#include "commands.hpp"

/* Добавляет директорию, где находится исполняемый файл theo.exe, в Windows PATH, чтобы софт можно было
* запускать откуда угодно с помозью консоли, просто вызвав команду 'theo' с аргументами */
static DWORD addExecutablePathToWindowsRegisrty();

static const char* const usages[] = {
    "theo [global options] [command] [command options] [command args]",
    NULL,
};

int main(int argc, const char** argv) {
    if (addExecutablePathToWindowsRegisrty() == ERROR_SUCCESS) 
        cout << "\nProgram has been successfully added to the Windows PATH, now it can be called from anywhere by writing 'theo' in cmd!\n" << endl;

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
    const char* programName = "THEO_SOFT"; // Имя программы в user env, чтобы искать по нему при повторных запусках
    string pathToDirectoryWithExecutable = getWorkingDirectoryPath();
    HKEY registryHkey; // Ключ регистра для доступа к редактированию и чтению user env, находящемуся в Windows Registry
    DWORD ALREADY_DONE = 14; // Код возврата на тот случай, если исполняемый файл софта уже добавлен в PATH

    // Проверяем, есть ли доступ к редактированию и чтению PATH из реестра, получаем ключ доступа registryKey
    if (RegOpenKeyEx(HKEY_CURRENT_USER, "Environment", 0, KEY_ALL_ACCESS, &registryHkey) != ERROR_SUCCESS) {
        // Если не получилось - показываем пользователю, что у софта недостаточно прав для работы с реестром
        cout << "Cannot get access to Windows Registry" << endl;
        return ERROR_ACCESS_DENIED;
    }

    // Проверяем, может, софт уже был ранее добавлен в PATH, если да, больше ничего делать не надо, выходим из функции
    if (RegQueryValueEx(registryHkey, programName, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) return ALREADY_DONE;

    // Полный PATH пользователя в environment, добавляем в него текущую директорию
    string pathVariableWithTheoDirectory = string(getenv("PATH")) + ";" + pathToDirectoryWithExecutable;

    // Пытаемся установить текущую директорию, в которой находится исполняемый файл софта, в Windows PATH
    if (RegSetValueEx(registryHkey, programName, 0, REG_SZ, (BYTE*)pathToDirectoryWithExecutable.c_str(), static_cast<DWORD>(pathToDirectoryWithExecutable.length())) != ERROR_SUCCESS or RegSetValueEx(registryHkey, "Path", 0, REG_SZ, (BYTE*)pathVariableWithTheoDirectory.c_str(), static_cast<DWORD>(pathVariableWithTheoDirectory.length())) != ERROR_SUCCESS) {
        cout << "Cannot add program to PATH (to Windows regisrty)" << endl;
        return ERROR_ACCESS_DENIED;
    }
    // Закрываем реестр после работы
    RegCloseKey(registryHkey);
    return ERROR_SUCCESS;
}