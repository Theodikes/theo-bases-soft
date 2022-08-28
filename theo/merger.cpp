#include "utils.hpp"

// Опции для ввода аргументов вызова программы из cmd, показыаемые пользователю при использовании флага --help или -h
static const char* const usages[] = {
	"theo m [options] [paths]",
	NULL,
};

int merge(int argc, const char** argv) {
	const char* resultFilePath = "merged.txt";
	/* Требуется ли рекурсивно искать файлы для объединения в переданных пользователем директориях (то есть,
	* надо ли проверять поддиректории и поддиректории поддиректорий и так далее до конца) */
	bool checkSourceDirectoriesRecursive = false;

	struct argparse_option options[] = {
		OPT_HELP(),
		OPT_GROUP("Basic options"),
		OPT_STRING('d', "destination", &resultFilePath, "Path to result file with all merged strings ('merged.txt' by default)"),
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

	FILE* resultFilePtr = fopen(resultFilePath, "wb+");

	if (resultFilePtr == NULL) {
		cout << "Error: Cannot open result file [" << resultFilePath << "] in write mode" << endl;
		exit(1);
	}

	for (string sourceFilePath : sourceFilesPaths) {
		FILE* sourceFilePtr = fopen(sourceFilePath.c_str(), "rb");
		if (sourceFilePtr == NULL) {
			cout << "File is skipped. Cannot open [" << sourceFilePath << "] because of invalid path or due to security policy reasons." << endl;
			continue;
		}

		// Читаем по 64 мегабайта за одну итерацию, поскольку это оптимальный размер для чтения блока данных с ssd
		size_t countBytesToReadInOneIteration = 1024 * 1024 * 64;
		char* buffer = new char[countBytesToReadInOneIteration + 1];
		if (buffer == NULL) {
			cout << "Error: not enough memory in heap to allocate 64MB temporary buffer" << endl;
			exit(1);
		}

		while (!feof(sourceFilePtr)) {
			size_t bytesReaded = fread(buffer, sizeof(char), countBytesToReadInOneIteration, sourceFilePtr);
			// Если файл дочитан до конца, добавим перенос строки, чтобы не соединилось с первой строкой следующего файла
			if (feof(sourceFilePtr)) buffer[bytesReaded++] = 10;
			fwrite(buffer, sizeof(char), bytesReaded, resultFilePtr);
		}
		delete[] buffer;
		fclose(sourceFilePtr);
	}

	fclose(resultFilePtr);

	cout << "\nAll files merged successfully!\n" << endl;

	return ERROR_SUCCESS;
}