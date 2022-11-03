#include "utils.hpp"

static const char* const usages[] = {
    "theo d [options] path",
    NULL,
};

/* Перемешивает строки из входного файла в случайном порядке и записывает в итоговый файл
* Оба файла после окончания работы функции (и успешном, и ошибочном) закрываются.
* Если параметр deallocate имеет значение false, то временный массив, в котором
* перемешивались строки из файла, не очищается после завершения функции */
static int shuffleFileInRAM(FILE* inputFile, FILE* outputFile, ull inputFileSizeInBytes, bool deallocate = true);

static char** getAllStringsFromFile(FILE* inputFile, size_t inputFileSize, size_t* resultStringsCount);

// Перемешивает элементы в массиве случайным образом, изменяя массив внутри функции
static void randomShuffleArrayInplace(char** array, size_t arrayLength);

/* Разбивает входной файл на несколько небольших, которые можно перемешать в оперативной памяти.
 * Возвращает путь к новосозданной временной директории, где находятся ТОЛЬКО эти файлы */
static wstring splitInputFileIntoTemporaryDirectory(const wstring& inputFilePath, size_t splittedFilesCount) noexcept;

/* Объединяет все переданные временные файлы в один итоговый.
 * Если возникла ошибка - завершает работу программы, перед этим вызывая cleanupFunction */
static void mergeAllShuffledTempfilesIntoResultFile(const vector<wstring>& shuffledTempfilesPaths, wstring resultFilePath, auto& cleanupFunction);

/* Перемешивает все файлы в указанной директории и возвращает список путей к перемешанным файлам
 * Если возникла ошибка - завершает работу программы, перед этим вызывая cleanupFunction */
static vector<wstring> shuffleTempFiles(const wstring& tempDirectory, auto& cleanupFunction);

/* Корректно освобождает память из под двумерного массива строк, 
 * если строки тоже выделены в памяти с помощью malloc / calloc / realloc */
static void freeStringsArray(char** array, size_t arrayLength);

