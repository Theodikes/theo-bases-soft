#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdbool.h>
#include "argparse.h"

// Переменная для небольших положительных числовых значений, влезающих в байт памяти
#define ushortest unsigned char

bool startsWith(const char* pre, const char* str)
{
	size_t lenpre = strlen(pre),
		lenstr = strlen(str);
	return lenstr < lenpre ? false : memcmp(pre, str, lenpre) == 0;
}

bool endsWith(const char* str, const char* suffix)
{
	if (!str || !suffix)
		return 0;
	size_t lenstr = strlen(str);
	size_t lensuffix = strlen(suffix);
	if (lensuffix > lenstr)
		return 0;
	return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

// Функция для объединения абсолютного пути к папке и имени файла в абсолютный путь к файлу
char* path_join(const char* dir, const char* file) {
	#ifdef _WIN32
	#define PATH_JOIN_SEPERATOR   "\\"
	#else
	#define PATH_JOIN_SEPERATOR   "/"
	#endif

	size_t size = strlen(dir) + strlen(file) + 2;
	char* buf =(char* ) malloc(size * sizeof(char));
	if (NULL == buf) return NULL;

	strcpy(buf, dir);

	// add the sep if necessary
	if (!endsWith(dir, PATH_JOIN_SEPERATOR)) {
		strcat(buf, PATH_JOIN_SEPERATOR);
	}

	// remove the sep if necessary
	if (startsWith(file, PATH_JOIN_SEPERATOR)) {
		char* filecopy = _strdup(file);
		if (NULL == filecopy) {
			free(buf);
			return NULL;
		}
		strcat(buf, ++filecopy);
		free(--filecopy);
	}
	else {
		strcat(buf, file);
	}

	return buf;
}

char* getFilenameFromPath(char* pathToFile) {
	char* fileNameWithExtension;
	(fileNameWithExtension = strrchr(pathToFile, '\\')) ? ++fileNameWithExtension : (fileNameWithExtension = pathToFile);

	return fileNameWithExtension;
}

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

FILE* getNormalizedBaseFilePtr(char*pathToResultFolder, char* pathToBaseFile) {
	char resultFileName[256];
	strcpy(resultFileName, getFileNameWithoutTxtExtension(pathToBaseFile));
	strcat(resultFileName, "_normalized.txt");
	char* resultFilePath = path_join(pathToResultFolder, resultFileName);
	FILE* normalizedBaseFilePtr = fopen(resultFilePath, "w+");
	if (normalizedBaseFilePtr == NULL) {
		printf("Cannot create file for normalized base by path: %s\n", resultFilePath);
	}
	return normalizedBaseFilePtr;
}

/*Функция проверяет строку из базы на соответствие требуемым параметрам. Строка должна содержать валидный емейл,
разделитель ":" и пароль, длина которого находится в заданном диапазоне. */
bool isBaseStringSatisfyingConditions(char* string, int minPasswordLength, int maxPasswordLength) {
	bool hasDelimeter = false;
	bool hasEmailSign = false;
	ushortest emailSignNumber = 0;
	ushortest dotsNumber = 0;
	char currentSymbol;

	ushortest i;
	for (i = 0; currentSymbol = string[i]; i++) {
		if (currentSymbol == ':') {
			hasDelimeter = true;
			break;
		}
		if (currentSymbol == ';') {
			hasDelimeter = true;
			string[i] = ':';
			break;
		}
		if (currentSymbol == '@') {
			emailSignNumber++;
			hasEmailSign = true;
		}
		if (currentSymbol == '.' && hasEmailSign) dotsNumber++;
	}

	if (minPasswordLength > 1 || maxPasswordLength < 256) {
		char* passwordStartPointer = &string[i + 1];
		int passwordLength = (int)strlen(passwordStartPointer);
		if (passwordLength < minPasswordLength || passwordLength > maxPasswordLength) return false;
	}

	if (hasEmailSign && hasDelimeter && emailSignNumber == 1 && dotsNumber > 0) {
		return true;
	}

	return false;
}

// Опции для ввода аргументов вызова программы из cmd, показыаемые пользователю при использовании флага --help или -h
static const char* const usages[] = {
	"basic [options] [[--] args]",
	"basic [options]",
	NULL,
};

void main(int argc, char* argv[]) {

	char* pathToResultFolder = NULL;
	int minPasswordLength = 1;
	int maxPasswordLength = 256;

	struct argparse_option options[] = {
		OPT_HELP(),
		OPT_GROUP("Basic options"),
		OPT_STRING('d', "destination", &pathToResultFolder, "absolute or relative path to result folder", NULL, 0, 0),
		OPT_INTEGER('m', "min", &minPasswordLength, "minimum password length", NULL, 0, 0),
		OPT_INTEGER('M', "max", &maxPasswordLength, "maximum password length", NULL, 0, 0),
		OPT_GROUP("All unmarked arguments are considered paths to files with bases that need to be normalized"),
		OPT_END(),
	};
	struct argparse argparse;
	argparse_init(&argparse, options, usages, 0);
	argparse_describe(&argparse, "\nA brief description of what the program does and how it works.", "\nAdditional description of the program after the description of the arguments.");
	int remainingArgumentsCount = argparse_parse(&argparse, argc, argv);

	if (remainingArgumentsCount == 0) {
		puts("Error: paths to bases not specified");
		return;
	}


	if (pathToResultFolder == NULL) {
		puts("Error: path to result directory not specified in args");
		return;
	}
	

	for (int i = 0; i < remainingArgumentsCount; i++) {
		char* pathToBaseFile = argv[i];
		if (!endsWith(pathToBaseFile, ".txt")) {
			printf("Wrong path: Base must be a .txt file, but got %s\nFile %s has skipped.\n", pathToBaseFile, pathToBaseFile);
			continue;
		}

		FILE* baseFilePointer = fopen(pathToBaseFile, "r");
		if (baseFilePointer == NULL) {
			printf("File is skipped. Cannot open %s because of invalid path or due to security policy reasons.\n", pathToBaseFile);
			continue;
		}

		FILE* normalizedBaseFilePtr = getNormalizedBaseFilePtr(pathToResultFolder, pathToBaseFile);
		if (normalizedBaseFilePtr == NULL) continue;
		

		/* Читаем весь файл построчно и проверяем каждую строку на соответствие заданным условиям, 
		если соответствует - записываем в итоговый файл с нормализованной базой */
		char str[256];
		while (!feof(baseFilePointer)) {
			if (fgets(str, 255, baseFilePointer) != NULL) {
				if(isBaseStringSatisfyingConditions(str, minPasswordLength, maxPasswordLength)) fputs(str, normalizedBaseFilePtr);
			}
		}
		
		fclose(baseFilePointer);
		fclose(normalizedBaseFilePtr);
	}

	printf("\n\nBases normalized successfully\n\n");
}