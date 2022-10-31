#include "utils.hpp"

/* Читает буфер побайтово, считая строки, пока remainingStrings не станет 0. Тогда перестаёт считать и возвращает
позицию начала следующей строки в буфере. Если же прочитан весь буфер, но нужного количества строк не набралось,
возвращает позицию последнего элемента в буфере. */
static size_t readBufferByLinesUntilCount(char* buffer, size_t buflen, size_t startBufIndex, size_t* remainingStrings);

// Создаёт следующий по счёту файл с N-ным количеством строк, открывает в режиме записи и возвращает указатель на него
static FILE* getNextSplittedFilePtr(wstring destinationDirectory, size_t linesInOneFile, size_t currentFileNumber, wstring inputFilePath);

// Опции для ввода аргументов вызова программы из cmd, показыаемые пользователю при использовании флага --help или -h
static const char* const usages[] = {
	"theo s [options] [path]",
	NULL,
};

int split(int argc, const char** argv) {
	const char* destinationDirectoryPath = ".";
	int linesInOneResultFile = 0;
	int parts = 0;

	struct argparse_option options[] = {
		OPT_HELP(),
		OPT_GROUP("Basic options"),
		OPT_INTEGER('l', "lines", &linesInOneResultFile, "Number of lines in each file after splitting"),
		OPT_INTEGER('p', "parts", &parts, "Into how many parts divide the source file"),
		OPT_GROUP("File options"),
		OPT_STRING('d', "destination", &destinationDirectoryPath, "Destination directory, where the splitted files will be written\n\t\t\t\t  (current directory by default)"),
		OPT_GROUP("    Unmarked (positional) argument are considered as path to file that need to be splitted. "),
		OPT_END(),
	};
	struct argparse argparse;
	argparse_init(&argparse, options, usages, 0);
	int remainingArgumentsCount = argparse_parse(&argparse, argc, argv);
	if (remainingArgumentsCount < 1) {
		argparse_usage(&argparse);
		return -1;
	}

	if (parts and linesInOneResultFile) {
		cout << "Error: only one parameter can be specified: either '--parts' or '--lines'" << endl;
		exit(1);
	}

	if (not parts and not linesInOneResultFile) {
		cout << "Error: you need to specify one of required parameters: either '--parts' or '--lines' with positive integer" << endl;
		exit(1);
	}

	// Проверяем итоговую директорию, есть ли к ней доступ и существует ли она
	checkDestinationDirectory(toWstring(destinationDirectoryPath));

	const char* inputFilePath = argv[0];
	FILE* inputFilePtr = fileOpen(inputFilePath, "rb");
	if (inputFilePtr == NULL) {
		cout << "Error: cannot open [" << inputFilePath << "] because of invalid path or due to security policy reasons." << endl;
		exit(1);
	}

	chrono::steady_clock::time_point begin = chrono::steady_clock::now();
	
	/* Вычисляем размер временного буфера для хранения и обработки байтовых данных с файлов. 
	* Обычно буфер должен иметь оптимальный размер для работы с диском (64 мегабайта, степень двойки в байтах),
	* однако, если сам изначальный файл для разбиения меньше, используется его размер, чтобы не занимать лишнюю оперативку */
	ull fileSize = getFileSize(toWstring(inputFilePath));
	size_t countBytesToReadInOneIteration = min(OPTIMAL_DISK_CHUNK_SIZE, fileSize);

	/* Буфер, в который будет считываться информация с диска(со входящего файла) 
	* и в котором будут считаться строки.
	* Аллоцируется в куче, потому что в стеке может быть ограничение на размер памяти */
	char* buffer = new char[countBytesToReadInOneIteration];
	if (buffer == NULL) {
		cout << "Error: annot allocate buffer of " << countBytesToReadInOneIteration << "bytes" << endl;
		exit(1);
	}

	/* Если файл надо разделить на определённое количество частей, то сначала
	 * считаем количество строк во входном файле и делим общее число строк из него
	 * на количество необходимых частей, получая так нужное количество строк на один итоговый файл.
	 */
	if (parts > 0) {
		long long linesInInputFileCount = getStringCountInFile(toWstring(inputFilePath));
		if (linesInInputFileCount == -1) {
			cout << "Error: cannot get count of lines in input file [" << inputFilePath << "]." << endl;
			return -1;
		}
		
		// Если итоговых частей указано больше, чем строк во входном файле - это ошибка, выходим
		long long linesInOneResultFileFloored = linesInInputFileCount / parts;
		if (linesInOneResultFileFloored == 0) {
			cout << "Error: invalid '--parts' parameter value - there are fewer lines in the input file [" << inputFilePath << "] than the specified number of parts into which it must be divided" << endl;
			return -1;
		}

		/* Округляем количество строк в одном файле в большую сторону, поскольку,
		 * если поделилось неровно, в последнем итоговом файле должно быть просто меньше строк,
		 * чем в остальных, и общее число итоговых файлов должно быть ровно заданное пользователем
		 */
		linesInOneResultFile = ceil(linesInInputFileCount / (double)parts);
	}

	/* Количество строк, которое должно быть в каждом файле после разделения. Создается дополнительная переменная,
	* помимо 'linesInOneFile', значение которой вводится пользователем, поскольку нужна изменяющаяся по указателю
	* переменная для работы с временным буфером (подсчёта строк в каждом новом) */
	size_t remainingStrings = linesInOneResultFile; 

	/* Текущий номер файла, в который записываются строки из изначального. Имя каждого нового файла - 
	* имя изначального файла без расширения + '_[порядковый номер файла].txt' в конце */
	size_t currentFileNumber = 1;
	FILE* currentSplittedFilePtr = getNextSplittedFilePtr(toWstring(destinationDirectoryPath), linesInOneResultFile, currentFileNumber, toWstring(inputFilePath));

	while (!feof(inputFilePtr)) {
		size_t bytesReaded = fread(buffer, sizeof(char), countBytesToReadInOneIteration, inputFilePtr);

		// Пробегаемся по считанному буферу, считая строки
		size_t startPos = 0;
		while (startPos < bytesReaded) {
			/* Считает строки либо до конца буфера, либо пока не наберет lineInOneFileCount (это значит,
			* что автодекрементирующийся счётчик remainingStrings станет равен нулю). 
			* Возвращается итоговую позицию в буфере после чтения нужного числа строк или после окончания чтения буфера */
			size_t endPos = readBufferByLinesUntilCount(buffer, bytesReaded, startPos, &remainingStrings);

			// Записываем считанные из буфера строки в текущий итоговый файл
			fwrite(&buffer[startPos], sizeof(char), endPos - startPos, currentSplittedFilePtr);

			// В следующий раз мы будем считывать информацию из буфера, начиная с endPos, на котором закончили в этот раз
			startPos = endPos;

			// Если мы набрали нужное число строк для текущего файла, обновляем счетчик строк, закрываем файл и создаем следующий
			if (remainingStrings == 0) {
				remainingStrings = linesInOneResultFile;
				fclose(currentSplittedFilePtr);
				// Если конец входного файла, новый файл для строк создавать не надо, так как он будет пустым
				if(!feof(inputFilePtr) or startPos < bytesReaded) currentSplittedFilePtr = getNextSplittedFilePtr(toWstring(destinationDirectoryPath), linesInOneResultFile, ++currentFileNumber, toWstring(inputFilePath));
			}
		}
		// Если прочитали весь файл, который мы делим, закрываем текущий файл для записи (он будет неполным и последним)
		if (feof(inputFilePtr)) fclose(currentSplittedFilePtr);
	}

	chrono::steady_clock::time_point end = chrono::steady_clock::now();
	cout << "\nFile splitted successfully! Execution time: " << chrono::duration_cast<std::chrono::seconds>(end - begin).count() << "[s]\n" << endl;

	return ERROR_SUCCESS;
}

static size_t readBufferByLinesUntilCount(char* buffer, size_t buflen, size_t startBufIndex, size_t* remainingStrings) {
	for (size_t i = startBufIndex; i < buflen; i++) if (buffer[i] == 10 and !-- * remainingStrings) return i + 1;
	return buflen;
}

static FILE* getNextSplittedFilePtr(wstring destinationDirectory, size_t linesInOneFile, size_t currentFileNumber, wstring inputFilePath) {
	wstring resultFilenameWithExtension = getFileNameWithoutExtension(inputFilePath) + L"_" + to_wstring(linesInOneFile) + L"_" + to_wstring(currentFileNumber) + L".txt";
	wstring pathToSplittedFile = joinPaths(destinationDirectory, resultFilenameWithExtension);
	return fileOpen(pathToSplittedFile, "wb+");
}