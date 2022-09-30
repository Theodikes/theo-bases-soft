﻿#include "utils.hpp"

bool startsWith(const char* str, const char* prefix)
{
	size_t lenpre = strlen(prefix),
		lenstr = strlen(str);
	return lenstr < lenpre ? false : memcmp(prefix, str, lenpre) == 0;
}

bool endsWith(const char* str, const char* suffix)
{
	if (!str or !suffix) return 0;
	size_t lenstr = strlen(str);
	size_t lensuffix = strlen(suffix);
	if (lensuffix > lenstr) return 0;

	return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

string joinPaths(string dirPath, string filePath) {
	return (fs::path(dirPath) / fs::path(filePath)).string();
}


bool processSourceFileOrDirectory(sourcefiles_info* textFilesPaths, string path, bool recursive)
{
	if (!isAnythingExistsByPath(path)) {
		cout << "File doesn`t exist: [" << path <<  "]" << endl;
		return false;
	}
	if (!isDirectory(path)) return addFileToSourceList(textFilesPaths, path);

	for (const auto& entry : fs::directory_iterator(path)) {
		string subpath = entry.path().string();
		if (isDirectory(subpath) and recursive) processSourceFileOrDirectory(textFilesPaths, subpath, recursive);
		else addFileToSourceList(textFilesPaths, subpath);
	}

	return true;
}

bool addFileToSourceList(sourcefiles_info* sourceTextFilesPaths, string filePath) {
	if (!(endsWith(filePath.c_str(), ".txt"))) return false;
	(*sourceTextFilesPaths).insert(filePath);
	return true;
}

sourcefiles_info getSourceFilesFromUserInput(size_t sourcePathsCount, const char** userSourcePaths, bool checkDirectoriesRecursive) {
	// Файлы для нормализации (set, чтобы избежать повторной обработки одних и тех же файлов)
	sourcefiles_info sourceFilesPaths;

	// Обрабатываем все пути, указанные пользователем, получаем оттуда все txt-файлы и сохраняем их в sourceFilesPaths
	for (size_t i = 0; i < sourcePathsCount; i++) {
		processSourceFileOrDirectory(&sourceFilesPaths, userSourcePaths[i], checkDirectoriesRecursive);
	}

	// Если ни одного валидного пути не оказалось, выходим
	if (sourceFilesPaths.empty()) {
		cout << "Error: valid paths to bases not specified" << endl;
		exit(1);
	}

	return sourceFilesPaths;
}

string getFileNameWithoutExtension(string pathToFile) {
	return filesystem::path(pathToFile).stem().string();
}


size_t getLinesCountInText(char* bytes) {
	size_t stringsCount = 0;
	while (*bytes++) if (*bytes == 10) stringsCount++;
	return stringsCount;
}

long long getFileSize(const char* pathToFile) {
	WIN32_FILE_ATTRIBUTE_DATA fileData;
	if (GetFileAttributesEx(pathToFile, GetFileExInfoStandard, &fileData))
		return (static_cast<ull>(fileData.nFileSizeHigh) << sizeof(fileData.nFileSizeLow) * 8) | fileData.nFileSizeLow;
	return -1;
}

ull getAvailableMemoryInBytes(void) {
	MEMORYSTATUSEX ms;
	ms.dwLength = sizeof(ms);
	GlobalMemoryStatusEx(&ms);
	return ms.ullAvailPhys;
}

ull getTotalMemoryInBytes(void) {
	MEMORYSTATUSEX ms;
	ms.dwLength = sizeof(ms);
	GlobalMemoryStatusEx(&ms);
	return ms.ullTotalPhys;
}

size_t getMemoryUsagePercent(void) {
	return static_cast<size_t>((getTotalMemoryInBytes() - getAvailableMemoryInBytes()) / (long double) getTotalMemoryInBytes() * 100);
}

bool isAnythingExistsByPath(string path) {
	return GetFileAttributes(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool isDirectory(string path) {
	return GetFileAttributes(path.c_str()) & FILE_ATTRIBUTE_DIRECTORY;
}

bool isValidRegex(string regularExpression) {
	try {
		regex re(regularExpression);
	}
	catch (const regex_error&) {
		return false;
	}
	return true;
}

string getWorkingDirectoryPath() {
	return fs::current_path().string();
}

string getDirectoryFromFilePath(string filePath) {
	return fs::absolute(filePath).parent_path().string();
}

void processFileByChunks(FILE* inputFile, FILE* resultFile, size_t processChunkBuffer(char*, size_t, char*)) {
	// Отдельно объявляем переменную с говорящим названием для понимания её смысла в данном контексте
	size_t countBytesToReadInOneIteration = OPTIMAL_DISK_CHUNK_SIZE;

	/* Буфер, в который будет считываться информация с диска(со входящего файла) и в котором будут считаться строки.
	* Аллоцируется в куче, потому что в стеке может быть ограничение на размер памяти */
	char* inputBuffer = new char[countBytesToReadInOneIteration + 2];
	char* resultBuffer = new char[countBytesToReadInOneIteration + 2];
	if (inputBuffer == NULL or resultBuffer == NULL) {
		cout << "Error: annot allocate buffer of " << countBytesToReadInOneIteration * 2 << "bytes" << endl;
		exit(1);
	}

	while (!feof(inputFile)) {
		/* Считываем нужное количество байт из входного файла в буфер, количество реально считаных байт записывается
		*  в переменную, нужную на случай, если файл закончился, и реально считалось меньше байт, чем предполагалось */
		size_t bytesReaded = fread(inputBuffer, sizeof(char), countBytesToReadInOneIteration, inputFile);
		size_t inputBufferLength = bytesReaded;
		/* Если это последняя строка во входном файле и после неё нет переноса строки, устанавливаем его после
		* конца строки, чтобы в дальнейшем функция-обработчик считала это за цельную строку.
		* Кроме того, увеличиваем длину входного буфера на единицу, чтобы последний перенос был считан */
		if (feof(inputFile) and inputBuffer[inputBufferLength - 1] != '\n') inputBuffer[inputBufferLength++] = '\n';
		/* Если в этом считанном входном буфере осталась незаконченная строка, обрезанная при считывании побайтово
		 * делаем отступ в файле назад на длину оставшегося в буфере неполного куска строки,
		 * чтобы при следующем fread обработать её полностью. Так же уменьшаем размер входного буфера для чтения,
		 * чтобы туда не попал неполный кусок строки */
		else {
			while (inputBuffer[inputBufferLength - 1] != '\n') inputBufferLength--;
			size_t remainingStringPartLength = bytesReaded - inputBufferLength;
			if(remainingStringPartLength) fseek(inputFile, -static_cast<long>(remainingStringPartLength), SEEK_CUR);
		}
		/* Читаем буфер посимвольно, генерируем хеши для строк, проверяем на уникальность, записываем уникальные строки
		* последовательно в итоговый буфер и получаем размер отступа назад для чтения в следующий раз (если буфер был
		* обрезан на середине какой-то строки, отступ ненулевой, чтобы прочесть строку полностью)*/
		size_t resultBufferLength = processChunkBuffer(inputBuffer, inputBufferLength, resultBuffer);
		// Записываем данные из итогового буфера с уникальными строками в файл вывода
		fwrite(resultBuffer, sizeof(char), resultBufferLength, resultFile);
	}
	// Освобождение памяти буферов и закрытие файлов
	delete[] inputBuffer;
	delete[] resultBuffer;
}

void processAllSourceFiles(sourcefiles_info sourceFilesPaths, bool needMerge, FILE* resultFile, string destinationDirectoryPath, string resultFilesSuffix, size_t processChunkBuffer(char* inputBuffer, size_t inputBufferLength, char* resultBuffer)) {
	for (string sourceFilePath : sourceFilesPaths) {

		FILE* inputBaseFilePointer = fopen(sourceFilePath.c_str(), "rb");
		if (inputBaseFilePointer == NULL) {
			cout << "File is skipped. Cannot open [" << sourceFilePath << "] because of invalid path or due to security policy reasons." << endl;
			continue;
		}

		/* Если мы не складываем всё в один файл, то каждую итерацию цикла создаём под каждый входной файл
		* свой итоговый файл, в котором будут находиться нормализованные строки из входного */
		if (!needMerge) {
			resultFile = getResultFilePtr(destinationDirectoryPath, sourceFilePath, resultFilesSuffix);
			if (resultFile == NULL) {
				cout << "Error: cannot open result file [" << joinPaths(destinationDirectoryPath, sourceFilePath) << "] in write mode" << endl;
				continue;
			}
		}

		// Обрабатываем весь файл почанково и записываем все нормализованные строки в итоговый файл
		processFileByChunks(inputBaseFilePointer, resultFile, processChunkBuffer);
		// Закрываем входной файл
		fclose(inputBaseFilePointer);

	}
	_fcloseall(); // Закрываем все итоговые файлы
}

void checkDestinationDirectory(const char* destinationDirectoryPath) {
	// Если на указанном пользователем пути к итоговой директории что-то есть, и это что-то - не папка, выходим
	if (isAnythingExistsByPath(destinationDirectoryPath) and not isDirectory(destinationDirectoryPath)) {
		cout << "Error: something exists by path [" << destinationDirectoryPath << "] and this isn`t directory" << endl;
		exit(1);
	}

	// Если директории не существует и пользователь не хочет её создавать, выходим
	if (not isAnythingExistsByPath(destinationDirectoryPath) and not createDirectoryUserDialog(destinationDirectoryPath)) exit(1);
}

bool createDirectoryUserDialog(string creatingDirectoryPath) {
	char answer;
	cout << "Destination directory doesn`t exist, create it? (y/n): ";
	cin >> answer;
	if (answer != 'y') return false;
	bool ret = CreateDirectory(creatingDirectoryPath.c_str(), NULL);
	if (!ret) {
		cout << "Error: cannot create directory by destination path" << endl;
		return false;
	}
	cout << "Destination directory successfully created!" << endl;
	return true;
}

FILE* getResultFilePtr(string pathToResultFolder, string pathToSourceFile, string fileSuffixName) {

	/* Поскольку могут быть файлы с одинаковыми названиями из разных директорий, выбираем имя итогового,
	нормализованного файла, пока не найдём незанятое (допустим, если нормализуется два файла из разных директорий с
	совпадающими именами, предположим, base1/test.txt и base2/test.txt, первый будет положен в итоговую директорию
	как test_normalized_1.txt, а второй как test_normalized_2.txt)*/
	string resultFilePath;
	size_t i = 1;
	do {
		string resultFileName = getFileNameWithoutExtension(pathToSourceFile);
		resultFilePath = joinPaths(pathToResultFolder, resultFileName + '_' + fileSuffixName + '_' + to_string(i++) + ".txt");
	} while (isAnythingExistsByPath(resultFilePath));

	// Записывать будем в открытый через fopen файл и в байтовом режиме для большей скорости
	FILE* resultFilePtr = fopen(resultFilePath.c_str(), "wb+");
	if (resultFilePtr == NULL) {
		cout << "Cannot create file for " << fileSuffixName << " base by path : [" << resultFilePath << "] " << endl;
	}
	return resultFilePtr;
}