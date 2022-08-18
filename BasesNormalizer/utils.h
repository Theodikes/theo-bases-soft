#pragma once
#ifndef MY_UTILS
#define MY_UTILS

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdbool.h>
#include "libs/argparse/argparse.h" // https://github.com/cofyc/argparse
#include "libs/tiny-regex/re.h" // https://github.com/kokke/tiny-regex-c
#include <Windows.h>

// Разделители путей в различных системах
#ifdef _WIN32
#define PATH_JOIN_SEPARATOR   "\\"
#else
#define PATH_JOIN_SEPARATOR   "/"
#endif

// Переменная для небольших положительных числовых значений, влезающих в байт памяти
#define ushortest unsigned char
// Максимальная длина пути к файлу в Windows
#define MAX_PATH 1024

// Проверяет, начинается ли строка с определённой другой строки (например, startsWith("test", "testing") == true)
bool startsWith(const char* pre, const char* str);

// Проверяет, заканчивается ли строка определённой другой строкой (например, endsWith("testing", "ing") == true)
bool endsWith(const char* str, const char* suffix);

// Функция для объединения абсолютного пути к папке и имени файла в абсолютный путь к файлу. Возвращает итоговый путь
char* path_join(const char* dir, const char* file);

// Функция принимает в качестве аргумента валидный путь к файлу и возвращает имя файла (с расширением)
char* getFilenameFromPath(char* pathToFile);

#endif // !MY_UTILS