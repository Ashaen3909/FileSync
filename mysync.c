#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "fileutil.h"
#include "mysync.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <fnmatch.h>
#include <sys/stat.h>
#include <glob.h> // pain :(
#include <regex.h>

char* patternStorage[MAX_IGNORE_PATTERNS]; 

void synchronizeFile(const char *sourcePath, const char *destinationPath, const struct stat *sourceStat, const struct stat *destStat, const struct Options *options);

void glob2regex(const char *globPattern, char *regex) {
    const char *ptr = globPattern;
    char *regexPtr = regex;
    while (*ptr) {
        if (*ptr == '*') {
            *regexPtr++ = '.';
            *regexPtr++ = '*';
        } else {
            *regexPtr++ = *ptr;
        }
        ptr++;
    }
    *regexPtr = '\0'; 
}

void freeOptions(struct Options *options) {
    for (int i = 0; i < options->numIgnorePatterns; ++i) {
        regfree(&options->ignoreRegexPatterns[i]);
    }
    free(options->ignoreRegexPatterns);

    for (int i = 0; i < options->numIncludePatterns; ++i) {
        regfree(&options->includeRegexPatterns[i]);
    }
    free(options->includeRegexPatterns);

    for (int i = 0; i < options->numSources; ++i) {
        free(options->sources[i]);
    }
    free(options->sources);

    for (int i = 0; i < options->numIgnorePatterns; ++i) {
        free(options->ignorePatterns[i]);
    }
    free(options->ignorePatterns);

    for (int i = 0; i < options->numIncludePatterns; ++i) {
        free(options->includePatterns[i]);
    }
    free(options->includePatterns);
}

struct Options parseOptions(int argc, char *argv[]) {
    char *ignorePatterns[MAX_IGNORE_PATTERNS];
    char* patternStorage[MAX_IGNORE_PATTERNS]; 

    struct Options options;
    memset(&options, 0, sizeof(options)); 
    options.sources = NULL;
    options.ignorePatterns = NULL;

    options.numSources = 0;

    int opt;

    // possibly unnecessary
    options.ignoreRegexPatterns = malloc(MAX_IGNORE_PATTERNS * sizeof(regex_t));
    options.includeRegexPatterns = malloc(MAX_IGNORE_PATTERNS * sizeof(regex_t));
    if (options.ignoreRegexPatterns == NULL || options.includeRegexPatterns == NULL) {
        fprintf(stderr, "Memory allocation error\n");
        freeOptions(&options);
        exit(EXIT_FAILURE);
    }

    options.ignorePatterns = malloc(MAX_IGNORE_PATTERNS * sizeof(char *));
    options.includePatterns = malloc(MAX_INCLUDE_PATTERNS * sizeof(char *));
    if (options.ignorePatterns == NULL || options.includePatterns == NULL) {
        fprintf(stderr, "Memory allocation error\n");
        freeOptions(&options);
        exit(EXIT_FAILURE);
    }

