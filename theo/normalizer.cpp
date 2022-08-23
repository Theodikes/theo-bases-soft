#include "utils.hpp"

FILE* getNormalizedBaseFilePtr(string pathToResultFolder, string pathToBaseFile) {
	
	/* Поскольку могут быть файлы с одинаковыми названиями из разных директорий, выбираем имя итогового, 
	нормализованного файла, пока не найдём незанятое (допустим, если нормализуется два файла из разных директорий с
	совпадающими именами, предположим, base1/test.txt и base2/test.txt, первый будет положен в итоговую директорию
	как test_normalized_1.txt, а второй как test_normalized_2.txt)*/
	fs::path resultFilePath;
	size_t i = 1;
	do {
		string resultFileName = getFileNameWithoutExtension(pathToBaseFile);
		resultFilePath = fs::path(pathToResultFolder) / fs::path(resultFileName + "_normalized_" + to_string(i++) + ".txt");
	} while (fs::exists(resultFilePath));

	// Записывать будем в открытый через fopen файл и в байтовом режиме для большей скорости
	FILE* normalizedBaseFilePtr = fopen(resultFilePath.string().c_str(), "wb+");
	if (normalizedBaseFilePtr == NULL) {
		cout << "Cannot create file for normalized base by path: [" << resultFilePath.string() << "]" << endl;
	}
	return normalizedBaseFilePtr;
}

/*Функция проверяет строку из базы на соответствие требуемым параметрам. Строка должна содержать валидный емейл,
разделитель ":" и пароль, длина которого находится в заданном диапазоне. */
bool isBaseStringSatisfyingConditions(char* string, int minPasswordLength, int maxPasswordLength, bool checkAscii, re_t emailRegex, re_t passwordRegex) {
	bool hasDelimeter = false;
	bool hasEmailSign = false;
	bool hasDotAfterEmailsSign = false; // Есть ли точка в емейле после символа '@'
	ushortest emailSignNumber = 0; // Количество символов '@' в емейле (если больше одного - невалидный)
	ushortest dotsNumber = 0;
	char currentSymbol; // Временная переменная для работы посимвольной работы со строкой
	int hasRegexpMatched = false; // Подошла ли строка под регулярку пользователя
	ushortest stringLength = (ushortest) strlen(string);
	
	if (stringLength > 80 || stringLength < 15) return false;

	// Если стоит флаг checkAscii, то все строки, в которых есть символы кроме английских букв, цифр и пунктуации - невалидны
	if (checkAscii) {
		for (ushortest i = 0; currentSymbol = string[i]; i++) {
			/* Если это командный символ, то есть имеет код от 0 до 31 или от 127 до 255, и при этом
			* это не символ с кодом 10(символ переноса строки, '\n') и не символ с кодом 13 (символ возврата каретки, '\r'),
			* то строка не является валидной строкой английского ascii и должна быть пропущена */
			if ((currentSymbol < 31 || currentSymbol > 126) && currentSymbol != 10 && currentSymbol != 13) return false;
		}
	}

	ushortest emailLength = 0;
	for (; currentSymbol = string[emailLength]; emailLength++) {
		if (currentSymbol == ':') {
			hasDelimeter = true;
			break;
		}
		if (currentSymbol == ';') {
			hasDelimeter = true;
			string[emailLength] = ':';
			break;
		}
		if (currentSymbol == '@') {
			emailSignNumber++;
			hasEmailSign = true;
		}
		if (currentSymbol == '.' && hasEmailSign) hasDotAfterEmailsSign = true;
	}
	/* Если емейл неадекватной длины, настолько, что похож на случайную строку, в которой присутствует символ '@' и '.',
	* игнорируем его */
	if (emailLength < 8 || emailLength > 50) return false;

	// Начинаем считать длину пароля с конца емела
	char* passwordStartPointer = &string[emailLength + 1];
	int passwordLength = (int)strlen(passwordStartPointer);
	if (passwordLength < minPasswordLength || passwordLength > maxPasswordLength) return false;

	if (!hasEmailSign || !hasDelimeter || emailSignNumber != 1 || !hasDotAfterEmailsSign) return false;
	
	if (emailRegex != NULL) {
		char* email = new char[emailLength + (ushortest) 1];
		memcpy(email, string, emailLength);
		email[emailLength] = '\0';
		int _ = re_matchp(emailRegex, email, &hasRegexpMatched);
		delete[] email;
		if (!hasRegexpMatched) return false;
	}

	if (passwordRegex != NULL) {
		int _ = re_matchp(emailRegex, passwordStartPointer, &hasRegexpMatched);
		if (!hasRegexpMatched) return false;
	}

	return true;
}

