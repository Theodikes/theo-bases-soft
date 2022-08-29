#pragma once
#ifndef THEO_COMMANDS
#define THEO_COMMANDS

int normalize(int argc, const char** argv); // Команда для нормализации файлов
int merge(int argc, const char** argv); // Команда для объединения файлов в один
int split(int argc, const char** argv); // Команда для разделения файла на строки
int deduplicate(int argc, const char** argv); // Команда для удаления совпадающих строк из файла
int count(int argc, const char** argv);

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
    {"split", split},
    {"d", deduplicate},
    {"dedup", deduplicate},
    {"c", count},
    {"count", count}
};

static const char* const commandsDescription = "Commands:\n\
            normalize, n    Normalize bases\n\
            merge, m        Merge files\n\
            split, s        Split file by number of lines\n\
            dedup, d        Delete duplicate lines in file\n\
            count, c        Count number of strings in files\n";

#endif // !THEO_COMMANDS
