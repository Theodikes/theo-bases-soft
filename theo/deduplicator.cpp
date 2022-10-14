#include "utils.hpp"

// Хранилище для всех хешей уникальных строк
static robin_hood::unordered_flat_set<ull> stringHashes;

/* Максимальный процент оперативной памяти, которая может быть занята при работе программы.
* Если этот процент превышается, программа начинает использовать диск для хранения хешей */
static int memoryUsageMaxPercent = 90;

/* Класс для хранения всей информации о базе данных с хешами: stl-контейнер для взаимодействия с 
базой, функции для инициализации/открытия/закрытия базы, технические переменные с информацией
о файлах, представляющих базу данных на диске. */
static class HashesDB {
private:
    // Директория, в которой будет хранится временный файл базы данных и технические файлы
    wstring dbParentDirectory;
    // Имя временного файла базы данных, при инициализации будет заменено на полный путь
    wstring dbFilePath = L"dedup.db";
    // Технические файлы Berkeley DB, создаваемые при инициализации базы, их тоже надо чистить
    const vector<wstring> dbTechFiles = { L"__db.001", L"__db.002", L"__db.003" };
public:
    // Используется ли база данных для хранения хешей или хватает оперативной памяти
    bool isDBUsed = false;
    /* Хранилище для числовых хешей уникальных строк в дисковой памяти (если закончилась
    * оперативная память, программа начинается пользоваться этим хешсетом).
    * Доступ осуществляется через указатель, поскольку иначе программа не запускается, так как
    * объекты BerkeleyDB не должны быть в глобальной области видимости. */
    db_hashset* stringHashes = NULL;
    // Сохраняет информацию информацию о директории, где будут храниться временные файлы базы
    void init(wstring destinationUserFilePath);
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

// Опции для ввода аргументов вызова программы из cmd, показыаемые пользователю при использовании флага --help или -h
static const char* const usages[] = {
	"theo d [options] [path]",
	NULL,
};

int deduplicate(int argc, const char** argv) {
    // Путь к итоговому файлу, куда будут записаны строки без дубликатов.
    const char* destinationPath = NULL;
    // Надо ли объединять все итоговые файлы в один и удалять общие дубликаты в разных файлах
    bool needMerge = false; 
    // Нужно ли проверить поданные пользователем директории рекурсивно
    bool checkSourceDirectoriesRecursive = false; 
	size_t	linesInOneFile = 0;

	struct argparse_option options[] = {
		OPT_HELP(),
        OPT_GROUP("Basic options"),
        OPT_INTEGER('m', "memory", &memoryUsageMaxPercent, "Maximum percentage of RAM usage. Only number (whout percent symbol).\n\t\t\t      After reaching limit, deduplication continues on disk (default - 90%)"),
        OPT_GROUP("File options"),
        OPT_BOOLEAN(0, "merge", &needMerge, "remove duplicates from all lines of input files together and put result to one file"),
        OPT_STRING('d', "destination", &destinationPath, "absolute or relative path to result folder(default: current directory)\n\t\t\t      or file, if merge parameter is specified (default: dedup_merged.txt)"),
        OPT_BOOLEAN('r', "recursive", &checkSourceDirectoriesRecursive, "check source directories recursive (default - false)"),
        OPT_GROUP("All unmarked (positional) arguments are considered paths to files and folders with bases that need to be deduplicated.\nExample command: 'theo d -d result base1.txt base2.txt'. More: github.com/Theodikes/theo-bases-soft"),
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

    // Получаем список всех валидных файлов, которые надо дедуплицировать
    sourcefiles_info sourceFilesPaths = getSourceFilesFromUserInput(remainingArgumentsCount, argv, checkSourceDirectoriesRecursive);

    FILE* resultFile = NULL; 
    processDestinationPath(&destinationPath, needMerge, &resultFile, "dedup_merged.txt");

    if (memoryUsageMaxPercent < 1 or memoryUsageMaxPercent > 100) {
        cout << "Invalid '--memory' parameter value, it must be lower than 100 and higher than 0" << endl;
        return ERROR_INVALID_PARAMETER;
    }

    wstring destinationPathW = toWstring(destinationPath);
    /* Инициализируем базу данных в итоговой директории, указанной пользователем.
    * Если пользователь указал итоговый файл, инициализируем в той же директории, где он находится */
    hashesDB.init(needMerge ? getDirectoryFromFilePath(destinationPathW) : destinationPathW);

    for (const wstring& inputFilePath : sourceFilesPaths) {
        FILE* inputBaseFile = _wfopen(inputFilePath.c_str(), L"rb");
        if (inputBaseFile == NULL) {
            wcout << "File is skipped. Cannot open [" << inputFilePath << "] because of invalid path or due to security policy reasons." << endl;
            continue;
        }

        ull inputFileSizeInBytes = getFileSize(inputFilePath.c_str());
        if (getAvailableMemoryInBytes() < static_cast<ull>(inputFileSizeInBytes)) {
            cout << "Warning: there may not be enough RAM to remove duplicates (if there are few duplicates in specified file). After starting disk space usage, the execution speed will slow down a lot.\n";
        }

        /* Если мы не складываем всё в один файл, то каждую итерацию цикла создаём под каждый входной файл
        * свой итоговый файл, в котором будут находиться нормализованные строки из входного.
        * Кроме того, очищаем хранилище хешей строк с предыдущего файла, поскольку нам нужно искать
        * дубликаты в каждом входном файле отдельно, а не во всех сразу.*/
        if (!needMerge) {
            // Очищаем хештаблицы строк прошлых файлов, если надо
            if(not stringHashes.empty()) stringHashes.clear();
            if (hashesDB.isDBUsed) hashesDB.clearDBs();

            resultFile = getResultFilePtr(destinationPathW, inputFilePath, L"_dedup");
            if (resultFile == NULL) {
                wcout << "Error: cannot open result file [" << joinPaths(destinationPathW, inputFilePath) << "] in write mode" << endl;
                continue;
            }
        }

        processFileByChunks(inputBaseFile, resultFile, deduplicateBufferLineByLine);
        // Закрываем входной файл
        fclose(inputBaseFile);
    }

    _fcloseall();

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

void HashesDB::init(wstring destinationUserFilePath) {
    // Устанавливаем внутренние переменные класса: путь к папке с базой и полный путь к файлу базы
    dbParentDirectory = getDirectoryFromFilePath(destinationUserFilePath);
    dbFilePath = joinPaths(dbParentDirectory, dbFilePath);
}

void HashesDB::createDB(void) {
    // Устанавливаем флаг, что теперь для хранения хешей используется DB, а не оперативная память
    isDBUsed = true; 
    DbEnv* penv = open_env(fromWstring(dbParentDirectory).c_str(), 0u, DB_CREATE | DB_INIT_MPOOL);
    Db* db = open_db(penv, fromWstring(dbFilePath).c_str(), DB_BTREE, DB_CREATE, 0u);
    stringHashes = new db_hashset(db, penv);
}
void HashesDB::clearDBs(void) {
    /* Устанавливаем isDBUseed на false, поскольку запуск базы данных может не требоваться
    * для обработки следующего файла, который может быть меньшего размера */
    isDBUsed = false;

    // Удаляем саму базу
    delete this->stringHashes;
    this->stringHashes = NULL;

    // Закрываем все окружения баз и все базы
    close_all_db_envs();
    close_all_dbs();

    // Удаляем файл с временной базой данных и все технические файлы базы
    fs::remove(dbFilePath);
    for (wstring techDBFile : dbTechFiles) fs::remove(joinPaths(dbParentDirectory, techDBFile));
}