int randomize(int argc, const char** argv) {
    int memoryUsageMaxPercent = 90;
    const char* destinationPath = NULL;
    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_GROUP("Basic options"),
        OPT_INTEGER(0, "memory", &memoryUsageMaxPercent, "Maximum percentage of RAM usage. Only number (whout percent symbol).\n\t\t\t      After reaching limit, deduplication continues on disk (default - 90%)"),
        OPT_GROUP("File options"),
        OPT_STRING('d', "destination", &destinationPath, "absolute or relative path to result file(default: current directory)"),
        OPT_GROUP("Unmarked (positional) argument will be considered as path to input file"),
        OPT_END(),
    };
    struct argparse argparse;
    argparse_init(&argparse, options, usages, 0);
    int remainingArgumentsCount = argparse_parse(&argparse, argc, argv);
    if (remainingArgumentsCount < 1) {
        argparse_usage(&argparse);
        return -1;
    }

    // Засекаем время выполнения программы
    chrono::steady_clock::time_point begin = chrono::steady_clock::now();

    if (remainingArgumentsCount != 1) {
        wcout << "Error: invalid positional arguments count. Maximum allowed one positional argument (path to input file)" << endl;
        return -1;
    }

    if (memoryUsageMaxPercent < 1 or memoryUsageMaxPercent > 100) {
        cout << "Invalid '--memory' parameter value, it must be lower than 100 and higher than 0" << endl;
        return ERROR_INVALID_PARAMETER;
    }
    
	wstring inputFilePath = toWstring(argv[0]);
    FILE* inputFile = fileOpen(inputFilePath, "rb");
    if (inputFile == NULL) {
        wcout << "Error: cannot open input file [" << inputFilePath << "]. Enetered path is invalid or cannot get access due to security reason." << endl;
        return -1;
    }

    wstring destinationFilePath = destinationPath != NULL ? toWstring(destinationPath) : getFileNameWithoutExtension(inputFilePath) + L"_randomized.txt";
    if (fs::exists(destinationFilePath)) {
        wcout << "Error: something already exists on destination file path [" << destinationFilePath << "]" << endl;
        return -1;
    }

    ull inputFileSizeInBytes = getFileSize(inputFile);

    /* Сколько процентов от общего объема оперативной памяти, занятой строками, считанными
     * из файла, могут потребовать указатели на эти строки. По умолчанию - 50%.
     * Для примера: файл весит 100 мегабайт, средняя длина строки - чуть меньше 20,
     * одна строка занимает ~20 байт в памяти, один указатель на строку 
     * (ячейка массива со всеми строками) - 8 байт. Получается, примерно 50% от памяти,
     * выделяемой на сами строки, нужны для указателей на эти строки.
     */
    size_t memoryPercentForPointers = static_cast<size_t>(ceil(100 / AVERAGE_STRING_LEGTH_IN_FILE * sizeof(char*)));
    ull memoryForPointers = inputFileSizeInBytes / 100 * memoryPercentForPointers;
    ull memoryInBytesToStoreAllInputFileStrings = inputFileSizeInBytes + memoryForPointers;
    ull availableMemoryInBytes = getAvailableMemoryInBytes();
    ull totalMemoryInBytes = getTotalMemoryInBytes();

    /* Процент оперативной памяти(от общей памяти), которую нельзя использовать
     * во вперям выполнения программы (она должна оставаться свободной для пользователя).
     * Добавляем ещё 5%, потому что это 5% памяти на накладные расходы программы, которые
     * будут задействованы во время работы в любом случае (временные буферы и так далее). */
    size_t prohibitedToUseMemoryPercent = 100 - memoryUsageMaxPercent + 5;
    size_t prohibitedToUseMemoryInBytes = static_cast<size_t>(ceil(totalMemoryInBytes / 100 * prohibitedToUseMemoryPercent));

    if (availableMemoryInBytes < prohibitedToUseMemoryInBytes) {
        wcout << "Error: not enough RAM. Change `--memory` start parameter value or close other processes on your PC";
        exit(ERROR_OUTOFMEMORY);
    }

    FILE* resultFile = fileOpen(destinationFilePath, "wb+");
    if (resultFile == NULL) {
        wcout << "Error: cannot create result file - no access due to security reasons" << endl;
        return -1;
    }

    /* Если свободной памяти достаточно, чтобы хранить все строки из входного файла,
     * рандомно перемешиваем строки прямо в оперативной памяти и записываем в итоговый.
     * Если памяти недостаточно, разделяем входной файл и перемешиваем частями, 
     * а затем объединяем части в рандомном порядке в итоговый файл. */
    if (availableMemoryInBytes - prohibitedToUseMemoryInBytes > memoryInBytesToStoreAllInputFileStrings) {
        /* Перемешиваем строки из файла прямо в оперативке, последний параметр false,
        * поскольку деаллоцировать массив не надо - память сама очистится после завершения 
        * программы, а ручная деаллокация занимает очень много времени (почти 50% от общего) */
        int retCode = shuffleFileInRAM(inputFile, resultFile, inputFileSizeInBytes, false);
        if (retCode != ERROR_SUCCESS) fs::remove(destinationFilePath);
    }
    else {
        /* Количество частей, на которые надо разделить файл, чтобы каждая часть
        * по отдельности помещалась в оперативку без проблем */
        size_t parts = static_cast<size_t>(ceil(memoryInBytesToStoreAllInputFileStrings / (availableMemoryInBytes - prohibitedToUseMemoryInBytes))) * 2;

        /* Закрываем все открытые файлы(input и result), поскольку взаимодействие с ними
        * будет производиться не напрямую из функции, а вызовом других команд theo через
        * _wsystem, например, `theo split` для разделения входного файла */
        _fcloseall();

        wstring tempDirectory = splitInputFileIntoTemporaryDirectory(inputFilePath, parts);

        /* Перед выходом(даже в случае ошибки) закрываем все открытые файлы и удаляем
         * временную директорию со всеми находящимися в ней файлами, чтобы не засорять диск */
        auto cleanup = [&]() {
            _fcloseall();
            fs::remove_all(tempDirectory);
        };

        vector<wstring> shuffledTempFilesPaths = shuffleTempFiles(tempDirectory, cleanup);
        mergeAllShuffledTempfilesIntoResultFile(shuffledTempFilesPaths, destinationFilePath, cleanup);
        cleanup();
    }

    chrono::steady_clock::time_point end = chrono::steady_clock::now();
    wcout << "\nFile random-shuffled successfully! Execution time: " << chrono::duration_cast<std::chrono::seconds>(end - begin).count() << "[s]\n" << endl;

    return ERROR_SUCCESS;
}


