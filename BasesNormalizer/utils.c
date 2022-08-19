#include "utils.h"

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
	char* buf = (char*)malloc(size * sizeof(char));
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


bool processSourceFileOrDirectory(const char** textFilesPaths, const char* path, size_t* filesCountPtr)
{

	DWORD dwAttributes = GetFileAttributes(path);
	if (dwAttributes == INVALID_FILE_ATTRIBUTES) {
		printf("File doesn`t exist: [%s]\n", path);
		return false;
	}
	if (!(dwAttributes & FILE_ATTRIBUTE_DIRECTORY)) return addFileToSourceList(textFilesPaths, path, filesCountPtr);

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
		processSourceFileOrDirectory(textFilesPaths, path_join(path, fdFile.cFileName), filesCountPtr);
		}
	} while (FindNextFile(hFind, &fdFile));

	FindClose(hFind);

	return true;
}

bool addFileToSourceList(const char** sourceTextFilesPaths, const char* filePath, size_t* filesCountPtr) {
	if (!(endsWith(filePath, ".txt"))) return false;
	sourceTextFilesPaths[(*filesCountPtr)++] = filePath;
}