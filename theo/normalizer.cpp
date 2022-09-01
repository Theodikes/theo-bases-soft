#include "utils.hpp"

// Возможные типы первой части строк в файле: емейлы (базы email:pass), номера (num:pass) и логины (log:pass)
static enum class StringFirstPartTypes {Email, Number, Login };

// Структура, содержащая все параметры, требуемые при проверке и нормализации строки
static struct StringsNormalizerAndValidatorParameters {
	StringFirstPartTypes firstPartType = StringFirstPartTypes::Email; // По умолчанию обрабатываются emailpass базы
	bool onlyAsciiSymbols = false; // Оставлять строки, в которых присутствуют только стандартные ascii-символы
	bool trim = false; // Убирать ли пробелы в начале и конце каждой строки
	size_t minAllLength = 10; // Минимальная длина всей строки, если меньше - отбрасываем, не обрабатываем в принципе
	size_t maxAllLength = 100; // Максимальная длина всей строки (включая и email/num/login, и пароль
	size_t minFirstPartLength = 5; // Минимальная длина первой части (email/log/pass)
	size_t maxFirstPartLength = 63; // Максимальная длина первой части
	size_t minPasswordLength = 4; // Минимальная длина второй части (пароля)
	size_t maxPasswordLength = 63; // Максимальная длина второй части (пароля)
	const char* separatorSymbols = ":;"; // Какие символы при проверке строки считать разделителем между частями
	char resultSeparator = ':'; // Унифицированный итоговый разделитель (будет вставлен разделителем в каждой строке)
	const char* firstPartNeededOccurency = NULL; // Строка, которая должна являться подстрокой первой части
	size_t firstPartOccurencyLength = 0; // Длина подстроки, если подстрока указана
	const char* passwordNeededOccurency = NULL; // Строка, которая должна являться подстрокой пароля
	size_t passwordOccurencyLength = 0; // Длина подстроки, если подстрока указана
	regex* firstPartRegexPtr = NULL; // Регулярное выражение, которому должна соответствовать первая часть строки
	regex* passwordRegexPtr = NULL; // Регулярное выражение, которому должен соответствовать пароль
} normalizerParameters;

// Создает и открывает в формате записи файл для текущего нормализуемого файла базы в итоговой директории
static FILE* getNormalizedBaseFilePtr(string pathToResultFolder, string pathToBaseFile);

/*Обрабатывает буфер с байтами, считанными из файла, делит их на строки, строки валидирует и нормализует.
* Возвращает длину итогового буфера, который надо записать в файл с нормализованными строками */
static size_t normalizeBufferLineByLine(char* inputBuffer, size_t inputBufferLength, char* resultBuffer);

/* Добавляет переданную строку в итоговый буфер и изменяет по указателю длину итогового буфера на новое значение
* (если строка удовлетворяет параметрам нормализации, находящимся в глобальной переменной normalizerParameters) */
static void addStringIfItSatisfyingConditions(char* string, size_t stringLength, char* resultBuffer, size_t* resultBufferLengthPtr);

/* Проверяет емейл на валидность, используя буфер байтов, из которых состоит емейл, и его длину, а также
* глобальную переменную с параметрами нормализации - normalizerParameters */
static bool isEmailValid(char* email, size_t emailLength);

/* Проверяет пароль на валидность, используя буфер байтов, из которых состоит емейл, и его длину, а также
* глобальную переменную с параметрами нормализации - normalizerParameters */
static bool isValidPassword(char* passwordStartPointer, size_t passwordLength);

// Проверяет, является ли одна строка подстрокой другой строки, используя быстрые системные функции
static bool hasOccurency(const char* string, size_t stringLength, const char* const substring, const size_t substringLength);

// Опции для ввода аргументов вызова программы из cmd, показыаемые пользователю при использовании флага --help или -h
static const char* const usages[] = {
	"theo n [options] [paths]",
	NULL,
};

