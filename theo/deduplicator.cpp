#include "utils.hpp"

// Хранилище для всех хешей уникальных строк
static robin_hood::unordered_flat_set<ull> stringHashes;

/* Максимальный процент оперативной памяти, которая может быть занята при работе программы.
* Если этот процент превышается, программа начинает использовать диск для хранения хешей */
static int memoryUsageMaxPercent = 90;

static class HashesDB {
private:
    // Директория, в которой будет хранится временный файл базы данных и технические файлы
    string dbParentDirectory;
    // Имя временного файла базы данных, при инициализации будет заменено на полный путь
    string dbFilePath = "dedup.db";
    // Технические файлы Berkeley DB, создаваемые при инициализации базы, их тоже надо чистить
    const vector<string> dbTechFiles = { "__db.001", "__db.002", "__db.003" };
public:
    // Используется ли база данных для хранения хешей или хватает оперативной памяти
    bool isDBUsed = false;
    /* Хранилище для числовых хешей уникальных строк в дисковой памяти (если закончилась
    * оперативная память, программа начинается пользоваться этим хешсетом).
    * Доступ осуществляется через указатель, поскольку иначе программа не запускается, так как
    * объекты BerkeleyDB не должны быть в глобальной области видимости. */
    db_hashset* stringHashes;
    // Сохраняет информацию информацию о директории, где будут храниться временные файлы базы
    void init(string destinationUserFilePath);
    /* Создает файл базы данных в той же директории, в которой создается пользовательский 
    * итоговый файл с дедуплицированными строками. Инициализирует stringHashes и линкует его к базе */
    void createDB(void);
    // Закрывает базу данных и удаляет файл базы с диска
    void clearDBs(void);
} hashesDB;


// По хешу определяет, была ли уже такая строка, если не было - добавляет её в итоговый буфер и меняет переменную с длиной итогового буфера
static void addStringToDestinationBufferCheckingHash(ull stringHash, char* sourceBuffer, size_t sourceBufferPos, size_t stringStartPosInSourceBuffer, char* destinationBuffer, size_t* destinationBufferStringStartPosPtr);

/* Считывает входной буфер посимвольно, хеширует каждую считанную строку, уникальные записывает в итоговый буфер.
*  Возвращает размер итогового буфера в байтах (чтобы впоследствии записать все данные из него в файл) */
static size_t deduplicateBufferLineByLine(char* buffer, size_t buflen, char* resultBuffer);

// Проверяет, указан ли пользователем путь к исходному файлу, если нет - создает свой из имени входного файла
static const char* getDeduplicatedResultFilePath(const char* inputFilePath);

// Опции для ввода аргументов вызова программы из cmd, показыаемые пользователю при использовании флага --help или -h
static const char* const usages[] = {
	"theo d [options] [path]",
	NULL,
};

