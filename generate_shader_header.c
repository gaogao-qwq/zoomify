#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE_LENGTH 1024
#define MAX_FILENAME_LENGTH 256

// Helper function to convert string to uppercase
char *strupr(char *s) {
    char *p = s;
    while (*p) {
        *p = toupper(*p);
        p++;
    }
    return s;
}

void write_shader_to_header(FILE *header_file, char *filename) {
    FILE *shader_file = fopen(filename, "r");
    if (!shader_file) {
        perror("Failed to open shader file");
        return;
    }

    char *base_name = strrchr(filename, '/');
    if (base_name) {
        base_name++;  // escape '/'
    } else {
        base_name = filename;
    }
    char *dot = strrchr(base_name, '.');
    if (dot) {
        *dot = '\0';  // remove extension
    }

    fprintf(header_file, "#define %s_SHADER_SRC \\\n", strupr(base_name));

    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), shader_file)) {
        fputc('\"', header_file);
        for (char *p = line; *p; p++) {
            if (*p == '\\') {
                fputs("\\\\", header_file);  // escape `''
            } else if (*p == '\"') {
                fputs("\\\"", header_file);  // escape `"'
            } else if (*p == '\n') {
                fputs("\\n\" \\\n", header_file);  // handle `\n'
                break;
            } else {
                fputc(*p, header_file);
            }
        }
    }

    fclose(shader_file);
    fputs("\n", header_file);
}

int main() {
    const char *input_dir = "resources";
    const char *output_file = "include/shaders.h";

    FILE *header_file = fopen(output_file, "w");
    if (!header_file) {
        perror("Failed to create header file");
        return EXIT_FAILURE;
    }

    fprintf(header_file, "// Auto-generated header file for GLSL shaders, DO NOT EDIT!\n");
    fprintf(header_file, "#ifndef SHADERS_H\n");
    fprintf(header_file, "#define SHADERS_H\n\n");

    DIR *dir = opendir(input_dir);
    if (!dir) {
        perror("Failed to open resources directory");
        fclose(header_file);
        return EXIT_FAILURE;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG && strstr(entry->d_name, ".glsl")) {
            char filepath[MAX_FILENAME_LENGTH];
            snprintf(filepath, sizeof(filepath), "%s/%s", input_dir, entry->d_name);
            write_shader_to_header(header_file, filepath);
        }
    }

    closedir(dir);
    fprintf(header_file, "#endif // SHADERS_H\n");
    fclose(header_file);

    printf("Header file %s generated successfully.\n", output_file);
    return EXIT_SUCCESS;
}
