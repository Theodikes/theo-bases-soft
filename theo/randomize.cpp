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

/* Считывает все строки из файла и сохраняет и возвращает массив, в котором находятся указатели
 * на все строки (в каждой строке удалён символ переноса строки '\n').
 * В переменную resultStringsCount после выполнения записывается количество строк в файле - это
 * так же размер итогового массива.
 * В переменную fileContentBuf записывается указатель на начало буфера, в котором находятся
 * все считанные из файла байты, для последующей его очистки */
static char** getAllStringsFromFile(FILE* inputFile, size_t inputFileSize, size_t* resultStringsCount, char** fileContentBuf);

// Запись всех перемешанных строк из по массиву указателей строк в выходной файл.
static int writeShuffledStringsToFile(FILE* resultFile, char** allStrings, size_t stringsCount, size_t inputFileSize);

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
        OPT_INTEGER(0, "memory", &memoryUsageMaxPercent, "Maximum percentage of RAM usage. Only number (whout percent symbol).\n\t\t\t      After reaching limit, shuffling continues on disk (default - 90%)"),
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
    /* Умножаем размер файла на два, поскольку будет храниться две копии всего файла:
     * во входном буфере и в итоговом буфере, для быстрого считывания и записи */
    ull memoryInBytesToStoreAllInputFileStrings = inputFileSizeInBytes * 2 + memoryForPointers;
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
        if (retCode != ERROR_SUCCESS) {
            fs::remove(destinationFilePath);
            exit(retCode);
        }
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

static char** getAllStringsFromFile(FILE* inputFile, size_t inputFileSize, size_t* resultStringsCount, char** fileContentBuf) {
    // Выделяем буфер под хранение всех байтов из входного файла
    char* allFileContent = new char[inputFileSize + 1];
    if (allFileContent == NULL) {
        wcout << "Error: not menough memory, cannot allocate buffer to store strings from input file";
        return NULL;
    }
    // Пытаемся считать весь входной файл в выделенный выше буфер за один раз
    size_t bytesReaded = fread(allFileContent, sizeof(char), inputFileSize, inputFile);
    if (bytesReaded != inputFileSize) {
        wcout << "Cannot read all input file" << endl;
        return NULL;
    }

    // Примерное ожидаемое число строк в файле - размер файла, делённый на среднюю длину строки
    size_t expectedStringsCount = inputFileSize / AVERAGE_STRING_LEGTH_IN_FILE;
    /* Создаем массив указателей на строки, каждый элемент будет указателем на начало новой
     * строки в буфере, где находится полностью считанный файл */
    char** allStrings = (char**)malloc((expectedStringsCount + 1) * sizeof(char*));

    size_t currentStringNumber = 0;
    for (size_t pos = 0; pos < inputFileSize; pos++) {
        /* Добавляем указатель на начало строки в итоговый массив строк. Началом строки символ
         * считается в том случае, если он либо первый во всем буфере (входном файле), либо если
         * перед ним символ конца предыдущей строки */
        if (pos == 0 or allFileContent[pos - 1] == '\0') {
            /* Если в файле много коротких строк, может случиться такое, что строк будет больше,
             * чем ожидалось заранее, и в массив новые не влезут. В таком случае требуется
             * реаллоцировать массив, увеличив его вместительность ровно на половину от текущего
             * размера */
            if (currentStringNumber >= expectedStringsCount) {
                size_t newExpectedStringsCount = (size_t)ceil(expectedStringsCount * 1.5);
                char** reallocated = (char**)realloc(allStrings, (newExpectedStringsCount + 1) * sizeof(char*));
                if (reallocated == NULL) {
                    wcout << "Error: cannot allocate more memory to store short strings from input file" << endl;
                    delete[] allFileContent;
                    free(allStrings);
                    return NULL;
                }
                allStrings = reallocated;
                expectedStringsCount = newExpectedStringsCount;
            }
            /* Добавляем указатель на начало текущей строки в буфере, в котором находятся
             * все байты из входного файла */
            allStrings[currentStringNumber++] = &allFileContent[pos];
        }
        /* Заменяем символ переноса строки на символ конца строки, чтобы следующий символ
         * был расценён как начало следующей строки. Не добавляем следующий символ
         * как начало следующей строки сразу же, чтобы не делать лишних проверок
         * на то, не конец ли это буфера, и чтобы корректно обработать начало первой строки
         * во всём файле */
        if (allFileContent[pos] == '\n') allFileContent[pos] = '\0';
    }
    /* Добавляем в конец буфера с содержимым файла, то есть, в конец последней считанной строки,
    * символ завершения строки, если она не оканчивалась переносом строки и он не был установлен
    * ранее, при удалении символа переноса */
    if (allFileContent[inputFileSize - 1] != '\0') allFileContent[bytesReaded] = '\0';
    
    // Возврат значений путем присваивания их по указателям, переданным в функцию
    *fileContentBuf = allFileContent;
    *resultStringsCount = currentStringNumber;
    return allStrings;
}