int deduplicate(int argc, const char** argv) {
    // Путь к итоговому файлу, куда будут записаны строки без дубликатов.
    const char* destinationFilePath = NULL;
	size_t	linesInOneFile = 0;

	struct argparse_option options[] = {
		OPT_HELP(),
        OPT_GROUP("Basic options"),
        OPT_INTEGER('m', "memory", &memoryUsageMaxPercent, "Maximum percentage of RAM usage. Only number (whout percent symbol).\n\t\t\t      After reaching limit, deduplication continues on disk (default - 90%)"),
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
    if (memoryUsageMaxPercent < 1 or memoryUsageMaxPercent > 100) {
        cout << "Invalid '--memory' parameter value, it must be lower than 100 and higher than 0" << endl;
        return ERROR_INVALID_PARAMETER;
    }

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

    if (destinationFilePath == NULL) destinationFilePath = getDeduplicatedResultFilePath(inputFilePath);

    hashesDB.init(destinationFilePath);

    FILE* resultFile = fopen(destinationFilePath, "wb+");
    if (resultFile == NULL) {
        cout << "Error: cannot open result file [" << destinationFilePath << " in write mode" << endl;
        exit(ERROR_OPEN_FAILED);
    }

    if (getAvailableMemoryInBytes() < static_cast<ull>(inputFileSizeInBytes)) {
        cout << "Warning: there may not be enough RAM to remove duplicates (if there are few duplicates in specified file). After starting disk space usage, the execution speed will slow down a lot.\n";
    }
    

    processFileByChunks(inputFile, resultFile, deduplicateBufferLineByLine);
    fclose(resultFile);
    if (hashesDB.isDBUsed) hashesDB.clearDBs();

    chrono::steady_clock::time_point end = chrono::steady_clock::now();
    cout << "\nFile deduplicated successfully! Execution time: " << chrono::duration_cast<std::chrono::seconds>(end - begin).count() << "[s]\n" << endl;

	return ERROR_SUCCESS;
}


static void addStringToDestinationBufferCheckingHash(ull stringHash, char* sourceBuffer, size_t sourceBufferPos, size_t stringStartPosInSourceBuffer, char* destinationBuffer, size_t* destinationBufferStringStartPosPtr) {
    // Если хеш строки уже присутствует в таблице, добавлять его снова не надо
    if (stringHashes.contains(stringHash)) return; 
    if (hashesDB.isDBUsed and hashesDB.stringHashes->count(stringHash)) return;

    // Добавляем в хеш-таблицу хеш строки для последующих проверок
    if (hashesDB.isDBUsed) hashesDB.stringHashes->insert(stringHash);
    else stringHashes.insert(stringHash); 
    // Вычисляем длину текущей строки (добавляем единицу, так как последний символ (перенос строки) надо оставить
    size_t currentStringLength = sourceBufferPos - stringStartPosInSourceBuffer + 1;
    // Сохраняем строку в итоговый буфер, копируя напрямую из изначального буфера
    memcpy(&destinationBuffer[*destinationBufferStringStartPosPtr], &sourceBuffer[stringStartPosInSourceBuffer], currentStringLength);

    // Вычисляем позицию конца в итоговом буфере и изменяем значение переменной, указывающей на начало следующей строки
    // в итоговом буфере, по указателю, чтобы при новом вызове функции сразу было верное значение
    *destinationBufferStringStartPosPtr += currentStringLength;
}


static size_t deduplicateBufferLineByLine(char* buffer, size_t buflen, char* resultBuffer) {
    /* Если диск для хранения хешей строк в базе данных ещё не используется, однако
    * оперативной памяти уже недостаточно (количество затраченной превышает разрешённый процент),
    * то создаем базу данных и связанный с ней хешсет, а также оповещаем об этом пользователя */
    if (not hashesDB.isDBUsed and getMemoryUsagePercent() > memoryUsageMaxPercent) {
        hashesDB.createDB();
        cout << "Not enough RAM. Start using disk space to deduplicate. Speed will be decreased." << endl;
    }
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

static const char* getDeduplicatedResultFilePath(const char* inputFilePath) {
    // По умолчанию создаёт файл с названием как у входного, но с добавлением '_dedup.txt' в конец
    string destString = getFileNameWithoutExtension(inputFilePath) + "_dedup.txt";

    /* Копируем локальную строку в массив char, выделяя под него память в куче, чтобы
    * при завершении функции доступ к строке по указателю остался */
    char* destPath = new char[destString.length() + 1];
    strcpy(destPath, destString.c_str());

    return destPath;
}

void HashesDB::init(string destinationUserFilePath) {
    // Устанавливаем внутренние переменные класса: путь к папке с базой и полный путь к файлу базы
    dbParentDirectory = getDirectoryFromFilePath(destinationUserFilePath);
    dbFilePath = joinPaths(dbParentDirectory, dbFilePath);
}

void HashesDB::createDB(void) {
    // Устанавливаем флаг, что теперь для хранения хешей используется DB, а не оперативная память
    isDBUsed = true; 
    DbEnv* penv = open_env(dbParentDirectory.c_str(), 0u, DB_CREATE | DB_INIT_MPOOL);
    Db* db = open_db(penv, dbFilePath.c_str(), DB_BTREE, DB_CREATE, 0u);
    stringHashes = new db_hashset(db, penv);
}
void HashesDB::clearDBs(void) {
    // Закрываем все окружения баз и все базы
    close_all_db_envs();
    close_all_dbs();

    // Удаляем файл с временной базой данных и все технические файлы базы
    fs::remove(dbFilePath);
    for (string techDBFile : dbTechFiles) fs::remove(joinPaths(dbParentDirectory, techDBFile));
}