#pragma once
#ifndef MY_UTILS
#define MY_UTILS

// Чтобы можно было без ошибок компилятора переводить wstring в обычные string и наоборот через codecvt
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

#include <string>
#include <iostream>
#include <regex>
#include <stdbool.h>
#include <filesystem>
#include <locale>
#include <codecvt>
#include <dbstl_set.h> // https://docs.oracle.com/cd/E17076_05/html/index.html (Berkeley DB)
#include "libs/argparse/argparse.h" // https://github.com/cofyc/argparse
#include "libs/robinhood.h" // https://github.com/martinus/robin-hood-hashing
#include <Windows.h>

using namespace std;
using namespace dbstl; // Неймспейс библиотеки для работы с базой данных
namespace fs = std::filesystem;

// Оптимальный размер чанка диска (ssd) для записи и чтения за одну операцию (fread/fwrite), вычислено тестированием
constexpr unsigned OPTIMAL_DISK_CHUNK_SIZE = 1024 * 1024 * 64;

// Средняя длина строки в файле обычной базы с аккаунтами
#define AVERAGE_STRING_LEGTH_IN_FILE 16

// Я люблю очевидные и чистые условия, как в питоне, извините
#define and &&
#define or ||
#define not !

// Сокращение ull для уменьшения количества кода и размера аргументов функций
#define ull unsigned long long

/* Строка, которую надо добавлять в начало длинного пути(длиннее MAX_PATH), чтобы
* система Windows считала его валидным. Например, \\?\C:\Users\Admin\somelooooo......ng */
#define WIN_LONG_PATH_START LR"(\\?\)"

// Тип для хранилища уникальных хешей (чисел unsigned long long), расположенного в дисковой памяти
typedef db_set<ull, ElementHolder<ull>> db_hashset;

// Информация о входных файлах, переданных юзером для обработки, сделал отдельный тип для лучшего понимания
#define sourcefiles_info robin_hood::unordered_flat_set<wstring>

// Функции для конвертации обычных строк в wide-строки и обратно
wstring toWstring(string s);
string fromWstring(wstring s);

// Функция для объединения абсолютного пути к папке и имени файла в абсолютный путь к файлу. Возвращает итоговый путь
wstring joinPaths(wstring dirPath, wstring filePath) noexcept;

/* Открывает файлы с именем в любой кодировке верным образом, при невозможности открыть возвращает NULL */
FILE* fileOpen(wstring filePath, const wchar_t* openFlags) noexcept;
FILE* fileOpen(wstring filePath, string openFlags) noexcept;
FILE* fileOpen(string filePath, string openFlags) noexcept;

// Функция принимает в качестве аргумента валидный путь к файлу и возвращает имя файла (без расширения)
wstring getFileNameWithoutExtension(wstring pathToFile) noexcept;

/* Добавляет в массив путей к файлам, который передан в первом аргументе, все .txt файлы, находящиеся в директории,
путь к которой передан вторым аргументом, и её поддиректориях, либо сам файл, если вторым аргументом передан путь
не к директории, а к файлу */
bool processSourceFileOrDirectory(sourcefiles_info&, wstring path, bool recursive);

// Заносит файл по указанному пути, если он имеет расширение .txt, в список файлов для обработки (нормализации, дедупликации etc)
bool addFileToSourceList(sourcefiles_info&, wstring filePath) noexcept;

/* Получает все входные пути для обработки из аргументов, введённых юзером (и папки, и файлы).
* Возвращает список путей ко всем входным уникальным файлам (включая файлы из директорий и поддиректорий, 
* если параметр рекурсии - true), которые будут обработаны софтом. */
sourcefiles_info getSourceFilesFromUserInput(size_t sourcePathsCount, const char** userSourcePaths, bool checkDirectoriesRecursive) noexcept;

// Считает количество строк, разделённых символами переноса строк, в тексте. Каждая строка должна заканчиваться символом \n
size_t getLinesCountInText(char* bytes) noexcept;

// Возвращает количество байт информации в файле, если файл не найден или к нему нет доступа, возвращает -1
long long getFileSize(wstring pathToFile) noexcept;

// Возвращает количество байт информации в файле, если файл не найден или к нему нет доступа, возвращает -1
long long getFileSize(FILE* filePtr) noexcept;

// Возвращает количество свободной оперативной памяти в байтах
ull getAvailableMemoryInBytes(void) noexcept;

// Возвращает количество максимальной оперативной памяти в байтах (с учетом используемой)
ull getTotalMemoryInBytes(void) noexcept;

// Возвращает процент используемой оперативной памяти от её общего количества
size_t getMemoryUsagePercent(void) noexcept;

// Существует ли что-либо по указанному пути
bool isAnythingExistsByPath(wstring path) noexcept;

// Является ли указанный путь путём к директории
bool isDirectory(wstring path) noexcept;

