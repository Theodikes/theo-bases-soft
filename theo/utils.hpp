#pragma once
#ifndef MY_UTILS
#define MY_UTILS

#include <Windows.h>
#include <string>
#include <iostream>
#include <stdbool.h>
#include <filesystem>
#include "libs/argparse/argparse.h" // https://github.com/cofyc/argparse
#include "libs/tiny-regex/re.h" // https://github.com/kokke/tiny-regex-c
#include "libs/sparsepp/spp.h" // https://github.com/greg7mdp/sparsepp

using namespace spp;
using namespace std;
namespace fs = std::filesystem;

// Оптимальный размер чанка диска (ssd) для записи и чтения за одну операцию (fread/fwrite), вычислено тестированием
static constexpr unsigned OPTIMAL_DISK_CHUNK_SIZE = 1024 * 1024 * 64;


// Разделители путей в различных системах
#ifdef _WIN32
#define PATH_JOIN_SEPARATOR   "\\"
#else
#define PATH_JOIN_SEPARATOR   "/"
#endif

// Переменная для небольших положительных числовых значений, влезающих в байт памяти
#define ushortest unsigned char

// Проверяет, начинается ли строка с определённой другой строки (например, startsWith("test", "testing") == true)
bool startsWith(const char* pre, const char* str);

// Проверяет, заканчивается ли строка определённой другой строкой (например, endsWith("testing", "ing") == true)
bool endsWith(const char* str, const char* suffix);

// Функция для объединения абсолютного пути к папке и имени файла в абсолютный путь к файлу. Возвращает итоговый путь
char* path_join(const char* dir, const char* file);

// Функция принимает в качестве аргумента валидный путь к файлу и возвращает имя файла (с расширением)
const char* getFilenameFromPath(const char* pathToFile);

// Функция принимает в качестве аргумента валидный путь к файлу и возвращает имя файла (без расширения)
string getFileNameWithoutExtension(string pathToFile);

/* Добавляет в массив путей к файлам, который передан в первом аргументе, все .txt файлы, находящиеся в директории,
путь к которой передан вторым аргументом, и её поддиректориях, либо сам файл, если вторым аргументом передан путь
не к директории, а к файлу */
bool processSourceFileOrDirectory(sparse_hash_set<string>*, const char* path);

// Заносит файл по указанному пути, если он имеет расширение .txt, в список файлов для обработки (нормализации, дедупликации etc)
bool addFileToSourceList(sparse_hash_set<string>*, const char* filePath);

// Хеширование строки, на выходе уникальное число
unsigned long long get_hash(char* s);

// Считает количество строк, разделённых символами переноса строк, в тексте. Каждая строка должна заканчиваться символом \n
size_t getLinesCountInText(char* bytes);

// Возвращает количество байт информации в файле, если файл не найден или к нему нет доступа, возаращает 0
unsigned long long getFileSize(const char* pathToFile);
#endif // !MY_UTILS