static vector<wstring> shuffleTempFiles(const wstring& tempDirectory, auto& cleanupFunction) {
    // Список путей ко всем перемешанным временным файлам
    vector<wstring> shuffledTempFilesPaths;

    /* Перемешиваем абсолютно все файлы в директории, поскольку в ней должны находиться
    * исключительно части основного исходного файла */
    for (const auto& entry : fs::directory_iterator(tempDirectory)) {
        wstring pathToCurrentPartOfInputFile = entry.path().wstring();
        wstring pathToCurrentResultFile = (fs::path(tempDirectory) / (getFileNameWithoutExtension(pathToCurrentPartOfInputFile) + L"_shuffled.txt")).wstring();

        FILE* tempInputFile = fileOpen(pathToCurrentPartOfInputFile, "rb");
        FILE* tempResultFile = fileOpen(pathToCurrentResultFile, "wb+");
        if (tempInputFile == NULL) {
            wcout << "Error: cannot open temp shuffling input file [" << pathToCurrentPartOfInputFile << "]" << endl;
            cleanupFunction();
            exit(ERROR_FILE_CORRUPT);
        }
        if (tempResultFile == NULL) {
            wcout << "Error: cannot open temp shuffling result file [" << pathToCurrentResultFile << "]" << endl;
            cleanupFunction();
            exit(ERROR_FILE_PROTECTED_UNDER_DPL);
        }

        // Перемешиваем текущую часть основного файла
        ull currentTempInputFileSizeInBytes = getFileSize(pathToCurrentPartOfInputFile);
        int retCode = shuffleFileInRAM(tempInputFile, tempResultFile, currentTempInputFileSizeInBytes);
        if (retCode != ERROR_SUCCESS) {
            wcout << "Error: cannot shuffle part of main input file, tempfile [" << pathToCurrentPartOfInputFile << "]" << endl;
            cleanupFunction();
            exit(retCode);
        }

        // Добавляем путь к успешно перемешанному итоговому файлу в список перемешанных
        shuffledTempFilesPaths.push_back(pathToCurrentResultFile);
    }

    // Перемешиваем имена файлов в случайном порядке, чтобы потом их соединять не по очереди
    Xoshiro256PlusPlus randomGenerator(time(NULL));
    shuffle(shuffledTempFilesPaths.begin(), shuffledTempFilesPaths.end(), randomGenerator);

    return shuffledTempFilesPaths;
}

static void mergeAllShuffledTempfilesIntoResultFile(const vector<wstring>& shuffledTempfilesPaths, wstring resultFilePath, auto& cleanupFunction) {
    wstring mergeCommand = L"theo merge -d \"" + resultFilePath + L"\"";
    // Добавляем все файлы в команду merge, так как они уже перемешаны, добавляем по очереди
    for (const wstring& pathToTempShuffledFile : shuffledTempfilesPaths) {
        mergeCommand += L" \"" + fs::absolute(pathToTempShuffledFile).wstring() + L'"';
    }
    // Блокируем вывод выполнения merge в консоль пользователя
    mergeCommand += SUPPRESS_CMD_WINDOWS_OUTPUT_END;

    int execRetCode = _wsystem(mergeCommand.c_str());
    if (execRetCode != ERROR_SUCCESS) {
        wcout << "Error: cannot merge shuffled parts into one result file [" << resultFilePath << "]" << endl;
        cleanupFunction();
        exit(execRetCode);
    }
}


static wstring splitInputFileIntoTemporaryDirectory(const wstring& inputFilePath, size_t splittedFilesCount) noexcept {

    // Создаём временную директорию с уникальным именем в temp-папке на компьютере
    wstring tempDirectoryPath = (fs::temp_directory_path() / tmpnam(NULL)).wstring();
    try {
        fs::create_directory(tempDirectoryPath);
    }
    catch (...) {
        wcout << "Error: cannot create temporary folder [" << tempDirectoryPath << "]" << endl;
        exit(ERROR_DIRECTORY_NOT_SUPPORTED);
    }

    /* Просто запускаем `theo split` для разделения файла на необходимое количество частей,
     * чтобы не писать повторяющийся код. Весь вывод в консоль от этой команды блокируем */
    wstring splitCommand = L"theo split -p " + to_wstring(splittedFilesCount) + L" -d \"" + tempDirectoryPath + L"\" \"" + inputFilePath + L'"' + SUPPRESS_CMD_WINDOWS_OUTPUT_END;
    int execRetCode = _wsystem(splitCommand.c_str());

    /* Отлавливаем статус завершения выполнения команды, если файл разбит успешно - он будет
     * равен нулю (ERROR_SUCCESS), если выпала ошибка - выходим */
    if (execRetCode != ERROR_SUCCESS) {
        wcout << "Error: cannot split input file to temporary folder [" << tempDirectoryPath << "]" << endl;
        fs::remove(tempDirectoryPath);
        exit(ERROR_CREATE_FAILED);
    }

    return tempDirectoryPath;
}

