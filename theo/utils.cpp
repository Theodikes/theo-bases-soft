#include "utils.hpp"

bool startsWith(const char* str, const char* prefix)
{
	size_t lenpre = strlen(prefix),
		lenstr = strlen(str);
	return lenstr < lenpre ? false : memcmp(prefix, str, lenpre) == 0;
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

char* path_join(const char* dir, const char* file) {

	size_t size = strlen(dir) + strlen(file) + 2;
	char* buf = new char[size];
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

const char* getFilenameFromPath(const char* pathToFile) {
	const char* fileNameWithExtension;
	(fileNameWithExtension = strrchr(pathToFile, '\\')) ? ++fileNameWithExtension : (fileNameWithExtension = pathToFile);

	return fileNameWithExtension;
}


bool processSourceFileOrDirectory(sparse_hash_set<string>* textFilesPaths, const char* path)
{

	DWORD dwAttributes = GetFileAttributes(path);
	if (dwAttributes == INVALID_FILE_ATTRIBUTES) {
		printf("File doesn`t exist: [%s]\n", path);
		return false;
	}
	if (!(dwAttributes & FILE_ATTRIBUTE_DIRECTORY)) return addFileToSourceList(textFilesPaths, path);

	WIN32_FIND_DATA fdFile;
	HANDLE hFind = NULL;

	char sPath[2048];
	sprintf(sPath, "%s\\*", path);

	if ((hFind = FindFirstFile(sPath, &fdFile)) == INVALID_HANDLE_VALUE)
	{
		printf("Path to directory not found: [%s]\n", path);
		return false;
	}

	do{
		if (strcmp(fdFile.cFileName, ".") != 0
			&& strcmp(fdFile.cFileName, "..") != 0) {
		processSourceFileOrDirectory(textFilesPaths, path_join(path, fdFile.cFileName));
		}
	} while (FindNextFile(hFind, &fdFile));

	FindClose(hFind);

	return true;
}

bool addFileToSourceList(sparse_hash_set<string>* sourceTextFilesPaths, const char* filePath) {
	if (!(endsWith(filePath, ".txt"))) return false;
	(*sourceTextFilesPaths).insert(string(filePath));
	return true;
}

string getFileNameWithoutExtension(string pathToFile) {
	return filesystem::path(pathToFile).stem().string();
}

unsigned long long get_hash(char* s) {
	unsigned long long h = 5381;
	int c;
	while ((c = *s++))
		h = ((h << 5) + h) + c;
	return h;
};


size_t getLinesCountInText(char* bytes) {
	size_t stringsCount = 0;
	while (*bytes++) if (*bytes == 10) stringsCount++;
	return stringsCount;
}

long long getFileSize(const char* pathToFile) {
	WIN32_FILE_ATTRIBUTE_DATA fileData;
	if (GetFileAttributesExA(pathToFile, GetFileExInfoStandard, &fileData))
		return (static_cast<ULONGLONG>(fileData.nFileSizeHigh) << sizeof(fileData.nFileSizeLow) * 8) |fileData.nFileSizeLow;
	return -1;
}

unsigned long long getAvailableMemoryInBytes(void) {
	MEMORYSTATUSEX ms;
	ms.dwLength = sizeof(ms);
	GlobalMemoryStatusEx(&ms);
	return ms.ullAvailPhys;
}

bool isFileValid(const char* pathToFile) {
	return GetFileAttributes(pathToFile) != INVALID_FILE_ATTRIBUTES;
}