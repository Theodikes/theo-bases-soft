﻿#include "utils.hpp"

// Опции для ввода аргументов вызова программы из cmd, показыаемые пользователю при использовании флага --help или -h
static const char* const usages[] = {
	"theo c [options] [paths]",
	NULL,
};

int count(int argc, const char** argv) {
	/* Требуется ли рекурсивно искать файлы для подсчёта строк в переданных пользователем директориях
	 * (то есть, надо ли проверять поддиректории и поддиректории поддиректорий и так далее до конца) */
	int checkSourceDirectoriesRecursive = 0;

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

	// Получаем список всех валидных файлов, которые надо токенизировать
	sourcefiles_info sourceFilesPaths = getSourceFilesFromUserInput(remainingArgumentsCount, argv, checkSourceDirectoriesRecursive);

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
	for (const wstring& sourceFilePath : sourceFilesPaths) {
		long long stringsInCurrentFile = getStringCountInFile(sourceFilePath, countBytesToReadInOneIteration, buffer);
		// Если функция вернула -1, это значит, что она не смогла открыть файл.
		if (stringsInCurrentFile == -1) {
			wcout << "File is skipped. Cannot open [" << sourceFilePath << "] because of invalid path or due to security policy reasons." << endl; 
			continue;
		}
		// Если нет - добавляем к общему числу строк количество строк текущем в файле
		else stringsCount += static_cast<ull>(stringsInCurrentFile);
	}
	delete[] buffer;

	chrono::steady_clock::time_point end = chrono::steady_clock::now();
	cout << "Execution time: " << chrono::duration_cast<std::chrono::seconds>(end - begin).count() << "[s]\n" << endl;
	cout << "Strings count " << (sourceFilesPaths.size() == 1 ? "in file" : "in all files") << ": " << stringsCount << endl;

	return ERROR_SUCCESS;
}