﻿#include "utils.hpp"

// Хранилище для всех хешей уникальных строк
static robin_hood::unordered_flat_set<ull> stringHashes;


// По хешу определяет, была ли уже такая строка, если не было - добавляет её в итоговый буфер и меняет переменную с длиной итогового буфера
void addStringToDestinationBufferCheckingHash(ull stringHash, char* sourceBuffer, size_t sourceBufferPos, size_t stringStartPosInSourceBuffer, char* destinationBuffer, size_t* destinationBufferStringStartPosPtr) {
    if (stringHashes.contains(stringHash)) return; // Если хеш строки уже присутствует в таблице, добавлять его снова не надо

    stringHashes.insert(stringHash); // Добавляем в хеш-таблицу хеш строки для последующих проверок
    size_t currentStringLength = sourceBufferPos - stringStartPosInSourceBuffer; // Вычисляем длину текущей строки
    // Сохраняем строку в итоговый буфер, копируя напрямую из изначального буфера
    memcpy(&destinationBuffer[*destinationBufferStringStartPosPtr], &sourceBuffer[stringStartPosInSourceBuffer], currentStringLength);
    
    // Вычисляем позицию конца в итоговом буфере и изменяем значение переменной, указывающей на начало следующей строки 
    // в итоговом буфере, по указателю, чтобы при новом вызове функции сразу было верное значение
    *destinationBufferStringStartPosPtr += currentStringLength;
}

/* Считывает входной буфер посимвольно, хеширует каждую считанную строку, уникальные записывает в итоговый буфер
Если остался незаконченный кусок строки из входного файла в буфере, переносит его в начало буфера и возвращает индекс
конца этой строки в изменённом входном буфере. 
Так же, по ходу добавления уникальных строк в итоговый буфер изменяет по указателю его длину в байтах. По окончанию 
работы функции в переменной, на которую указывает resultBufferLengthPtr, находится актуальная длина итогового буфера */
size_t processBufferLineByLine(char* buffer, size_t buflen, char* resultBuffer, size_t* resultBufferLengthPtr, bool hasEof) {
    // Изначальное оптимальное значения для хеширования символов - 5381 (почему так - смотреть http://www.cse.yorku.ca/~oz/hash.html)
    ull hashStartValue = 5381, currentHash = hashStartValue;
    // Индекс начала текущей строки в буфере со входными данными, чтобы впоследствии можно было скопировать из него всю строку
    size_t currentStringStartPosInInputBufferIdx = 0;
    // Пробегаемся по каждому символу буфера
    for (size_t bufferPos = 0; bufferPos < buflen; bufferPos++) {
        /* Если код текущего символа равен 13 (символ - '\r', перенос каретки), то пропускаем его без обработки и добавления в хеш,
        * поскольку это незначащий символ и на представление строки не влияет, а используется как вспомогательный для разделителя
        * строк в Windows (разделение строк там не '\n', а '\r\n') */
        if (buffer[bufferPos] == 13) continue;
        // Если это символ переноса строки, надо строку обработать, хеш проверить и если его ещё не было, строку записать
        if (buffer[bufferPos] == 10) {
            // Проверяем зеш и записываем строку в итоговый буфер, а также меняем указатель конца итогового буфера, если строка уникальна
            addStringToDestinationBufferCheckingHash(currentHash, buffer, bufferPos, currentStringStartPosInInputBufferIdx, resultBuffer, resultBufferLengthPtr);
            // Устанавливаем корректное значение начала новой строки во входном буфере, который мы читаем
            currentStringStartPosInInputBufferIdx = bufferPos + 1;
            // Сбрасываем хеш до значения инициализации, так как высчитывать хеш переноса строки нет смысла
            currentHash = hashStartValue;
            continue;
        }
        // Высчитываем хеш строки, внося в него влияние текущего символа (видоизменяем значение хеша в зависимости от символа)
        currentHash = ((currentHash << 5) + currentHash) + buffer[bufferPos];
    }
    //  Если символ переноса последний в буфере - строка уже была добавлена в итоговый буфер ранее, её не рассматриваем
    if (buffer[buflen - 1] == '\n') return 0;
    // Если конец файла добавляем текущую строку в итоговый буфер (поскольку это точно полная строка, даже если нет символа переноса)
    if (hasEof) {
        addStringToDestinationBufferCheckingHash(currentHash, buffer, buflen, currentStringStartPosInInputBufferIdx, resultBuffer, resultBufferLengthPtr);
        return 0;
    }
    /* Если не конец файла, и при этом во входном буфере остался кусок строки - возвращаем её длину, чтобы в передвинуть указатель
    в файле на нужное место и в следующий раз начать чтение с этой же строки*/
    return  buflen - currentStringStartPosInInputBufferIdx;
}