int normalize(int argc, const char** argv) {
	const char* destinationDirectoryPath = ".";
	const char* firstPartRegexString = NULL; // Строка с пользовательским регулярным выражением для проверки емейлов
	const char* passwordRegexString = NULL; // Строка с пользовательским регулярным выражением для проверки паролей
	// Итоговый сепаратор, которым будут разделены email/log/num и password в нормализованной базе
	const char* resultSeperatorInputAsString = NULL; 
	bool needMerge = false; // Требуется ли объединять нормализованные строки со всех файлов в один итоговый
	// Требуется ли рекурсивно искать файлы для нормализации в переданных пользователем директориях
	bool checkSourceDirectoriesRecursive = false;
	const char* pathToMergedResultFile = "normalized_merged.txt"; // Путь к итоговому файлу, если объединять надо

	struct argparse_option options[] = {
		OPT_HELP(),
		OPT_GROUP("\nFiles options:\n"),
		OPT_STRING('d', "destination", &destinationDirectoryPath, "absolute or relative path to result folder (default: current directory"),
		OPT_BOOLEAN('m', "merge", &needMerge, "merge strings from all normalized files to one destination file\n\t\t\t\t  if this flag is set, 'destination' option will be ignored, use 'merged-file' instead"),
		OPT_STRING('f', "merged-file", &pathToMergedResultFile, "path to merged result file (default - 'normalized_merged.txt')"),
		OPT_BOOLEAN('r', "recursive", &checkSourceDirectoriesRecursive, "check source directories recursive (default - false)"),
		OPT_GROUP("All unmarked (positional) arguments are considered paths to files and folders with bases that need to be normalized.\nExample command: 'theo n -d result needNormalize1.txt needNormalize2.txt'. More: github.com/Theodikes/theo-bases-soft"),

		OPT_GROUP("\nBasic normalize options:\n"),
		OPT_BOOLEAN('a', "check-ascii", &(normalizerParameters.onlyAsciiSymbols), "ignore strings with invalid ascii characters (default - false)"),
		OPT_BOOLEAN('t', "trim", &(normalizerParameters.trim), "remove spaces at the beginning and at the end of each line (default - false)"),
		OPT_STRING('s', "separators", &(normalizerParameters.separatorSymbols), "a string containing possible delimiter characters\n\t\t\t\t  (between email and password), enter without spaces. (default - \":;\")"),
		OPT_STRING(0, "result-sep", &resultSeperatorInputAsString, "separator symbol after normalization (default - ':')"),
		OPT_INTEGER(0, "min-pass", &(normalizerParameters.minPasswordLength), "minimum password length (default - 4)"),
		OPT_INTEGER(0, "max-pass", &(normalizerParameters.maxPasswordLength), "maximum password length (default - 63)"),

		OPT_GROUP("\nAdditional (rarely used) normalize options:\n"),
		OPT_INTEGER(0, "min-all", &(normalizerParameters.minAllLength), "minimum length of the entire string (default - 10)"),
		OPT_INTEGER(0, "max-all", &(normalizerParameters.maxAllLength), "maximum length of the entire string (default - 100)"),
		OPT_INTEGER(0, "min-fp", &(normalizerParameters.minFirstPartLength), "minimum length of the string first part (email/num/login) (default - 5)"),
		OPT_INTEGER(0, "max-fp", &(normalizerParameters.maxFirstPartLength), "length of the string first part (email/num/login) (default - 63)"),
		OPT_STRING('e', "fp-regex", &firstPartRegexString, "regular expression for filtering emails/logs/nums (first part of every string)"),
		OPT_STRING('p', "password-regex", &passwordRegexString, "regular expression for filtering passwords"),
		OPT_STRING(0, "fp-occurency", &(normalizerParameters.firstPartNeededOccurency), "Mandatory occurrence first part of string (email/num/login).\n\t\t\t\t  If possible, use istead of regex, because its much faster"),
		OPT_STRING(0, "password-occurency", &(normalizerParameters.passwordNeededOccurency), "Mandatory occurrence in every string password.\n\t\t\t\t  If possible, use istead of regex, because its much faster"),
		OPT_END(),
	};
	struct argparse argparse;
	argparse_init(&argparse, options, usages, ARGPARSE_STOP_AT_NON_OPTION);
	int remainingArgumentsCount = argparse_parse(&argparse, argc, argv);
	if (remainingArgumentsCount < 1) {
		argparse_usage(&argparse);
		return -1;
	}

	// Файлы для нормализации (set, чтобы избежать повторной обработки одних и тех же файлов)
	robin_hood::unordered_flat_set<string> sourceFilesPaths;

	for (int i = 0; i < remainingArgumentsCount; i++) {
		processSourceFileOrDirectory(&sourceFilesPaths, argv[i], checkSourceDirectoriesRecursive);
	}

	if (sourceFilesPaths.empty()) {
		cout << "Error: paths to bases not specified" << endl;
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
		cout << "Destination directory successfully created, start spitting file into it" << endl;
	}


	if (resultSeperatorInputAsString != NULL) normalizerParameters.resultSeparator = resultSeperatorInputAsString[0];

	/* Проверяем валидность введённого пользователем регулярного выражения для email/num/log и сохраняем его
	* в normalizerOptions по указателю. Столь сложная конструкция обусловлена тем, что сохранить напрямую указатель
	* на новосозданный regex, не инициализируя переменную, невозможно, поскольку значение по указателю без ссылок
	* на него автоматически очистится */
	regex firstPartRegexPattern;
	if (firstPartRegexString != NULL) {
		if(not isValidRegex(firstPartRegexString)) {
			cout << "Error: invalid email regular expression" << endl;
			exit(1);
		}
		firstPartRegexPattern = regex(firstPartRegexString);
		normalizerParameters.firstPartRegexPtr= &firstPartRegexPattern;
	}

	// Проверяем валидность введённого пользователем регулярного выражения для пароля и сохраняем его
	regex passwordRegexPattern;
	if (passwordRegexString != NULL) {
		if(not isValidRegex(passwordRegexString)) {
			cout << "Error: invalid password regular expression" << endl;
			exit(1);
		}
		passwordRegexPattern = regex(passwordRegexString);
		normalizerParameters.passwordRegexPtr = &passwordRegexPattern;
	}
	
	/* Если нам в дальнейшем будет требоваться проверять подстроки-вхождения в пароли или емейлы,
	* заранее один раз вычисляем их длину и сохраняем в параметры, чтобы не считать её при обработке каждой строки */
	if (normalizerParameters.firstPartNeededOccurency != NULL)
		normalizerParameters.firstPartOccurencyLength = strlen(normalizerParameters.firstPartNeededOccurency);
		if (normalizerParameters.passwordNeededOccurency != NULL)
		normalizerParameters.passwordOccurencyLength = strlen(normalizerParameters.passwordNeededOccurency);

	FILE* resultFile = NULL;
	if (needMerge) resultFile = fopen(pathToMergedResultFile, "wb+");

	chrono::steady_clock::time_point begin = chrono::steady_clock::now();
	
	for (string sourceFilePath: sourceFilesPaths) {

		FILE* inputBaseFilePointer = fopen(sourceFilePath.c_str(), "rb");
		if (inputBaseFilePointer == NULL) {
			cout << "File is skipped. Cannot open [" << sourceFilePath << "] because of invalid path or due to security policy reasons." << endl;
			continue;
		}

		/* Если мы не складываем всё в один файл, то каждую итерацию цикла создаём под каждый входной файл
		* свой итоговый файл, в котором будут находиться нормализованные строки из вхождного */
		if (!needMerge) {
			resultFile = getNormalizedBaseFilePtr(destinationDirectoryPath, sourceFilePath);
			if (resultFile == NULL) {
				cout << "Error: cannot open result file [" << joinPaths(destinationDirectoryPath, sourceFilePath) << "] in write mode" << endl;
				continue;
			}
		}

		// Обрабатываем весь файл почанково и записываем все нормализованные строки в итоговый файл
		processFileByChunks(inputBaseFilePointer, resultFile, normalizeBufferLineByLine);
		
	}
	_fcloseall(); // Закрываем все итоговые файлы

	chrono::steady_clock::time_point end = chrono::steady_clock::now();
	cout << "\nBases normalized successfully! Execution time: " << chrono::duration_cast<std::chrono::seconds>(end - begin).count() << "[s]\n" << endl;
	return ERROR_SUCCESS;
}



