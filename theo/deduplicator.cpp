#include "utils.hpp"

// Хранилище для всех хешей уникальных строк
static robin_hood::unordered_flat_set<ull> stringHashes;

// По хешу определяет, была ли уже такая строка, если не было - добавляет её в итоговый буфер и меняет переменную с длиной итогового буфера
static void addStringToDestinationBufferCheckingHash(ull stringHash, char* sourceBuffer, size_t sourceBufferPos, size_t stringStartPosInSourceBuffer, char* destinationBuffer, size_t* destinationBufferStringStartPosPtr);

/* Считывает входной буфер посимвольно, хеширует каждую считанную строку, уникальные записывает в итоговый буфер.
*  Возвращает размер итогового буфера в байтах (чтобы впоследствии записать все данные из него в файл) */
static size_t deduplicateBufferLineByLine(char* buffer, size_t buflen, char* resultBuffer);

// Проверяет, указан ли пользователем путь к исходному файлу, если нет - создает свой из имени входного файла
static FILE* getDeduplicatedResultFilePtr(const char* inputFilePath, const char* destinationFilePath);

// Опции для ввода аргументов вызова программы из cmd, показыаемые пользователю при использовании флага --help или -h
static const char* const usages[] = {
	"theo d [options] [path]",
	NULL,
};

int deduplicate(int argc, const char** argv) {
	const char* destinationFilePath = NULL;
	size_t	linesInOneFile = 0;

	struct argparse_option options[] = {
		OPT_HELP(),
		OPT_GROUP("File options"),
		OPT_STRING('d', "destination", &destinationFilePath, "Path to result file without duplicates (default: '{your_file_name}_dedup.txt'"),
        OPT_GROUP("    Unmarked (positional) argument are considered as path to file that need to be deduplicated. "),
		OPT_END(),
	};
	struct argparse argparse;
	argparse_init(&argparse, options, usages, 0);
	int remainingArgumentsCount = argparse_parse(&argparse, argc, argv);
    if (remainingArgumentsCount < 1) {
        argparse_usage(&argparse);
        return -1;
    }

    const char* inputFilePath = argv[0];

    chrono::steady_clock::time_point begin = chrono::steady_clock::now();
    FILE* inputFile = fopen(inputFilePath, "rb");
    if (inputFile == NULL) {
        cout << "Invalid path to source file: [" << inputFilePath << "]" << endl;
        return ERROR_OPEN_FAILED;
    }

    long long inputFileSizeInBytes = getFileSize(inputFilePath);
    if (inputFileSizeInBytes == -1) {
        cout << "Error: cannot get access to file: [" << inputFilePath << "]" << endl;
        return ERROR_ACCESS_DENIED;
    }
    if (inputFileSizeInBytes == 0) {
        cout << "Error: cannot deduplicate, input file [" << inputFilePath << "] is empty." << endl;
        return 1;
    }
    if (getAvailableMemoryInBytes() < static_cast<ull>(inputFileSizeInBytes)) {
        cout << "Warning: there may not be enough RAM to remove duplicates (if there are few duplicates in specified file). The program can crash at any time if the memory limit is exceeded. Continue? (y/n): ";
        char answer;
        cin >> answer;
        if(answer != 'y') return WN_OUT_OF_MEMORY;
    }
    

    FILE* resultFile = getDeduplicatedResultFilePtr(inputFilePath, destinationFilePath);
    processFileByChunks(inputFile, resultFile, deduplicateBufferLineByLine);
    fclose(resultFile);

    chrono::steady_clock::time_point end = chrono::steady_clock::now();
    cout << "\nFile deduplicated successfully! Execution time: " << chrono::duration_cast<std::chrono::seconds>(end - begin).count() << "[s]\n" << endl;

	return ERROR_SUCCESS;
}


static void addStringToDestinationBufferCheckingHash(ull stringHash, char* sourceBuffer, size_t sourceBufferPos, size_t stringStartPosInSourceBuffer, char* destinationBuffer, size_t* destinationBufferStringStartPosPtr) {
    if (stringHashes.contains(stringHash)) return; // Если хеш строки уже присутствует в таблице, добавлять его снова не надо

    stringHashes.insert(stringHash); // Добавляем в хеш-таблицу хеш строки для последующих проверок
    // Вычисляем длину текущей строки (добавляем единицу, так как последний символ (перенос строки) надо оставить
    size_t currentStringLength = sourceBufferPos - stringStartPosInSourceBuffer + 1;
    // Сохраняем строку в итоговый буфер, копируя напрямую из изначального буфера
    memcpy(&destinationBuffer[*destinationBufferStringStartPosPtr], &sourceBuffer[stringStartPosInSourceBuffer], currentStringLength);

    // Вычисляем позицию конца в итоговом буфере и изменяем значение переменной, указывающей на начало следующей строки
    // в итоговом буфере, по указателю, чтобы при новом вызове функции сразу было верное значение
    *destinationBufferStringStartPosPtr += currentStringLength;
}


static size_t deduplicateBufferLineByLine(char* buffer, size_t buflen, char* resultBuffer) {
    // Изначальное оптимальное значения для хеширования символов - 5381 (почему так - смотреть http://www.cse.yorku.ca/~oz/hash.html)
    ull hashStartValue = 5381, currentHash = hashStartValue;

    // Индекс начала текущей строки в буфере со входными данными, чтобы впоследствии можно было скопировать из него всю строку
    size_t currentStringStartPosInInputBufferIdx = 0;
    
    // Длина итогового буфера с валидными данными, которые надо полностью записать в итоговый файл
    size_t resultBufferLength = 0;

    // Пробегаемся по каждому символу буфера
    for (size_t bufferPos = 0; bufferPos < buflen; bufferPos++) {
        /* Если текущий символ - перенос каретки, то пропускаем его без обработки и добавления в хеш, 
        *  поскольку это незначащий символ и на представление строки не влияет, а используется как вспомогательный
        *  для разделителя строк в Windows (разделение строк там не '\n', а '\r\n') */
        if (buffer[bufferPos] == '\r') continue;
        // Если это символ переноса строки, надо строку обработать, хеш проверить, и если его не было, строку записать
        if (buffer[bufferPos] == '\n') {
            // Проверяем хеш и записываем строку в итоговый буфер, а также меняем указатель конца итогового буфера, если строка уникальна
            addStringToDestinationBufferCheckingHash(currentHash, buffer, bufferPos, currentStringStartPosInInputBufferIdx, resultBuffer, &resultBufferLength);
            // Устанавливаем корректное значение начала новой строки во входном буфере, который мы читаем
            currentStringStartPosInInputBufferIdx = bufferPos + 1;
            // Сбрасываем хеш до значения инициализации, так как высчитывать хеш переноса строки нет смысла
            currentHash = hashStartValue;
            continue;
        }
        // Высчитываем хеш строки, внося в него влияние текущего символа (меняем значение хеша в зависимости от символа)
        currentHash = ((currentHash << 5) + currentHash) + buffer[bufferPos];
    }
    
    return resultBufferLength;
}

static FILE* getDeduplicatedResultFilePtr(const char* inputFilePath, const char* destinationFilePath) {
    string destString = getFileNameWithoutExtension(inputFilePath) + "_dedup.txt";
    const char* destPath = destinationFilePath != NULL ? destinationFilePath : destString.c_str();

    FILE* resultFile = fopen(destPath, "wb+");
    if (resultFile == NULL) {
        cout << "Error: cannot open result file [" << destPath << " in write mode" << endl;
        exit(ERROR_OPEN_FAILED);
    }
    return resultFile;
}