    while ((opt = getopt(argc, argv, "ranpvi:o:")) != -1) {
        switch (opt) {
            case 'r':
                options.recursive = true;
                break;
            case 'a':
                options.allFiles = true;
                break;
            case 'n':
                options.dryRun = true;
                options.verbose = true;
                break;
            case 'p':
                options.preserve = true;
                break;
            case 'v':
                options.verbose = true;
                break;
            case 'i':
                if (options.numIgnorePatterns < MAX_IGNORE_PATTERNS) {
                    options.ignorePatterns[options.numIgnorePatterns++] = strdup(optarg);
                } else {
                    fprintf(stderr, "Too many ignore patterns. Maximum allowed: %d\n", MAX_IGNORE_PATTERNS);
                    freeOptions(&options);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'o':
                if (options.numIncludePatterns < MAX_INCLUDE_PATTERNS) {
                    options.includePatterns[options.numIncludePatterns++] = strdup(optarg);
                } else {
                    fprintf(stderr, "Too many include patterns. Maximum allowed: %d\n", MAX_INCLUDE_PATTERNS);
                    freeOptions(&options);
                    exit(EXIT_FAILURE);
                }
                break;
            default:
                fprintf(stderr, "Usage: %s [-ranpv] <source_directory> <destination_directory>\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    options.sources = malloc(options.numSources * sizeof(char *));
    if (options.sources == NULL) {
        fprintf(stderr, "Memory allocation error\n");
        freeOptions(&options);
        exit(EXIT_FAILURE);
    }
    
    for (int i = 0; i < options.numSources; ++i) {
        options.sources[i] = NULL;
    }

    for (int i = optind; i < argc; ++i) {
        options.sources[i - optind] = strdup(argv[i]);
        if (options.sources[i - optind] == NULL) {
            fprintf(stderr, "Memory allocation error\n");
            freeOptions(&options);
            exit(EXIT_FAILURE);
        }
    }
    options.numSources = argc - optind;

    return options;
}

bool matchesIgnorePatterns(const char *filename, char *const *ignorePatterns, int numIgnorePatterns) {
    for (int i = 0; i < numIgnorePatterns; ++i) {
        if (fnmatch(ignorePatterns[i], filename, FNM_PATHNAME) == 0) {
            return true; 
        }
    }
    return false; 
}

bool matchesIncludePatterns(const char *filename, char *const *includePatterns, int numIncludePatterns) {
    if (numIncludePatterns == 0) {
        return true;
    }
    for (int i = 0; i < numIncludePatterns; ++i) {
        if (fnmatch(includePatterns[i], filename, FNM_PATHNAME) == 0) {
            return true;
        }
    }
    return false;
}


void synchronizeDirectories(const struct Options *options) {
    for (int i = 0; i < options->numSources; ++i) {
        const char *sourceDir = options->sources[i];

        DIR *sourceDirPtr = opendir(sourceDir);
        if (sourceDirPtr == NULL) {
            perror("Error opening source directory");
            continue; 
        }

        struct dirent *entry;
        while ((entry = readdir(sourceDirPtr)) != NULL) {
            if (entry->d_name[0] == '.') {
                // Skipping hidden files 
                if (!options->allFiles) {
                    continue;
                }
            }

            char sourceFilePath[PATH_MAX];
            char destFilePath[PATH_MAX];

            snprintf(sourceFilePath, PATH_MAX, "%s/%s", sourceDir, entry->d_name);

            if (!matchesIgnorePatterns(entry->d_name, options->ignorePatterns, options->numIgnorePatterns) && matchesIncludePatterns(entry->d_name, options->includePatterns, options->numIncludePatterns)){
                struct stat sourceStat;
                if (stat(sourceFilePath, &sourceStat) == 0) {
                    if (S_ISDIR(sourceStat.st_mode) && options->recursive) {
                        for (int j = 0; j < options->numSources; ++j) {
                            snprintf(destFilePath, PATH_MAX, "%s/%s", options->sources[j], entry->d_name);
                            recursiveSyncDirectories(sourceFilePath, destFilePath, options);
                        }
                    } else if (S_ISREG(sourceStat.st_mode)) {
                        for (int j = 0; j < options->numSources; ++j) {
                            snprintf(destFilePath, PATH_MAX, "%s/%s", options->sources[j], entry->d_name);
                            struct stat destStat;
                            if (stat(destFilePath, &destStat) == 0) {
                                synchronizeFile(sourceFilePath, destFilePath, &sourceStat, &destStat, options);
                            } else {
                                if (!options->dryRun) {
                                    if (copyFile(sourceFilePath, destFilePath)) {
                                        if (options->verbose) {
                                            printf("Synchronized: %s\n", sourceFilePath);
                                        }
                                    } else {
                                        fprintf(stderr, "Failed to synchronize: %s\n", sourceFilePath);
                                    }
                                } else if (options->verbose) {
                                    printf("Dry run - Synchronized (simulated): %s\n", sourceFilePath);
                                }
                            }
                        }
                    }
                }
            }
        }

        closedir(sourceDirPtr);
    }
}

int main(int argc, char *argv[]) {
    struct Options options = parseOptions(argc, argv);

    printf("Recursive: %d\n", options.recursive);
    printf("All Files: %d\n", options.allFiles);
    printf("Dry Run: %d\n", options.dryRun);
    printf("Preserve: %d\n", options.preserve);
    printf("Verbose: %d\n", options.verbose);
    printf("Number of Ignore Patterns: %d\n", options.numIgnorePatterns);
    for (int i = 0; i < options.numIgnorePatterns; ++i) {
        printf("Ignore Pattern %d: %s\n", i, options.ignorePatterns[i]);
    }
    printf("Number of Include Patterns: %d\n", options.numIncludePatterns);
    for (int i = 0; i < options.numIncludePatterns; ++i) {
        printf("Include Pattern %d: %s\n", i, options.includePatterns[i]);
    }
    
    synchronizeDirectories(&options);

    return 0;
}