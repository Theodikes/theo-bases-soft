#include "utils.h"


/* Функция для получения имени файла без расширения .txt из абсолютного пути. 
*Используется, чтобы получить чистое название файла и к нему можно было что-нибудь добавлять.
Работает ТОЛЬКО с файлами, имеющими расширение txt. */
char* getFileNameWithoutTxtExtension(char* pathToFile) {
	char* fileNameWithExtension = getFilenameFromPath(pathToFile);

	char* fileName = (char*)malloc((strlen(fileNameWithExtension) + 2) * sizeof(char));
	if (fileName == NULL) {
		puts("Cannot allocate memory to store filename");
		exit(0);
	}

	for (ushortest i = 0; fileNameWithExtension[i]; i++) {
		if (fileNameWithExtension[i] == '.' && fileNameWithExtension[i + 4] == '\0') {
			fileName[i] = '\0';
			break;
		}
		else fileName[i] = fileNameWithExtension[i];
	}

	return fileName;
}

FILE* getNormalizedBaseFilePtr(char*pathToResultFolder, char* pathToBaseFile, int fileNumber) {
	char resultFileName[256];
	sprintf(resultFileName, "%s_normalized_%d.txt", getFileNameWithoutTxtExtension(pathToBaseFile), fileNumber);
	char* resultFilePath = path_join(pathToResultFolder, resultFileName);
	FILE* normalizedBaseFilePtr = fopen(resultFilePath, "wb+");
	if (normalizedBaseFilePtr == NULL) {
		printf("Cannot create file for normalized base by path: [%s]\n", resultFilePath);
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
	ushortest stringLength = strlen(string);
	
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
		char* email = (char*)malloc(sizeof(char) * (emailLength + 1));
		memcpy(email, string, emailLength);
		email[emailLength] = '\0';
		int _ = re_matchp(emailRegex, email, &hasRegexpMatched);
		free(email);
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
	"basic [options] [[--] args]",
	"basic [options]",
	NULL,
};

void main(int argc, char* argv[]) {
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
		OPT_STRING('d', "destination", &pathToResultFolder, "absolute or relative path to result folder", NULL, 0, 0),
		OPT_STRING('e', "email-regex", &emailRegexString, "Regular expression for filtering emails (up to 30 characters)", NULL, 0, 0),
		OPT_STRING('p', "password-regex", &passwordRegexString, "Regular expression for filtering passwords (up to 30 characters)", NULL, 0, 0),
		OPT_INTEGER(0, "min", &minPasswordLength, "minimum password length", NULL, 0, 0),
		OPT_INTEGER(0, "max", &maxPasswordLength, "maximum password length", NULL, 0, 0),
		OPT_BOOLEAN('a', "check-ascii", &checkAscii, "ignore strings with invalid ascii characters", NULL, 0, 0),
		OPT_BOOLEAN('m', "merge", &needMerge, "merge strings from all normalized files to one file ('merged.txt')", NULL, 0, 0),
		OPT_GROUP("All unmarked arguments are considered paths to files and folders with bases that need to be normalized. \nYou can combine this files and flag 'source' with directory"),
		OPT_END(),
	};
	struct argparse argparse;
	argparse_init(&argparse, options, usages, 0);
	argparse_describe(&argparse, "\nA brief description of what the program does and how it works.", "\nAdditional description of the program after the description of the arguments.");
	int remainingArgumentsCount = argparse_parse(&argparse, argc, argv);


	size_t sourceFilesCount = 0; // Количество файлов, которые будут нормализоваться
	char** sourceFiles = (char**)calloc(65536, sizeof(char*));

	for (int i = 0; i < remainingArgumentsCount; i++) {
		processSourceFileOrDirectory(sourceFiles, argv[i], &sourceFilesCount);
	}
	

	if (sourceFilesCount == 0) {
		puts("Error: paths to bases not specified");
		exit(1);
	}


	if (pathToResultFolder == NULL) {
		puts("Error: path to result directory not specified in args");
		exit(1);
	}

	if (emailRegexString != NULL) {
		emailRegexPattern = re_compile(emailRegexString);

		if (emailRegexPattern == NULL) {
			puts("Error: invalid email regular expression");
			exit(1);
		}
	}

	if (passwordRegexString != NULL) {
		passwordRegexPattern = re_compile(passwordRegexString);

		if (passwordRegexPattern == NULL) {
			puts("Error: invalid password regular expression");
			exit(1);
		}
	}

	if (needMerge) mergedResultFile = fopen(path_join(pathToResultFolder, "merged.txt"), "wb+");
	
	for (int i = 0; i < sourceFilesCount; i++) {
		char* pathToBaseFile = sourceFiles[i];

		if (!endsWith(pathToBaseFile, ".txt")) {
			printf("Wrong path: Base must be a .txt file, but got [%s]\nFile [%s] has skipped.\n", pathToBaseFile, pathToBaseFile);
			continue;
		}

		FILE* baseFilePointer = fopen(pathToBaseFile, "rb");
		if (baseFilePointer == NULL) {
			printf("File is skipped. Cannot open [%s] because of invalid path or due to security policy reasons.\n", pathToBaseFile);
			continue;
		}

		FILE* normalizedBaseFilePtr = getNormalizedBaseFilePtr(pathToResultFolder, pathToBaseFile, i);
		if (normalizedBaseFilePtr == NULL) continue;

		/* Читаем весь файл построчно и проверяем каждую строку на соответствие заданным условиям, 
		если соответствует - записываем в итоговый файл с нормализованной базой */
		char str[1024];
		while (fgets(str, 1023, baseFilePointer)) {
			if(isBaseStringSatisfyingConditions(str, minPasswordLength, maxPasswordLength, checkAscii, emailRegexPattern, passwordRegexPattern)) fputs(str, mergedResultFile);
		}
		
		fclose(baseFilePointer);
		if (!mergedResultFile) fclose(normalizedBaseFilePtr);
	}
	if (mergedResultFile) fclose(mergedResultFile);

	printf("\n\nBases normalized successfully!\n\n");
}