static int writeShuffledStringsToFile(FILE* resultFile, char** allStrings, size_t stringsCount, size_t inputFileSize) {
    /* Размер inputFileSize, поскольку после перемешивания весь контент 
     * из входного файла должен оказаться в итоговом, ничего добавляться или удаляться не будет */
    char* resultBuffer = new char[inputFileSize + 2];
    if (resultBuffer == NULL) {
        cout << "Error: not enough memory to allocate buffer to write shuffled strings to result file" << endl;
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    // Пробегаемся по всему массиву перемешанными строками и записываем их в итоговый буфер
    size_t currentPosInBuffer = 0;
    for (size_t stringNumber = 0; stringNumber < stringsCount; stringNumber++) {
        size_t len = strlen(allStrings[stringNumber]);
        memcpy(&resultBuffer[currentPosInBuffer], allStrings[stringNumber], len);
        currentPosInBuffer += len;
        /* В конце каждой строки вместо символа завершения строки вставляем символ
         * переноса строки, так как эти символы были удалены при считывании буфера
         * из входного файла и разбиении его на строки */
        resultBuffer[currentPosInBuffer++] = '\n';
    }

    /* Проверяем длину выходного файла.В нормальных обстоятельствах длина должна либо
     * совпадать с длиной входного файла, либо быть больше на 1, поскольку
     * в конец последней строки всегда добавляется '\n', даже если его там не было
     * во входном файле */
    if (currentPosInBuffer != inputFileSize and currentPosInBuffer != inputFileSize + 1) {
        cout << "Error: wrong output buffer length" << endl;
        delete[] resultBuffer;
        return ERROR_NDIS_INVALID_LENGTH;
    }

    size_t bytesWrited = fwrite(resultBuffer, sizeof(char), currentPosInBuffer, resultFile);
    if (bytesWrited != currentPosInBuffer) {
        cout << "Error: cannot write all shuffled strings to result file, write failure" << endl;
        delete[] resultBuffer;
        return ERROR_WRITE_FAULT;
    }

    delete[] resultBuffer;
    
    return ERROR_SUCCESS;
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
    // Указатель на массив со всеми байтами из входного файла, хранит все строки
    char* allFileContent = NULL; 
    // Массив указателей на строки (на начало каждой строки строк) в allFileContent
    char** allStrings = NULL;
    // Количество строк в массиве allStrings и, соответственно, во входном файле
    size_t stringsInFileCount = 0;

    // Закрываем входной и итоговый файл даже в случае неуспешного выполнения функции
    auto cleanup = [&]() {
        fclose(inputFile);
        fclose(outputFile);
        if (allFileContent != NULL) free(allFileContent);
        if (allStrings != NULL) free(allStrings);
    };

    allStrings = getAllStringsFromFile(inputFile, inputFileSize, &stringsInFileCount, &allFileContent);
    if (allStrings == NULL) {
        cout << "Error: cannot read all strings from file and store it in array" << endl;
        cleanup();
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    randomShuffleArrayInplace(allStrings, stringsInFileCount);

    int retCode = writeShuffledStringsToFile(outputFile, allStrings, stringsInFileCount, inputFileSize);
    if (retCode != ERROR_SUCCESS) {
        cleanup();
        return retCode;
    }

    cleanup();

    return ERROR_SUCCESS;
}

static void freeStringsArray(char** array, size_t arrayLength) {
    for (size_t i = 0; i < arrayLength; i++) free(array[i]);
    free(array);
}