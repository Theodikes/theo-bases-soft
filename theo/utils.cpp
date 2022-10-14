﻿#include "utils.hpp"

wstring toWstring(string s) noexcept {
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	return converter.from_bytes(s);
}

string fromWstring(wstring s) noexcept {
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	return converter.to_bytes(s);
}

wstring joinPaths(wstring dirPath, wstring filePath) noexcept {
	return (fs::path(dirPath) / fs::path(filePath)).wstring();
}


bool processSourceFileOrDirectory(sourcefiles_info& textFilesPaths, wstring path, bool recursive)
{
	if (!isAnythingExistsByPath(path)) {
		wcout << "File doesn`t exist: [" << path <<  "]" << endl;
		return false;
	}
	if (!isDirectory(path)) return addFileToSourceList(textFilesPaths, path);

	for (const auto& entry : fs::directory_iterator(path)) {
		wstring subpath = entry.path().wstring();
		if (isDirectory(subpath) and recursive) processSourceFileOrDirectory(textFilesPaths, subpath, recursive);
		else addFileToSourceList(textFilesPaths, subpath);
	}

	return true;
}

bool addFileToSourceList(sourcefiles_info& sourceTextFilesPaths, wstring filePath) noexcept {
	if (not filePath.ends_with(L".txt")) return false;
	sourceTextFilesPaths.insert(filePath);
	return true;
}

sourcefiles_info getSourceFilesFromUserInput(size_t sourcePathsCount, const char** userSourcePaths, bool checkDirectoriesRecursive) noexcept {
	// Файлы для нормализации (set, чтобы избежать повторной обработки одних и тех же файлов)
	sourcefiles_info sourceFilesPaths;

	// Обрабатываем все пути, указанные пользователем, получаем оттуда все txt-файлы и сохраняем их в sourceFilesPaths
	for (size_t i = 0; i < sourcePathsCount; i++) {
		processSourceFileOrDirectory(sourceFilesPaths, toWstring(userSourcePaths[i]), checkDirectoriesRecursive);
	}

	// Если ни одного валидного пути не оказалось, выходим
	if (sourceFilesPaths.empty()) {
		cout << "Error: valid paths to bases not specified" << endl;
		exit(1);
	}

	return sourceFilesPaths;
}

wstring getFileNameWithoutExtension(wstring pathToFile) noexcept {
	if (not fs::exists(pathToFile)) return L"";
	return filesystem::path(pathToFile).stem().wstring();
}


size_t getLinesCountInText(char* bytes) noexcept {
	size_t stringsCount = 0;
	while (*bytes++) if (*bytes == 10) stringsCount++;
	return stringsCount;
}

long long getFileSize(wstring pathToFile) {
	WIN32_FILE_ATTRIBUTE_DATA fileData;
	if (GetFileAttributesExW(pathToFile.c_str(), GetFileExInfoStandard, &fileData))
		return (static_cast<ull>(fileData.nFileSizeHigh) << sizeof(fileData.nFileSizeLow) * 8) | fileData.nFileSizeLow;
	return -1;
}

static LPMEMORYSTATUSEX getMemoryInfo(void) noexcept {
	MEMORYSTATUSEX ms;
	ms.dwLength = sizeof(ms);
	DWORD ret = GlobalMemoryStatusEx(&ms);
	if (ret == 0) {
		cout << "Cannot get info about computer memory. Program may working incorrectly." << endl;
		return NULL;
	}
	return &ms;
}

ull getAvailableMemoryInBytes(void) noexcept {
	LPMEMORYSTATUSEX ms = getMemoryInfo();
	if (ms == NULL) return 0;
	return ms->ullAvailPhys;
}

ull getTotalMemoryInBytes(void) noexcept {
	LPMEMORYSTATUSEX ms = getMemoryInfo();
	if (ms == NULL) return 0;
	return ms->ullTotalPhys;
}

size_t getMemoryUsagePercent(void) noexcept {
	/* Если нет информации о памяти, считаем общую память за единицу, чтобы общий процент
	* использованной памяти считался 100 (вся память использована), дабы программа не вылетела */
	ull totalRAMInBytes = getTotalMemoryInBytes() ? getTotalMemoryInBytes() : 1;
	return static_cast<size_t>((totalRAMInBytes - getAvailableMemoryInBytes()) / (long double) totalRAMInBytes * 100);
}