// Опции для ввода аргументов вызова программы из cmd, показыаемые пользователю при использовании флага --help или -h
static const char* const usages[] = {
	"theo d [options]",
	NULL,
};

int deduplicate(int argc, const char** argv) {
	const char* inputFilePath = "merged.txt";
	const char* destinationFilePath = "merged_dedup.txt";
	size_t	linesInOneFile = 0;

	struct argparse_option options[] = {
		OPT_HELP(),
		OPT_GROUP("Basic options"),
		OPT_STRING('s', "source", &inputFilePath, "Path to file from which duplicates will be removed (default: 'merged.txt')"),
		OPT_STRING('d', "destination", &destinationFilePath, "Path to result file without duplicates (default: 'merged_dedup.txt'"),
		OPT_END(),
	};
	struct argparse argparse;
	argparse_init(&argparse, options, usages, 0);
	argparse_parse(&argparse, argc, argv);

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
    if (getAvailableMemoryInBytes() / 10 * 8 < static_cast<ull>(inputFileSizeInBytes)) {
        cout << "Not enough memory to process this file: [" << inputFilePath << "]" << endl;
        return WN_OUT_OF_MEMORY;
    }

    FILE* resultFile = fopen(destinationFilePath, "wb+");
    if (resultFile == NULL) {
        cout << "Error: cannot open result file [" << destinationFilePath << " in write mode" << endl;
        return ERROR_OPEN_FAILED;
    }

    /* Вычисляем размер временного буфера для хранения и обработки байтовых данных с файлов.
    * Обычно буфер должен иметь оптимальный размер для работы с диском (64 мегабайта, степень двойки в байтах),
    * однако, если сам изначальный файл для разбиения меньше, используется его размер, чтобы не занимать лишнюю оперативку */
    ull fileSize = getFileSize(inputFilePath);
    size_t countBytesToReadInOneIteration = min(OPTIMAL_DISK_CHUNK_SIZE, fileSize);

    /* Буфер, в который будет считываться информация с диска(со входящего файла) и в котором будут считаться строки.
    * Аллоцируется в куче, потому что в стеке может быть ограничение на размер памяти */
    char* inputBuffer = new char[countBytesToReadInOneIteration];
    char* resultBuffer = new char[countBytesToReadInOneIteration];
    if (inputBuffer == NULL or resultBuffer == NULL) {
        cout << "Error: annot allocate buffer of " << countBytesToReadInOneIteration * 2 << "bytes" << endl;
        exit(1);
    }

    // Текущее количество памяти в байтах, доступной на запись из файла, во входном буфере.
    size_t currentInputBufferAvailableWriteSize = countBytesToReadInOneIteration;
    while (!feof(inputFile)) {
        size_t resultBufferLength = 0; // Длина итогового буфера с уникальными строками в байтах
        /* Считываем нужное количество байт из входного файла в буфер, количество реально считаных байт записывается 
        *  в переменную, нужную на случай, если файл закончился, и реально считалось меньше байт, чем предполагалось */
        size_t bytesReadedCount = fread(inputBuffer, sizeof(char), currentInputBufferAvailableWriteSize, inputFile);
        /* Читаем буфер посимвольно, генерируем хеши для строк, проверяем на уникальность, записываем уникальные строки
        * последовательно в итоговый буфер и получаем размер отступа назад для чтения в следующий раз (если буфер был
        * обрезан на середине какой-то строки, отступ ненулевой, чтобы прочесть строку полностью)*/
        size_t inputBufferWriteOffset = processBufferLineByLine(inputBuffer, bytesReadedCount, resultBuffer, &resultBufferLength, feof(inputFile));
        // Записываем данные из итогового буфера с уникальными строками в файл вывода
        fwrite(resultBuffer, sizeof(char), resultBufferLength, resultFile);
        /* Если в этом считанном входном буфере осталась незаконченная строка, обрезанная при считывании побайтово, делаем отступ
         * в файле назад на длину оставшегося в буфере неполного куска строки, чтобы при следующем fread обработать её полностью */
        if (inputBufferWriteOffset) fseek(inputFile, -static_cast<long>(inputBufferWriteOffset), SEEK_CUR);
    }
    // Освобождение памяти буферов и закрытие файлов
    delete[] inputBuffer;
    delete[] resultBuffer;
    fclose(inputFile);
    fclose(resultFile);

    chrono::steady_clock::time_point end = chrono::steady_clock::now();
    cout << "\nFile deduplicated successfully! Execution time: " << chrono::duration_cast<std::chrono::seconds>(end - begin).count() << "[s]\n" << endl;

	return ERROR_SUCCESS;
}