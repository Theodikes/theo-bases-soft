#include "utils.hpp"

// Опции для ввода аргументов вызова программы из cmd, показыаемые пользователю при использовании флага --help или -h
static const char* const usages[] = {
	"theo c [options] [paths]",
	NULL,
};

int count(int argc, const char** argv) {
	/* Требуется ли рекурсивно искать файлы для подсчёта строк в переданных пользователем директориях
	 * (то есть, надо ли проверять поддиректории и поддиректории поддиректорий и так далее до конца) */
	bool checkSourceDirectoriesRecursive = false;

	struct argparse_option options[] = {
		OPT_HELP(),
		OPT_GROUP("File options"),
		OPT_BOOLEAN('r', "recursive", &checkSourceDirectoriesRecursive, "check source directories recursive (default - false)"),
		OPT_GROUP("All unmarked arguments are considered paths to files and folders with bases that need to be merged."),
		OPT_END(),
	};
	struct argparse argparse;
	argparse_init(&argparse, options, usages, ARGPARSE_STOP_AT_NON_OPTION);
	int remainingArgumentsCount = argparse_parse(&argparse, argc, argv);
	if (remainingArgumentsCount < 1) {
		argparse_usage(&argparse);
		return -1;
	}

	// Файлы для объединения (set, чтобы избежать повторной обработки одних и тех же файлов)
	robin_hood::unordered_flat_set<string> sourceFilesPaths;
	for (int i = 0; i < remainingArgumentsCount; i++) {
		processSourceFileOrDirectory(&sourceFilesPaths, argv[i], checkSourceDirectoriesRecursive);
	}

	if (sourceFilesPaths.empty()) {
		cout << "Error: paths to bases not specified" << endl;
		exit(1);
	}

	// Читаем по 64 мегабайта за одну итерацию, поскольку это оптимальный размер для чтения блока данных с ssd
	size_t countBytesToReadInOneIteration = OPTIMAL_DISK_CHUNK_SIZE;
	// Создаём буфер, в который будут считываться данные из файла
	char* buffer = new char[countBytesToReadInOneIteration + 1];
	if (buffer == NULL) {
		cout << "Error: not enough memory in heap to allocate 64MB temporary buffer" << endl;
		exit(1);
	}

	unsigned long long stringsCount = 0;

	chrono::steady_clock::time_point begin = chrono::steady_clock::now();
	for (string sourceFilePath : sourceFilesPaths) {
		FILE* sourceFilePtr = fopen(sourceFilePath.c_str(), "rb");
		if (sourceFilePtr == NULL) {
			cout << "File is skipped. Cannot open [" << sourceFilePath << "] because of invalid path or due to security policy reasons." << endl;
			continue;
		}

		while (!feof(sourceFilePtr)) {
			size_t bytesReaded = fread(buffer, sizeof(char), countBytesToReadInOneIteration, sourceFilePtr);
			for (size_t i = 0; i < bytesReaded; i++) if (buffer[i] == '\n') stringsCount++;
			if (feof(sourceFilePtr) and buffer[bytesReaded - 1] != '\n') stringsCount++;
		}
		fclose(sourceFilePtr);
	}
	delete[] buffer;

	chrono::steady_clock::time_point end = chrono::steady_clock::now();
	cout << "Execution time: " << chrono::duration_cast<std::chrono::seconds>(end - begin).count() << "[s]\n" << endl;
	cout << "Strings count " << (sourceFilesPaths.size() == 1 ? "in file" : "in all files") << ": " << stringsCount << endl;

	return ERROR_SUCCESS;
}