static FILE* getNormalizedBaseFilePtr(string pathToResultFolder, string pathToBaseFile) {

	/* Поскольку могут быть файлы с одинаковыми названиями из разных директорий, выбираем имя итогового,
	нормализованного файла, пока не найдём незанятое (допустим, если нормализуется два файла из разных директорий с
	совпадающими именами, предположим, base1/test.txt и base2/test.txt, первый будет положен в итоговую директорию
	как test_normalized_1.txt, а второй как test_normalized_2.txt)*/
	string resultFilePath;
	size_t i = 1;
	do {
		string resultFileName = getFileNameWithoutExtension(pathToBaseFile);
		resultFilePath = joinPaths(pathToResultFolder, resultFileName + "_normalized_" + to_string(i++) + ".txt");
	} while (isAnythingExistsByPath(resultFilePath));

	// Записывать будем в открытый через fopen файл и в байтовом режиме для большей скорости
	FILE* normalizedBaseFilePtr = fopen(resultFilePath.c_str(), "wb+");
	if (normalizedBaseFilePtr == NULL) {
		cout << "Cannot create file for normalized base by path: [" << resultFilePath << "]" << endl;
	}
	return normalizedBaseFilePtr;
}

static bool hasOccurency(const char* string, size_t stringLength, const char* const substring, const size_t substringLength) {
	if (string == NULL or stringLength == 0 or substring == NULL or substringLength == 0) return false; 

	for (const char* s = string;
		stringLength >= substringLength;
		++s, --stringLength) {
		if (!memcmp(s, substring, substringLength)) {
			return true;
		}
	}
	return false;
}

