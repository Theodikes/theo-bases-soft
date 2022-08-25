﻿#include "utils.hpp"

bool startsWith(const char* str, const char* prefix)
{
	size_t lenpre = strlen(prefix),
		lenstr = strlen(str);
	return lenstr < lenpre ? false : memcmp(prefix, str, lenpre) == 0;
}

bool endsWith(const char* str, const char* suffix)
{
	if (!str or !suffix) return 0;
	size_t lenstr = strlen(str);
	size_t lensuffix = strlen(suffix);
	if (lensuffix > lenstr) return 0;

	return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

string joinPaths(string dirPath, string filePath) {
	return (fs::path(dirPath) / fs::path(filePath)).string();
}


bool processSourceFileOrDirectory(robin_hood::unordered_flat_set<string>* textFilesPaths, string path)
{
	if (!isAnythingExistsByPath(path)) {
		cout << "File doesn`t exist: [" << path <<  "]" << endl;
		return false;
	}
	if (!isDirectory(path)) return addFileToSourceList(textFilesPaths, path);

	WIN32_FIND_DATA fdFile;
	HANDLE hFind = NULL;

	string searchPath = path + "\\*";

	if ((hFind = FindFirstFile(searchPath.c_str(), &fdFile)) == INVALID_HANDLE_VALUE)
	{
		cout << "Path to directory not found: [" << path << "]" << endl;;
		return false;
	}

	do{
		if (strcmp(fdFile.cFileName, ".") != 0
			and strcmp(fdFile.cFileName, "..") != 0) {
		processSourceFileOrDirectory(textFilesPaths, joinPaths(path, fdFile.cFileName));
		}
	} while (FindNextFile(hFind, &fdFile));

	FindClose(hFind);

	return true;
}

bool addFileToSourceList(robin_hood::unordered_flat_set<string>* sourceTextFilesPaths, string filePath) {
	if (!(endsWith(filePath.c_str(), ".txt"))) return false;
	(*sourceTextFilesPaths).insert(filePath);
	return true;
}

string getFileNameWithoutExtension(string pathToFile) {
	return filesystem::path(pathToFile).stem().string();
}


size_t getLinesCountInText(char* bytes) {
	size_t stringsCount = 0;
	while (*bytes++) if (*bytes == 10) stringsCount++;
	return stringsCount;
}

long long getFileSize(const char* pathToFile) {
	WIN32_FILE_ATTRIBUTE_DATA fileData;
	if (GetFileAttributesEx(pathToFile, GetFileExInfoStandard, &fileData))
		return (static_cast<ull>(fileData.nFileSizeHigh) << sizeof(fileData.nFileSizeLow) * 8) | fileData.nFileSizeLow;
	return -1;
}

ull getAvailableMemoryInBytes(void) {
	MEMORYSTATUSEX ms;
	ms.dwLength = sizeof(ms);
	GlobalMemoryStatusEx(&ms);
	return ms.ullAvailPhys;
}

bool isAnythingExistsByPath(string path) {
	return GetFileAttributes(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool isDirectory(string path) {
	return GetFileAttributes(path.c_str()) & FILE_ATTRIBUTE_DIRECTORY;
}

bool isValidRegex(string regularExpression) {
	try {
		regex re(regularExpression);
	}
	catch (const regex_error&) {
		return false;
	}
	return true;
}

string getWorkingDirectoryPath() {
	return fs::current_path().string();
}