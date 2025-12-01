#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "params.h"

float load_param(const char *filename, const char *key) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        // If file not found, return a safe default 
        // Or print error. Let's print error and return default 1.0
        perror("Error opening params file");
        return 1.0f; 
    }

    char line[128];
    char read_key[64];
    float read_val;
    float result = -1.0f; // Default error value
    int found = 0;

    while (fgets(line, sizeof(line), f)) {
        // Parse "KEY VALUE"
        if (sscanf(line, "%s %f", read_key, &read_val) == 2) {
            if (strcmp(read_key, key) == 0) {
                result = read_val;
                found = 1;
                break;
            }
        }
    }

    fclose(f);
    
    if (!found) {
        printf("[Params] Warning: Key '%s' not found in %s. Using default 1.0\n", key, filename);
        return 1.0f;
    }
    
    return result;
}