// Опции для ввода аргументов вызова программы из cmd, показыаемые пользователю при использовании флага --help или -h
static const char* const usages[] = {
	"theo n [options] [paths]",
	NULL,
};

int normalize(int argc, const char** argv) {
	char* pathToResultFolder = NULL;
	FILE* mergedResultFile = NULL;
	char* emailRegexString = NULL;
	char* passwordRegexString = NULL;
	re_t emailRegexPattern = NULL;
	re_t passwordRegexPattern = NULL;
	int minPasswordLength = 3;
	int maxPasswordLength = 40; // Все строки с паролем длиннее будут игнорироваться
	int checkAscii = false; // На самом деле bool...
	int needMerge = false; // Тоже bool

	struct argparse_option options[] = {
		OPT_HELP(),
		OPT_GROUP("Basic options"),
		OPT_STRING('d', "destination", &pathToResultFolder, "absolute or relative path to result folder"),
		OPT_STRING('e', "email-regex", &emailRegexString, "Regular expression for filtering emails (up to 30 characters)"),
		OPT_STRING('p', "password-regex", &passwordRegexString, "Regular expression for filtering passwords (up to 30 characters)"),
		OPT_INTEGER(0, "min", &minPasswordLength, "minimum password length"),
		OPT_INTEGER(0, "max", &maxPasswordLength, "maximum password length"),
		OPT_BOOLEAN('a', "check-ascii", &checkAscii, "ignore strings with invalid ascii characters"),
		OPT_BOOLEAN('m', "merge", &needMerge, "merge strings from all normalized files to one file\n\t\t\t\t  (By default, 'normalized_merged.txt' in destination folder)"),
		OPT_GROUP("All unmarked arguments are considered paths to files and folders with bases that need to be normalized."),
		OPT_END(),
	};
	struct argparse argparse;
	argparse_init(&argparse, options, usages, 0);
	int remainingArgumentsCount = argparse_parse(&argparse, argc, argv);

	// Файлы для нормализации (set, чтобы избежать повторной обработки одних и тех же файлов)
	robin_hood::unordered_flat_set<string> sourceFilesPaths;

	for (int i = 0; i < remainingArgumentsCount; i++) {
		processSourceFileOrDirectory(&sourceFilesPaths, argv[i]);
	}
	

	if (sourceFilesPaths.empty()) {
		cout << "Error: paths to bases not specified" << endl;
		exit(1);
	}


	if (pathToResultFolder == NULL) {
		cout << "Error: path to result directory not specified in args" << endl;
		exit(1);
	}

	if (emailRegexString != NULL) {
		emailRegexPattern = re_compile(emailRegexString);

		if (emailRegexPattern == NULL) {
			cout << "Error: invalid email regular expression" << endl;
			exit(1);
		}
	}

	if (passwordRegexString != NULL) {
		passwordRegexPattern = re_compile(passwordRegexString);

		if (passwordRegexPattern == NULL) {
			cout << "Error: invalid password regular expression" << endl;
			exit(1);
		}
	}

	if (needMerge) mergedResultFile = fopen(path_join(pathToResultFolder, "normalized_merged.txt"), "wb+");
	
	for (string sourceFilePath: sourceFilesPaths) {

		FILE* baseFilePointer = fopen(sourceFilePath.c_str(), "rb");
		if (baseFilePointer == NULL) {
			cout << "File is skipped. Cannot open [" << sourceFilePath << "] because of invalid path or due to security policy reasons." << endl;
			continue;
		}

		FILE* normalizedBaseFilePtr = NULL;
		if (!needMerge) {
			normalizedBaseFilePtr = getNormalizedBaseFilePtr(pathToResultFolder, sourceFilePath);
			if (normalizedBaseFilePtr == NULL) continue;
		}

		/* Читаем весь файл построчно и проверяем каждую строку на соответствие заданным условиям, 
		если соответствует - записываем в итоговый файл с нормализованной базой */
		char str[1024];
		while (fgets(str, 1023, baseFilePointer)) {
			if(isBaseStringSatisfyingConditions(str, minPasswordLength, maxPasswordLength, checkAscii, emailRegexPattern, passwordRegexPattern)) fputs(str, needMerge ? mergedResultFile : normalizedBaseFilePtr);
		}
		
		fclose(baseFilePointer);
		if (!needMerge) fclose(normalizedBaseFilePtr);
	}
	if (needMerge) fclose(mergedResultFile);

	cout << "\nBases normalized successfully!\n" << endl;
	return ERROR_SUCCESS;
}