#ifndef FILEUTIL_H
#define FILEUTIL_H

#include "mysync.h"
#include <stdbool.h>
#include <sys/stat.h>

// Check if a path points to a regular file
bool isRegularFile(const char *path);

// Copy a file from sourcePath to destinationPath
bool copyFile(const char *sourcePath, const char *destinationPath);

// Recursive syncing
void recursiveSyncDirectories(const char *sourceDir, const char *destinationDir, const struct Options *options);

void synchronizeFile(const char *sourcePath, const char *destinationPath, const struct stat *sourceStat, const struct stat *destStat, const struct Options *options);

#endif