#pragma once
#ifndef THEO_COMMANDS
#define THEO_COMMANDS

int normalize(int argc, const char** argv); // Команда для нормализации файлов
int merge(int argc, const char** argv); // Команда для объединения файлов в один
int split(int argc, const char** argv); // Команда для разделения файла на строки

struct cmd_struct {
    const char* cmd;
    int (*fn) (int, const char**);
};

static struct cmd_struct commands[] = {
    {"n", normalize},
    {"normalize", normalize},
    {"m", merge},
    {"merge", merge},
    {"s", split},
    {"split", split}
};

static const char* const commandsDescription = "Commands:\n\
            normalize, n    Normalize bases\n\
            merge, m        Merge files\n\
            split, s        Split file by number of lines";

#endif // !THEO_COMMANDS