// Возвращает строку с путём к директории, в которой лежит файл, находящийся по пути filePath
wstring getDirectoryFromFilePath(wstring filePath) noexcept;

// Является ли регулярное выражение (переданное строкой) валидным
bool isValidRegex(string regularExpression) noexcept;

// Возвращает строку, содержащую путь к текущец директории (откуда вызвана программа, исполняемый файл)
wstring getWorkingDirectoryPath() noexcept;

/* Возвращает количество строк в указанном файле(один перенос строки '\n' = одна строка).
* Если файл не открывается или не считывается - возвращает '-1'.
* Если нужно обработать много файлов, то во избежание излишних аллокаций буфера
* можно передавать временные буфер для считывания блоками из файла и его размер в качестве
* параметров функции. Если оставить пустым - будет аллоцироваться и очищаться прямо в функции. 
* Если буфер передан в качестве параметра функции, вызывающий функцию гарантирует,
* что его размер не будет меньше указанного размера буфера в байтах, иначе может произойти
* неожиданное завершение программы с ошибкой. 
* Кроме того, переданный буфер НЕ будет очищен перед возвратом и может быть заполнен всяческим
* мусором, оставшимся от работы.
*/
long long getStringCountInFile(const wstring& filePath, size_t temporaryBufferSizeInBytes = OPTIMAL_DISK_CHUNK_SIZE + 1, char* temporaryInputBuffer = NULL);

/* Обрабатывает путь к итоговому файлу или папке, указанный пользователем. Исходя из значения параметра
 * needMerge решает, требуется ли создавать итоговый файл, или надо просто проверить итоговую директорию.
 * Если пользователь не указал путь, то устанавливается значение пути на дефолтный по указателю:
 * если needMerge = false, то путь по умолчанию - рабочая директория, если needMerge = true, то путь
 * к итоговому файлу передаётся в последнем аргументе, так как он уникален для каждой команды. */
void processDestinationPath(const char** destinationPathPtr, bool needMerge, FILE** resultFile, const char* defaultResultMergedFilePath) noexcept;

/* Проверяет директорию, указанную пользователем как директорию вывода. 
 * Если директории не существует - создаёт её.
 * Если есть какая-то ошибка - например, на месте директории уже существует файл 
 * с таким же именем - завершает работу программы с подходящим кодом ошибки. */
void checkDestinationDirectory(wstring destinationDirectoryPath) noexcept;

/* Считывает переданный файл оптимальными для чтения чанками (с записью чанков во временный буфер).
* Каждый считанный чанк в буфере обрезается по границе последней строки, далее во входном файле идёт отступ назад
* на отрезанный кусок неполной строки, а полученный буфер обрабатывается функцией processChunkBuffer, уникальной
* для каждой команды(у deduplicate будет одна функция, у normalize другая), и итоговые данные после обработки
* входного буфера записываются в итоговый файл (будет ли это общий файл, определяет функция выше уровнем). */
void processStringsInFileByChunks(FILE* inputFile, FILE* resultFile, size_t processChunkBuffer(char*, size_t, char*));

/* Обработка каждого файла из списка путей ко всем файлам, переданным пользователем. Обёртка верхнего уровня
* для функции processStringsInFileByChunks, служит для корректной обработки ситуации со множеством входных файлов
* (тогда как та функция работает исключительно с одним входным и одним выходным).
* Обрабатывает ситуацию, когда пользователю требуется сложить все итоговые строки в один файл, если же нет - 
* создаёт для каждого входного файла свой собственный итоговый с обработанными строками.
* Для каждого конкретного входного файла все действия выполняются с помощью функции 'processStringsInFileByChunks'.
* После полного выполнения функция закрывает все открытые файлы. */
void processAllSourceFiles(sourcefiles_info sourceFilesPaths, bool needMerge, FILE* resultFile, wstring destinationDirectoryPath, wstring resultFilesSuffix, size_t processChunkBuffer(char* inputBuffer, size_t inputBufferLength, char* resultBuffer));

/* Генерирует валидный путь к итоговому файлу и открывает сам файл, используя имя входного файла, 
 * итоговую директорию и суффикс функции, который надо добавлять ко всем обработанным файлам. 
 * Чтобы не было пересечений с другими файлами (из других папок, но с такими же названиями),
 * если у файлов такое же имя, использует так же первый доступный номер (цифру) в конце имени файла.
 * Например, после нормализации файла test.txt и если это первый нормализуемый файл с таким именем,
 * в итоговой директории создастся и откроется файл test_normalized_1.txt. Если функция-обработчик другая,
 * то и суффикс другой, например, после токенизации test.txt в результате будет test_tokenized_1.txt.
 * ВОзвращает указатель на открытый файл в режиме бинарной записи. */
FILE* getResultFilePtr(wstring pathToResultFolder, wstring pathToSourceFile, wstring fileSuffixName);
#endif // !MY_UTILS