static char** getAllStringsFromFile(FILE* inputFile, size_t inputFileSize, size_t* resultStringsCount) {
    /* Считаем, что в одной строке примерно 20 символов, поэтому количество строк равно
    * количеству байт (символов) в файле, поделённому на 20 */
    size_t expectedStringsCount = inputFileSize / 20;
    char** allStrings = (char**)malloc((expectedStringsCount + 1) * sizeof(char*));
    size_t maxStringLength = min(OPTIMAL_DISK_CHUNK_SIZE, inputFileSize);
    char* tempBuf = new char[maxStringLength + 1];
    size_t currentStringNumber = 0;
    auto cleanup = [&]() {
        delete[] tempBuf;
        freeStringsArray(allStrings, currentStringNumber);
    };
    while (not feof(inputFile)) {
        char* tempString = fgets(tempBuf, maxStringLength, inputFile);
        if (tempString == NULL) {
            if (feof(inputFile)) break;
            cleanup();
            return NULL;
        }
        size_t stringLength = strlen(tempString);

        // Удалем все переносы строки и пробелы с конца строки, оставляя только \n
        for (size_t i = stringLength - 1; isspace(tempString[stringLength - 1]) and stringLength > 1; i--) stringLength--;
        tempString[stringLength++] = '\n';
        tempString[stringLength] = '\0';

        char* string = (char*) malloc((stringLength + 1) * sizeof(char));
        if (string == NULL) {
            wcout << "Error: not enough memory. Cannot allocate place for string in RAM" << endl;
            cleanup();
            return NULL;
        }
        memcpy(string, tempString, stringLength + 1);
        if (currentStringNumber > expectedStringsCount) {
            size_t newExpectedStringsCount = (size_t) ceil(expectedStringsCount * 1.5);
            char** reallocated = (char**)realloc(allStrings, (newExpectedStringsCount + 1) * sizeof(char*));
            if (reallocated == NULL) {
                cleanup();
                return NULL;
            }
            allStrings = reallocated;
            expectedStringsCount = newExpectedStringsCount;

        }
        allStrings[currentStringNumber++] = string;
    }

    delete[] tempBuf;
    *resultStringsCount = currentStringNumber;
    return allStrings;
}


static void randomShuffleArrayInplace(char** array, size_t arrayLength) {
    const ull seed = time(NULL);
    Xoshiro256PlusPlus randomGenerator(seed);
    uniform_int_distribution<ull> distribution(0, arrayLength - 1);

    // Меняем каждый элемент массива с другим случайным элементом в массиве
    for (size_t elementNumber = 0; elementNumber < arrayLength; elementNumber++) {
        swap(array[elementNumber], array[distribution(randomGenerator)]);
    }
}


static int shuffleFileInRAM(FILE* inputFile, FILE* outputFile, ull inputFileSize, bool deallocate) {
    // Закрываем входной и итоговый файл даже в случае неуспешного выполнения функции
    auto cleanup = [&]() {
        fclose(inputFile);
        fclose(outputFile);
    };

    size_t stringsInFileCount = 0;
    char** allStrings = getAllStringsFromFile(inputFile, inputFileSize, &stringsInFileCount);
    if (allStrings == NULL) {
        cout << "Error: cannot read all strings from file and store it in array" << endl;
        cleanup();
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    randomShuffleArrayInplace(allStrings, stringsInFileCount);

    for (size_t stringNumber = 0; stringNumber < stringsInFileCount; stringNumber++) {
        fputs(allStrings[stringNumber], outputFile);
    }

    if(deallocate) freeStringsArray(allStrings, stringsInFileCount);
    cleanup();

    return ERROR_SUCCESS;
}

static void freeStringsArray(char** array, size_t arrayLength) {
    for (size_t i = 0; i < arrayLength; i++) free(array[i]);
    free(array);
}