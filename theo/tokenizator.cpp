#include "utils.hpp"

static struct TokenizerParameters {
	bool onlyFirstPart = false; // Получить только первые части строк (emails/logins/nums)
	bool onlyLastPart = false; // Получить только последние части всех строк (passwords)
	const char* separatorSymbols = ";:";// Возможные разделители между email/login/num и password в каждой строке
} tokenizerParameters;

/*Обрабатывает буфер с байтами, считанными из файла, делит их на строки, строки разбивает по сепаратору и
* нужную часть записывает в итоговый буфер. Если сепаратора в строке нет, не записывает ничего.
* Возвращает длину итогового буфера, который надо записать в файл с нормализованными строками */
static size_t tokenizeBufferLineByLine(char* inputBuffer, size_t inputBufferLength, char* resultBuffer);

// Является ли текущий символ сепаратором между email/log/num и password в строке
static bool isSeparator(char symbol) { return strchr(tokenizerParameters.separatorSymbols, symbol) != NULL; }

// Опции для ввода аргументов вызова программы из cmd, показыаемые пользователю при использовании флага --help или -h
static const char* const usages[] = {
	"theo t` [options] [paths]",
	NULL,
};

int tokenize(int argc, const char** argv) {
	// Брать ли файлы для токенизации из указанных пользователем директорий рекурсивно (проверяя субдиректории)
	bool checkSourceDirectoriesRecursive = false; 
	const char* destinationPath = NULL; // Путь к итоговой папке или файлу (если needMerge = true)
	bool needMerge = false; // Требуется ли объединять нормализованные строки со всех файлов в один итоговый
	const char* resultStringPart = "first";

	struct argparse_option options[] = {
		OPT_HELP(),
		OPT_GROUP("Basic tokenize options"),
		OPT_STRING('p', "part", &resultStringPart, "which part of strings to get, 'first' or 'last' (default - first)"),
		OPT_STRING('s', "separators", &(tokenizerParameters.separatorSymbols), "a string containing possible delimiter characters\n\t\t\t\t  (between first part and password), enter without spaces. (default - \":;\")"),
		OPT_GROUP("File options"),
		OPT_BOOLEAN('m', "merge", &needMerge, "merge strings from all tokenized files to one destination file"),
		OPT_STRING('d', "destination", &destinationPath, "absolute or relative path to result folder(default: current directory)\n\t\t\t\t  or file, if merge parameter is specified (default: tokenized_merged.txt)"),
		OPT_BOOLEAN('r', "recursive", &checkSourceDirectoriesRecursive, "check source directories recursive (default - false)"),
		OPT_GROUP("All unmarked (positional) arguments are considered paths to files and folders with bases that need to be tokenized.\nExample command: 'theo t -d result base1.txt base2.txt'. More: github.com/Theodikes/theo-bases-soft"),
		OPT_END(),
	};
	struct argparse argparse;
	argparse_init(&argparse, options, usages, ARGPARSE_STOP_AT_NON_OPTION);
	int remainingArgumentsCount = argparse_parse(&argparse, argc, argv);
	if (remainingArgumentsCount < 1) {
		argparse_usage(&argparse);
		return -1;
	}

	// Засекаем время выполнения программы
	chrono::steady_clock::time_point begin = chrono::steady_clock::now();

	// Получаем список всех валидных файлов, которые надо токенизировать
	sourcefiles_info sourceFilesPaths = getSourceFilesFromUserInput(remainingArgumentsCount, argv, checkSourceDirectoriesRecursive);

	FILE* resultFile = NULL;
	processDestinationPath(&destinationPath, needMerge, &resultFile, "tokenized_merged.txt");

	if (string(resultStringPart) == "first") tokenizerParameters.onlyFirstPart = true;
	else if (string(resultStringPart) == "last") tokenizerParameters.onlyLastPart = true;
	else {
		cout << "Error: invalid 'part' parameter value - [" << resultStringPart << "]. Valid options: 'first', 'last' (without apostrophes)" << endl;
		exit(1);
	}

	processAllSourceFiles(sourceFilesPaths, needMerge, resultFile, toWstring(destinationPath), L"tokenized", tokenizeBufferLineByLine);

	chrono::steady_clock::time_point end = chrono::steady_clock::now();
	cout << endl << (sourceFilesPaths.size() == 1 ? "File" : "All files") << " tokenized successfully! Execution time: " << chrono::duration_cast<std::chrono::seconds>(end - begin).count() << "[s]\n" << endl;

	return ERROR_SUCCESS;
}

static size_t tokenizeBufferLineByLine(char* inputBuffer, size_t inputBufferLength, char* resultBuffer) {
	size_t currentStringStartPosInInputBuffer = 0; // Позиция начала текущей строки в буфере (номер байта)
	size_t resultBufferLength = 0;
	for (size_t pos = 0; pos < inputBufferLength; pos++) {
		if (inputBuffer[pos] == '\n') {
			// В данном случае в строке не надо учитывать \n, оно будет автоматически вставлено после нормализации
			size_t currentStringLength = pos - currentStringStartPosInInputBuffer;
			size_t separatorPos = 0;
			// Ищем разделитель в строке
			for (size_t i = currentStringStartPosInInputBuffer; i < pos; i++) {
				if (isSeparator(inputBuffer[i])) separatorPos = i;
			}
			// Если разделитель не найден, или он в самом конце или самом начале, строку пропускаем, она невалидна
			if (not separatorPos or separatorPos == pos - 1 or separatorPos == currentStringStartPosInInputBuffer) continue;
			// Если нам нужно получить первую часть, копируем в итоговый буфер строку до сепаратора
			if (tokenizerParameters.onlyFirstPart) {
				size_t partLength = separatorPos - currentStringStartPosInInputBuffer;
				memcpy(&resultBuffer[resultBufferLength], &inputBuffer[currentStringStartPosInInputBuffer], partLength);
				resultBufferLength += partLength;
			}
			// Если нам нужно получить вторую часть (пароль), копируем всё из строки от сепаратора (не вкл.) до конца
			else {
				size_t partLength = pos - (separatorPos + 1);
				memcpy(&resultBuffer[resultBufferLength], &inputBuffer[separatorPos + 1], partLength);
				resultBufferLength += partLength;
			}
			// Добавляем перенос строки в конец каждого взятого куска, чтобы они не слиплись в итоговом файле
			resultBuffer[resultBufferLength++] = '\n';
			// Начало следующей строки во входном буфере - следующий символ после текущей позиции
			currentStringStartPosInInputBuffer = pos + 1;
		}
	}

	return resultBufferLength;
}