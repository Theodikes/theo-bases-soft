#include "utils.hpp"

/* Читает буфер побайтово, считая строки, пока remainingStrings не станет 0. Тогда перестаёт считать и возвращает
позицию начала следующей строки в буфере. Если же прочитан весь буфер, но нужного количества строк не набралось,
возвращает позицию последнего элемента в буфере. */
size_t readBufferByLinesUntilCount(char* buffer, size_t buflen, size_t startBufIndex, size_t* remainingStrings) {
	for (size_t i = startBufIndex; i < buflen; i++) if (buffer[i] == 10 and !-- * remainingStrings) return i + 1;
	return buflen;
}

FILE* getNextSplittedFilePtr(const char* destinationDirectory, size_t linesInOneFile, size_t currentFileNumber, const char* inputFilePath) {
	string resultFilenameWithExtension = getFileNameWithoutExtension(inputFilePath) + "_" + to_string(linesInOneFile) + + "_" + to_string(currentFileNumber) + ".txt";
	const char* pathToSplittedFile = path_join(destinationDirectory, resultFilenameWithExtension.c_str());
	return fopen(pathToSplittedFile, "wb+");
}

// Опции для ввода аргументов вызова программы из cmd, показыаемые пользователю при использовании флага --help или -h
static const char* const usages[] = {
	"theo s [options]",
	NULL,
};

int split(int argc, const char** argv) {
	const char* inputFilePath = "merged.txt";
	const char* destinationDirectoryPath = ".";
	size_t	linesInOneFile = 0;

	struct argparse_option options[] = {
		OPT_HELP(),
		OPT_GROUP("Basic options"),
		OPT_INTEGER('l', "lines", &linesInOneFile, "Number of lines in each file after splitting"),
		OPT_STRING('s', "source", &inputFilePath, "Path to file to be splitted ('merged.txt' by default)"),
		OPT_STRING('d', "destination", &destinationDirectoryPath, "Destination directory, where the split files will be written\
						      (current directory by default)"),
		OPT_END(),
	};
	struct argparse argparse;
	argparse_init(&argparse, options, usages, 0);
	argparse_parse(&argparse, argc, argv);

	if (linesInOneFile < 1) {
		cout << "Error: wrong 'lines' parameter value [" << linesInOneFile << "]: it must be a positive integer" << endl;
		exit(1);
	}

	if (isAnythingExistsByPath(destinationDirectoryPath) and not isDirectory(destinationDirectoryPath)) {
		cout << "Error: something is by path [" << destinationDirectoryPath << "] and this isn`t directory" << endl;
		exit(1);
	}
	

	if (!isAnythingExistsByPath(destinationDirectoryPath)) {
		char answer;
		cout << "Destination directory doesn`t exist, create it? (y/n): ";
		cin >> answer;
		if (answer != 'y') exit(1);
		bool ret = CreateDirectory(destinationDirectoryPath, NULL);
		if (!ret) {
			cout << "Error: cannot create directory by destination path" << endl;
			exit(1);
		}
	}

	FILE* inputFilePtr = fopen(inputFilePath, "rb");
	if (inputFilePtr == NULL) {
		cout << "Error: cannot open [" << inputFilePath << "] because of invalid path or due to security policy reasons." << endl;
		exit(1);
	}

	chrono::steady_clock::time_point begin = chrono::steady_clock::now();
	
	/* Вычисляем размер временного буфера для хранения и обработки байтовых данных с файлов. 
	* Обычно буфер должен иметь оптимальный размер для работы с диском (64 мегабайта, степень двойки в байтах),
	* однако, если сам изначальный файл для разбиения меньше, используется его размер, чтобы не занимать лишнюю оперативку */
	ull fileSize = getFileSize(inputFilePath);
	size_t countBytesToReadInOneIteration = min(OPTIMAL_DISK_CHUNK_SIZE, fileSize);

	/* Буфер, в который будет считываться информация с диска(со входящего файла) и в котором будут считаться строки.
	* Аллоцируется в куче, потому что в стеке может быть ограничение на размер памяти */
	char* buffer = new char[countBytesToReadInOneIteration];
	if (buffer == NULL) {
		cout << "Error: annot allocate buffer of " << countBytesToReadInOneIteration << "bytes" << endl;
		exit(1);
	}

	/* Количество строк, которое должно быть в каждом файле после разделения. Создается дополнительная переменная,
	* помимо 'linesInOneFile', значение которой вводится пользователем, поскольку нужна изменяющаяся по указателю
	* переменная для работы с временным буфером (подсчёта строк в каждом новом) */
	size_t remainingStrings = linesInOneFile; 

	/* Текущий номер файла, в который записываются строки из изначального. Имя каждого нового файла - 
	* имя изначального файла без расширения + '_[порядковый номер файла].txt' в конце */
	size_t currentFileNumber = 1;
	FILE* currentSplittedFilePtr = getNextSplittedFilePtr(destinationDirectoryPath, linesInOneFile, currentFileNumber, inputFilePath);

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
				remainingStrings = linesInOneFile;
				fclose(currentSplittedFilePtr);
				// Если конец входного файла, новый файл для строк создавать не надо, так как он будет пустым
				if(!feof(inputFilePtr) or startPos < bytesReaded) currentSplittedFilePtr = getNextSplittedFilePtr(destinationDirectoryPath, linesInOneFile, ++currentFileNumber, inputFilePath);
			}
		}
		// Если прочитали весь файл, который мы делим, закрываем текущий файл для записи (он будет неполным и последним)
		if (feof(inputFilePtr)) fclose(currentSplittedFilePtr);
	}

	chrono::steady_clock::time_point end = chrono::steady_clock::now();
	cout << "\nFile splitted successfully! Execution time: " << chrono::duration_cast<std::chrono::seconds>(end - begin).count() << "[s]\n" << endl;

	return ERROR_SUCCESS;
}