static bool isEmailValid(char* email, size_t emailLength) {

	bool hasDotAfterEmailsSign = false; // Есть ли точка в емейле после символа '@'
	size_t emailSignNumber = 0; // Количество символов '@' в емейле (если больше или меньше одного - невалидный)

	for (size_t i = 0; i < emailLength; i++) {
		if (email[i] == '@') {
			emailSignNumber++;
			continue;
		}
		if (email[i] == '.' and emailSignNumber) hasDotAfterEmailsSign = true;

		// Приводим весь емейл к нижнему регистру
		email[i] = tolower(email[i]);
	}

	// Проверяем, что в строке емейла один знак собачки '@' и присутствует точка после него (в домене)
	if (emailSignNumber != 1 or !hasDotAfterEmailsSign) return false;

	return true;
}

static bool isValidPassword(char* passwordStartPointer, size_t passwordLength) {

	// Проверяем соответствие длины пароля заданным пользователем параметрам
	if (passwordLength < normalizerParameters.minPasswordLength or passwordLength > normalizerParameters.maxPasswordLength) return false;

	// Если нужно проверить вхождение какой-либо строки в строку пароля, проверяем
	if (normalizerParameters.passwordNeededOccurency != NULL and !hasOccurency(passwordStartPointer, passwordLength, normalizerParameters.passwordNeededOccurency, normalizerParameters.passwordOccurencyLength)) return false;

	// Проверяем соответствие пароля регулярке, введённой пользователем (если таковая есть)
	if (normalizerParameters.passwordRegexPtr != NULL and not regex_search(passwordStartPointer, &passwordStartPointer[passwordLength], *(normalizerParameters.passwordRegexPtr))) return false;

	return true;
}

