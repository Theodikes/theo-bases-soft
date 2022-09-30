#pragma once
#ifndef MY_UTILS
#define MY_UTILS

#include <string>
#include <iostream>
#include <regex>
#include <stdbool.h>
#include <filesystem>
#include <dbstl_set.h> // https://docs.oracle.com/cd/E17076_05/html/index.html (Berkeley DB)
#include "libs/argparse/argparse.h" // https://github.com/cofyc/argparse
#include "libs/robinhood.h" // https://github.com/martinus/robin-hood-hashing
#include <Windows.h>

using namespace std;
using namespace dbstl;
namespace fs = std::filesystem;


// Оптимальный размер чанка диска (ssd) для записи и чтения за одну операцию (fread/fwrite), вычислено тестированием
constexpr unsigned OPTIMAL_DISK_CHUNK_SIZE = 1024 * 1024 * 64;


// Я люблю очевидные и чистые условия, как в питоне, извините
#define and &&
#define or ||
#define not !

// Сокращение ull для уменьшения количества кода и размера аргументов функций
#define ull unsigned long long

// Тип для хранилища уникальных хешей (чисел unsigned long long), расположенного в дисковой памяти
typedef db_set<ull, ElementHolder<ull>> db_hashset;

// Информация о входных файлах, переданных юзером для обработки, сделал отдельный тип для лучшего понимания
#define sourcefiles_info robin_hood::unordered_flat_set<string>

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
bool processSourceFileOrDirectory(sourcefiles_info*, string path, bool recursive);

// Заносит файл по указанному пути, если он имеет расширение .txt, в список файлов для обработки (нормализации, дедупликации etc)
bool addFileToSourceList(sourcefiles_info*, string filePath);

/* Получает все входные пути для обработки из аргументов, введённых юзером (и папки, и файлы).
* Возвращает список путей ко всем входным уникальным файлам (включая файлы из директорий и поддиректорий, 
* если параметр рекурсии - true), которые будут обработаны софтом. */
sourcefiles_info getSourceFilesFromUserInput(size_t sourcePathsCount, const char** userSourcePaths, bool checkDirectoriesRecursive);

// Считает количество строк, разделённых символами переноса строк, в тексте. Каждая строка должна заканчиваться символом \n
size_t getLinesCountInText(char* bytes);

// Возвращает количество байт информации в файле, если файл не найден или к нему нет доступа, возаращает -1
long long getFileSize(const char* pathToFile);

// Возвращает количество свободной оперативной памяти в байтах
ull getAvailableMemoryInBytes(void);

// Возвращает количество максимальной оперативной памяти в байтах (с учетом используемой)
ull getTotalMemoryInBytes(void);

// Возвращает процент используемой оперативной памяти от её общего количества
size_t getMemoryUsagePercent(void);

// Существует ли что-либо по указанному пути
bool isAnythingExistsByPath(string path);

// Является ли указанный путь путём к директории
bool isDirectory(string path);

// Возвращает строку с путём к директории, в которой лежит файл, находящийся по пути filePath
string getDirectoryFromFilePath(string filePath);

// Является ли регулярное выражение (переданное строкой) валидным
bool isValidRegex(string regularExpression);

// Возвращает строку, содержащую путь к текущец директории (откуда вызвана программа, исполняемый файл)
string getWorkingDirectoryPath();

/* Проверяет директорию, указанную пользователем как директорию вывода, если есть какая-то оишбка
 * (например, на месте директории с таким же именем уже присутствует файл)- завершает работу программы */
void checkDestinationDirectory(const char* destinationDirectoryPath);

// Спрашиваем пользователя, надо ли создавать папку по указанному пути, если создана - возвращает true, иначе false
bool createDirectoryUserDialog(string creatingDirectoryPath);

/* Считывает переданный файл оптимальными для чтения чанками (с записью чанков во временный буфер).
* Каждый считанный чанк в буфере обрезается по границе последней строки, далее во входном файле идёт отступ назад
* на отрезанный кусок неполной строки, а полученный буфер обрабатывается функцией processChunkBuffer, уникальной
* для каждой команды(у deduplicate будет одна функция, у normalize другая), и итоговые данные после обработки
* входного буфера записываются в итоговый файл (будет ли это общий файл, определяет функция выше уровнем). */
void processFileByChunks(FILE* inputFile, FILE* resultFile, size_t processChunkBuffer(char*, size_t, char*));

/* Обработка каждого файла из списка путей ко всем файлам, переданным пользователем. Обёртка верхнего уровня
* для функции processFileByChunks, служит для корректной обработки ситуации со множеством входных файлов
* (тогда как та функция работает исключительно с одним входным и одним выходным).
* Обрабатывает ситуацию, когда пользователю требуется сложить все итоговые строки в один файл, если же нет - 
* создаёт для каждого входного файла свой собственный итоговый с обработанными строками.
* Для каждого конкретного входного файла все действия выполняются с помощью функции 'processFileByChunks'.
* После полного выполнения функция закрывает все открытые файлы. */
void processAllSourceFiles(sourcefiles_info sourceFilesPaths, bool needMerge, FILE* resultFile, string destinationDirectoryPath, string resultFilesSuffix, size_t processChunkBuffer(char* inputBuffer, size_t inputBufferLength, char* resultBuffer));

/* Генерирует валидный путь к итоговому файлу и открывает сам файл, используя имя входного файла, 
 * итоговую директорию и суффикс функции, который надо добавлять ко всем обработанным файлам. 
 * Чтобы не было пересечений с другими файлами (из других папок, но с такими же названиями),
 * если у файлов такое же имя, использует так же первый доступный номер (цифру) в конце имени файла.
 * Например, после нормализации файла test.txt и если это первый нормализуемый файл с таким именем,
 * в итоговой директории создастся и откроется файл test_normalized_1.txt. Если функция-обработчик другая,
 * то и суффикс другой, например, после токенизации test.txt в результате будет test_tokenized_1.txt.
 * ВОзвращает указатель на открытый файл в режиме бинарной записи. */
FILE* getResultFilePtr(string pathToResultFolder, string pathToSourceFile, string fileSuffixName);
#endif // !MY_UTILS