bool isAnythingExistsByPath(wstring path) noexcept {
	return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool isDirectory(wstring path) noexcept {
	return fs::is_directory(path);
}

bool isValidRegex(string regularExpression) noexcept {
	try {
		regex re(regularExpression);
	}
	catch (const regex_error&) {
		return false;
	}
	return true;
}

wstring getWorkingDirectoryPath() noexcept {
	return fs::current_path().wstring();
}

wstring getDirectoryFromFilePath(wstring filePath) noexcept {
	if (not fs::exists(filePath)) return L"";
	return fs::absolute(filePath).parent_path().wstring();
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

void processAllSourceFiles(sourcefiles_info sourceFilesPaths, bool needMerge, FILE* resultFile, wstring destinationDirectoryPath, wstring resultFilesSuffix, size_t processChunkBuffer(char* inputBuffer, size_t inputBufferLength, char* resultBuffer)) {
	for (wstring& sourceFilePath : sourceFilesPaths) {

		FILE* inputBaseFilePointer = _wfopen(sourceFilePath.c_str(), L"rb");
		if (inputBaseFilePointer == NULL) {
			wcout << "File is skipped. Cannot open [" << sourceFilePath << "] because of invalid path or due to security policy reasons." << endl;
			continue;
		}

		/* Если мы не складываем всё в один файл, то каждую итерацию цикла создаём под каждый входной файл
		* свой итоговый файл, в котором будут находиться нормализованные строки из входного */
		if (!needMerge) {
			resultFile = getResultFilePtr(destinationDirectoryPath, sourceFilePath, resultFilesSuffix);
			if (resultFile == NULL) {
				wcout << "Error: cannot open result file [" << joinPaths(destinationDirectoryPath, sourceFilePath) << "] in write mode" << endl;
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

void processDestinationPath(const char** destinationPathPtr, bool needMerge, FILE** resultFile, const char* defaultResultMergedFilePath) noexcept {
	// Проверяем, всё ли нормально с итоговой директорией (или итоговым файлом)
	if (needMerge) {
		if (not *destinationPathPtr) *destinationPathPtr = defaultResultMergedFilePath;
		if (isAnythingExistsByPath(toWstring(*destinationPathPtr))) {
			cout << "Error: cannot create result file, something exist on path [" << *destinationPathPtr << ']' << endl;
			exit(1);
		}

		/* Открываем итоговый файл и присваиваем значение на открытый файл по указателю, чтобы он
		* был доступен вне функции */
		*resultFile = fopen(*destinationPathPtr, "wb+");
		if (*resultFile == NULL) {
			cout << "Error: cannot open result file [" << *destinationPathPtr << "] in write mode" << endl;
			exit(ERROR_OPEN_FAILED);
		}
	}
	else {
		// Если пользователь не указал путь к итоговой директории, путь по умолчанию - рабочая директория
		if (not *destinationPathPtr) *destinationPathPtr = ".";
		// Проверяем директорию на валидность и есть ли к ней доступ
		checkDestinationDirectory(toWstring(*destinationPathPtr));
	}
}

void checkDestinationDirectory(wstring destinationDirectoryPath) noexcept {
	// Если на указанном пользователем пути к итоговой директории что-то есть, и это что-то - не папка, выходим
	if (isAnythingExistsByPath(destinationDirectoryPath) and not isDirectory(destinationDirectoryPath)) {
		wcout << "Error: something exists by path [" << destinationDirectoryPath << "] and this isn`t directory" << endl;
		exit(1);
	}

	// Если директории не существует и пользователь не хочет её создавать, выходим
	if (not isAnythingExistsByPath(destinationDirectoryPath) and not createDirectoryUserDialog(destinationDirectoryPath)) exit(1);
}

bool createDirectoryUserDialog(wstring creatingDirectoryPath) noexcept {
	char answer;
	cout << "Destination directory doesn`t exist, create it? (y/n): ";
	cin >> answer;
	if (answer != 'y') return false;
	bool ret = fs::create_directory(creatingDirectoryPath);
	if (!ret) {
		cout << "Error: cannot create directory by destination path" << endl;
		return false;
	}
	cout << "Destination directory successfully created!" << endl;
	return true;
}

FILE* getResultFilePtr(wstring pathToResultFolder, wstring pathToSourceFile, wstring fileSuffixName) {

	/* Поскольку могут быть файлы с одинаковыми названиями из разных директорий, выбираем имя итогового,
	нормализованного файла, пока не найдём незанятое (допустим, если нормализуется два файла из разных директорий с
	совпадающими именами, предположим, base1/test.txt и base2/test.txt, первый будет положен в итоговую директорию
	как test_normalized_1.txt, а второй как test_normalized_2.txt)*/
	wstring resultFilePath;
	size_t i = 1;
	do {
		wstring resultFileName = getFileNameWithoutExtension(pathToSourceFile);
		resultFilePath = joinPaths(pathToResultFolder, resultFileName + L'_' + fileSuffixName + L'_' + to_wstring(i++) + L".txt");
	} while (isAnythingExistsByPath(resultFilePath));

	// Записывать будем в открытый через fopen файл и в байтовом режиме для большей скорости
	FILE* resultFilePtr = _wfopen(resultFilePath.c_str(), L"wb+");
	if (resultFilePtr == NULL) {
		wcout << "Cannot create file for " << fileSuffixName << " base by path : [" << resultFilePath << "] " << endl;
	}
	return resultFilePtr;
}