#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "fileutil.h"
#include "mysync.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <linux/limits.h>
#include <sys/time.h>
#include <utime.h>

bool isRegularFile(const char *path) {
    struct stat path_stat;
    if (stat(path, &path_stat) != 0) {
        perror("Error getting file status");
        return false;
    }
    return S_ISREG(path_stat.st_mode) || S_ISDIR(path_stat.st_mode);
}

bool copyFile(const char *sourcePath, const char *destinationPath) {
    int sourceFile = open(sourcePath, O_RDONLY);
    if (sourceFile == -1) {
        perror("Error opening source file");
        return false;
    }

    int destinationFile = open(destinationPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (destinationFile == -1) {
        perror("Error opening destination file");
        close(sourceFile);
        return false;
    }

    char buffer[BUFSIZ];
    ssize_t bytesRead, bytesWritten;
    while ((bytesRead = read(sourceFile, buffer, BUFSIZ)) > 0) {
        bytesWritten = write(destinationFile, buffer, bytesRead);
        if (bytesWritten != bytesRead) {
            perror("Error writing to destination file");
            close(sourceFile);
            close(destinationFile);
            return false;
        }
    }

    if (bytesRead == -1) {
        perror("Error reading from source file");
        close(sourceFile);
        close(destinationFile);
        return false;
    }

    struct stat sourceFileStat;
    if (stat(sourcePath, &sourceFileStat) == -1) {
        perror("Error getting source file status");
        close(sourceFile);
        close(destinationFile);
        return false;
    }

    close(sourceFile);

    struct utimbuf times;
    times.actime = sourceFileStat.st_atime;
    times.modtime = sourceFileStat.st_mtime;

    // Setting modification time 
    if (utime(destinationPath, &times) == -1) {
        perror("Error setting modification time for destination file");
        close(destinationFile);
        return false;
    }

    // Setting file permissions 
    if (fchmod(destinationFile, sourceFileStat.st_mode) == -1) {
        perror("Error setting file permissions for destination file");
        close(destinationFile);
        return false;
    }

    close(destinationFile);
    return true;
}

void recursiveSyncDirectories(const char *sourceDir, const char *destinationDir, const struct Options *options) {
    DIR *sourceDirPtr = opendir(sourceDir);
    if (sourceDirPtr == NULL) {
        perror("Error opening source directory");
        return;
    }

    if (mkdir(destinationDir, 0777) == -1 && errno != EEXIST) {
        perror("Error creating destination directory");
        closedir(sourceDirPtr);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(sourceDirPtr)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue; 
        }

        char subSourceFilePath[PATH_MAX];
        char subDestFilePath[PATH_MAX];

        snprintf(subSourceFilePath, PATH_MAX, "%s/%s", sourceDir, entry->d_name);
        snprintf(subDestFilePath, PATH_MAX, "%s/%s", destinationDir, entry->d_name);

        struct stat fileStat;
        if (stat(subSourceFilePath, &fileStat) == -1) {
            perror("Error getting file status");
            continue;
        }

        if (S_ISREG(fileStat.st_mode)) {
            if (!options->dryRun) {
                copyFile(subSourceFilePath, subDestFilePath);
            }

            if (options->verbose) {
                printf("Copied: %s\n", subSourceFilePath);
            }
        } else if (S_ISDIR(fileStat.st_mode) && options->recursive) {
            recursiveSyncDirectories(subSourceFilePath, subDestFilePath, options);
        }
    }

    closedir(sourceDirPtr);
}

void synchronizeFile(const char *sourcePath, const char *destinationPath, const struct stat *sourceStat, const struct stat *destStat, const struct Options *options) {
    if (sourceStat->st_mtime > destStat->st_mtime || !S_ISREG(destStat->st_mode)) {
        if (!options->dryRun) {
            if (copyFile(sourcePath, destinationPath)) {
                if (options->verbose) {
                    printf("Synchronized: %s\n", sourcePath);
                }
            } else {
                fprintf(stderr, "Failed to synchronize: %s\n", sourcePath);
            }
        } else {
            if (options->verbose) {
                printf("Dry run - Identified for synchronization: %s\n", sourcePath);
            }
        }
    } else if (options->verbose) {
        printf("Up to date: %s\n", sourcePath);
    }
}

