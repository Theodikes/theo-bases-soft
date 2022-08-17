#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdbool.h>
#include "argparse.h"
#include <Windows.h>

// Переменная для небольших положительных числовых значений, влезающих в байт памяти
#define ushortest unsigned char
// Максимальная длина пути к файлу в Windows
#define MAX_PATH 1024
// Разделители путей в различных системах
#ifdef _WIN32
#define PATH_JOIN_SEPARATOR   "\\"
#else
#define PATH_JOIN_SEPARATOR   "/"
#endif

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

	size_t size = strlen(dir) + strlen(file) + 2;
	char* buf =(char* ) malloc(size * sizeof(char));
	if (NULL == buf) return NULL;

	strcpy(buf, dir);

	// add the sep if necessary
	if (!endsWith(dir, PATH_JOIN_SEPARATOR)) {
		strcat(buf, PATH_JOIN_SEPARATOR);
	}

	// remove the sep if necessary
	if (startsWith(file, PATH_JOIN_SEPARATOR)) {
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

/* Возвращает массив, содержащий пути ко всем txt - файлам в указанной директории.
Кроме того, меняет значение переменной filesCount по указателю, чтобы вне функции стало известно, сколько всего
в возвращенном массиве элементов (указателей на строки, каждая строка - путь к файлу для нормализации). */
char** getDirectoryTextFilesList(const char* dirPath, unsigned short* filesCount)
{
	char** textFilesInDirectory = (char**)malloc(1024 * sizeof(char*));
	WIN32_FIND_DATA fdFile;
	HANDLE hFind = NULL;

	char sPath[2048];
	sprintf(sPath, "%s\\*.txt", dirPath);

	if ((hFind = FindFirstFile(sPath, &fdFile)) == INVALID_HANDLE_VALUE)
	{
		printf("Path to directory not found: [%s]\n", dirPath);
		return false;
	}

	do
	{
		if (strcmp(fdFile.cFileName, ".") != 0
			&& strcmp(fdFile.cFileName, "..") != 0) {
			char* baseFilePath = (char*)malloc(MAX_PATH * sizeof(char));
			if (baseFilePath == NULL) {
				puts("Cannot allocate memory to base file path string");
				exit(1);
			}
			strcpy(baseFilePath, path_join(dirPath, fdFile.cFileName));
			*(textFilesInDirectory + *filesCount) = baseFilePath;
			(*filesCount)++;
		}
	} while (FindNextFile(hFind, &fdFile));

	FindClose(hFind);

	return textFilesInDirectory;
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

FILE* getNormalizedBaseFilePtr(char*pathToResultFolder, char* pathToBaseFile, int fileNumber) {
	char resultFileName[256];
	sprintf(resultFileName, "%s_normalized_%d.txt", getFileNameWithoutTxtExtension(pathToBaseFile), fileNumber);
	char* resultFilePath = path_join(pathToResultFolder, resultFileName);
	FILE* normalizedBaseFilePtr = fopen(resultFilePath, "w+");
	if (normalizedBaseFilePtr == NULL) {
		printf("Cannot create file for normalized base by path: [%s]\n", resultFilePath);
	}
	return normalizedBaseFilePtr;
}

/*Функция проверяет строку из базы на соответствие требуемым параметрам. Строка должна содержать валидный емейл,
разделитель ":" и пароль, длина которого находится в заданном диапазоне. */
bool isBaseStringSatisfyingConditions(char* string, int minPasswordLength, int maxPasswordLength, bool checkAscii) {
	bool hasDelimeter = false;
	bool hasEmailSign = false;
	bool hasDotAfterEmailsSign = false; // Есть ли точка в емейле после символа '@'
	ushortest emailSignNumber = 0; // Количество символов '@' в емейле (если больше одного - невалидный)
	ushortest dotsNumber = 0;
	char currentSymbol; // Временная переменная для работы посимвольной работы со строкой
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

	if (hasEmailSign && hasDelimeter && emailSignNumber == 1 && hasDotAfterEmailsSign) {
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
	char* pathToSourceFolder = NULL;
	int minPasswordLength = 3;
	int maxPasswordLength = 40; // Все строки с паролем длиннее будут игнорироваться
	int checkAscii = false; // На самом деле bool...
	unsigned short sourceFilesCount = 0;
	char** sourceFiles = NULL;

	struct argparse_option options[] = {
		OPT_HELP(),
		OPT_GROUP("Basic options"),
		OPT_STRING('s', "source", &pathToSourceFolder, "absolute or relative path to folder with text bases", NULL, 0, 0),
		OPT_STRING('d', "destination", &pathToResultFolder, "absolute or relative path to result folder", NULL, 0, 0),
		OPT_INTEGER('m', "min", &minPasswordLength, "minimum password length", NULL, 0, 0),
		OPT_INTEGER('M', "max", &maxPasswordLength, "maximum password length", NULL, 0, 0),
		OPT_BOOLEAN("ca", "check-ascii", &checkAscii, "ignore strings with invalid ascii characters", NULL, 0, 0),
		OPT_GROUP("All unmarked arguments are considered paths to files with bases that need to be normalized. \nYou can combine this files and flag 'source' with directory"),
		OPT_END(),
	};
	struct argparse argparse;
	argparse_init(&argparse, options, usages, 0);
	argparse_describe(&argparse, "\nA brief description of what the program does and how it works.", "\nAdditional description of the program after the description of the arguments.");
	int remainingArgumentsCount = argparse_parse(&argparse, argc, argv);

	// Если пользователем указан путь к папке, откуда надо брать базы для нормализации, и там они есть - нормализуем их
	if (pathToSourceFolder != NULL) {
		sourceFiles = getDirectoryTextFilesList(pathToSourceFolder, &sourceFilesCount);
		for (int i = 0; i < remainingArgumentsCount; i++) {
			sourceFiles[sourceFilesCount + i] = argv[i];
		}
		sourceFilesCount += argc;
	}
	if (sourceFilesCount == 0) {
		sourceFilesCount = remainingArgumentsCount;
		sourceFiles = argv;
	}
	

	if (sourceFilesCount == 0) {
		puts("Error: paths to bases not specified");
		return;
	}


	if (pathToResultFolder == NULL) {
		puts("Error: path to result directory not specified in args");
		return;
	}
	
	for (int i = 0; i < sourceFilesCount; i++) {
		char* pathToBaseFile = sourceFiles[i];

		if (!endsWith(pathToBaseFile, ".txt")) {
			printf("Wrong path: Base must be a .txt file, but got [%s]\nFile [%s] has skipped.\n", pathToBaseFile, pathToBaseFile);
			continue;
		}

		FILE* baseFilePointer = fopen(pathToBaseFile, "r");
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
			if(isBaseStringSatisfyingConditions(str, minPasswordLength, maxPasswordLength, checkAscii)) fputs(str, normalizedBaseFilePtr);
		}
		
		fclose(baseFilePointer);
		fclose(normalizedBaseFilePtr);
	}

	printf("\n\nBases normalized successfully!\n\n");
}