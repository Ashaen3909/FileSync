//  CITS2002 Project 2 2023
//  Student1:   23883137   Ashaen Bandara Damunupola
//  Student2:   23881368   Rabi Ul Hasan

#ifndef MYSYNC_H
#define MYSYNC_H

#include <regex.h>
#include <stdbool.h>
#include <time.h>
#include "fileutil.h"

#define MAX_IGNORE_PATTERNS 99
#define MAX_INCLUDE_PATTERNS 99

struct FileInfo {
    char *name;            
    time_t modificationTime; 
};

struct Options {
    bool recursive;                     // -r 
    bool allFiles;                      // -a 
    bool dryRun;                        // -n 
    bool preserve;                      // -p 
    bool verbose;                       // -v 
    const char *dest;                   
    char **sources;                     
    int numSources;                     
    int numIgnorePatterns;              
    regex_t *ignoreRegexPatterns;       
    regex_t *includeRegexPatterns;      
    int numIncludePatterns;             
    char **ignorePatterns;
    char **includePatterns;
};

struct Options parseOptions(int argc, char *argv[]);

// Implementing options
void synchronizeDirectories(const struct Options *options);

bool matchesIgnorePatterns(const char *filename, char *const *ignorePatterns, int numIgnorePatterns);

bool matchesIncludePatterns(const char *filename, char *const *includePatterns, int numIncludePatterns);

// Free memory allocated for ignore patterns
void freeOptions(struct Options *options);

#endif 