static void addStringIfItSatisfyingConditions(char* string, size_t stringLength, char* resultBuffer, size_t* resultBufferLengthPtr) {
	bool hasDelimeter = false;

	// Удаляем пробельные символы в начале и конце строки, кроме переноса строки в самом конце
	if (normalizerParameters.trim) {
		// Просто пропускаем символы в начале, если они пробельные, каждый раз перенося указатель на следующий символ
		for (size_t i = 0; isspace(*string) and i < stringLength; i++) {
			string++;
			stringLength--;
		}
		// Если пробелы в конце строки - просто уменьшаем длину строки
		for (size_t i = stringLength - 1; isspace(string[stringLength - 1]) and stringLength > 1; i--) stringLength--;
	}

	if (stringLength > normalizerParameters.maxAllLength or stringLength < normalizerParameters.minAllLength) return;

	// Если стоит флаг onlyAscii, то все строки, в которых есть символы кроме английских букв, цифр и пунктуации - невалидны
	if (normalizerParameters.onlyAsciiSymbols){
		for (size_t i = 0; i < stringLength; i++) {
			/* Если это командный символ, то есть имеет код от 0 до 31 или от 127 до 255, и при этом
			* это не символ с кодом 10(символ переноса строки, '\n') и не символ с кодом 13 (символ возврата каретки, '\r'),
			* то строка не является валидной строкой английского ascii и должна быть пропущена */
			if ((string[i] < 31 or string[i] > 126) and string[i] != 10 and string[i] != 13) return;
		}
	}

	// Считаем длину части строки до разделителя (разделитель между email/num/log и password) и проверяем, есть ли он
	size_t firstPartLength = 0;
	for (; firstPartLength < stringLength; firstPartLength++) {
		// Если символ - разделитель строки на емейл и пароль, заменяем его на стандартный символ ":"
		if (strchr(normalizerParameters.separatorSymbols, string[firstPartLength])) {
			hasDelimeter = true;
			string[firstPartLength] = normalizerParameters.resultSeparator;
			break;
		}
	}
	if (not hasDelimeter) return; // Если в строке не найден разделитель, она невалидна

	// Проверяем нормальность длины первой части строки (email/login/num)
	if (firstPartLength < normalizerParameters.minFirstPartLength or firstPartLength > normalizerParameters.maxFirstPartLength) return;

	// Если мы проверяем email:pass и емейл невалиден, то строка невалидна вся
	if (normalizerParameters.firstPartType == StringFirstPartTypes::Email and not isEmailValid(string, firstPartLength)) return; 

	/* Если нужно проверить вхождение какой - либо строки в строку email / login / num, проверяем.
	* Так же важен порядок: сначала проверка валидность емейла/номера, потом проверка подстрок и регулярных выражений,
	* так как эти проверки занимают несоизмеримо больше времени для каждой строки */
	if (normalizerParameters.firstPartNeededOccurency != NULL and !hasOccurency(string, firstPartLength, normalizerParameters.firstPartNeededOccurency, normalizerParameters.firstPartOccurencyLength)) return;

	// Если нужно проверить, подходит ли строка с email/login/num под пользовательское регулярное выражение, проверяем
	if (normalizerParameters.firstPartRegexPtr != NULL and not regex_search(string, &string[firstPartLength], *(normalizerParameters.firstPartRegexPtr))) return;
	
	 // Добавляем единицу, поскольку есть ещё сепаратор, который не должен попасть в пароль
	char* passwordStartPtr = &string[firstPartLength + 1];
	// Вычитаем ещё единицу, поскольку сепаратор в середине не должен попасть в пароль
	size_t passwordLength = stringLength - firstPartLength - 1;
	if (not isValidPassword(passwordStartPtr, passwordLength)) return;

	// Добавляем обязательный перенос строки в конце, и увеличиваем длину строки на единицу, если переноса не было
	if (string[stringLength - 1] != '\n') string[stringLength++] = '\n';

	// Копируем строку в итоговый буфер и одновременно увеличиваем переменную с длиной буфера по указателю
	memcpy(&resultBuffer[*resultBufferLengthPtr], string, stringLength);
	*resultBufferLengthPtr += stringLength;
}

static size_t normalizeBufferLineByLine(char* inputBuffer, size_t inputBufferLength, char* resultBuffer) {
	size_t currentStringStartPosInInputBuffer = 0; // Позиция начала текущей строки в буфере (номер байта)
	size_t resultBufferLength = 0;
	for (size_t pos = 0; pos < inputBufferLength; pos++) {
		if (inputBuffer[pos] == '\n') {
			// В данном случае в строке не надо учитывать \n, оно будет автоматически вставлено после нормализации
			size_t currentStringLength = pos - currentStringStartPosInInputBuffer;
			addStringIfItSatisfyingConditions(&inputBuffer[currentStringStartPosInInputBuffer], currentStringLength, resultBuffer, &resultBufferLength);
			// Начало следующей строки - следующий символ после тукущей позиции
			currentStringStartPosInInputBuffer = pos + 1;
		}
	}

	return resultBufferLength;
}