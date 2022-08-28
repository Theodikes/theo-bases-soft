#pragma once
#ifndef MY_UTILS
#define MY_UTILS

#include <Windows.h>
#include <string>
#include <iostream>
#include <regex>
#include <stdbool.h>
#include <filesystem>
#include "libs/argparse/argparse.h" // https://github.com/cofyc/argparse
#include "libs/robinhood.h" // https://github.com/martinus/robin-hood-hashing

using namespace std;
namespace fs = std::filesystem;

// Оптимальный размер чанка диска (ssd) для записи и чтения за одну операцию (fread/fwrite), вычислено тестированием
constexpr unsigned OPTIMAL_DISK_CHUNK_SIZE = 1024 * 1024 * 64;


// Я люблю очевилные и чистые условия, как в питоне, извините
#define and &&
#define or ||
#define not !

// Переменная для небольших положительных числовых значений, влезающих в байт памяти
#define ushortest unsigned char

// СОкращение ull для уменьшения количества кода и размера аргументов функций
#define ull unsigned long long

// Проверяет, начинается ли строка с определённой другой строки (например, startsWith("test", "testing") == true)
bool startsWith(const char* pre, const char* str);

// Проверяет, заканчивается ли строка определённой другой строкой (например, endsWith("testing", "ing") == true)
bool endsWith(const char* str, const char* suffix);

// Функция для объединения абсолютного пути к папке и имени файла в абсолютный путь к файлу. Возвращает итоговый путь
string joinPaths(string dirPath, string filePath);

// Функция принимает в качестве аргумента валидный путь к файлу и возвращает имя файла (без расширения)
string getFileNameWithoutExtension(string pathToFile);

/* Добавляет в массив путей к файлам, который передан в первом аргументе, все .txt файлы, находящиеся в директории,
путь к которой передан вторым аргументом, и её поддиректориях, либо сам файл, если вторым аргументом передан путь
не к директории, а к файлу */
bool processSourceFileOrDirectory(robin_hood::unordered_flat_set<string>*, string path, bool recursive);

// Заносит файл по указанному пути, если он имеет расширение .txt, в список файлов для обработки (нормализации, дедупликации etc)
bool addFileToSourceList(robin_hood::unordered_flat_set<string>*, string filePath);

// Считает количество строк, разделённых символами переноса строк, в тексте. Каждая строка должна заканчиваться символом \n
size_t getLinesCountInText(char* bytes);

// Возвращает количество байт информации в файле, если файл не найден или к нему нет доступа, возаращает -1
long long getFileSize(const char* pathToFile);

// Возвращает количество свободной оперативной памяти в байтах
ull getAvailableMemoryInBytes(void);

// Существует ли что-либо по указанному пути
bool isAnythingExistsByPath(string path);

// Является ли указанный путь путём к директории
bool isDirectory(string path);

// Является ли регулярное выражение (переданное строкой) валидным
bool isValidRegex(string regularExpression);

// Возвращает строку, содержащую путь к текущец директории (откуда вызвана программа, исполняемый файл)
string getWorkingDirectoryPath();
#endif // !MY_UTILS