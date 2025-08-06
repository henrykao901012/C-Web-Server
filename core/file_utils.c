#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "file_utils.h"
#include "logger.h"

int read_file(const char *filename, char **content)
{
    FILE *file = fopen(filename, "rb");
    if (!file)
    {
        return -1;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    *content = malloc(file_size + 1);
    if (!*content)
    {
        fclose(file);
        return -1;
    }

    size_t read_size = fread(*content, 1, file_size, file);
    fclose(file);

    if (read_size != file_size)
    {
        free(*content);
        return -1;
    }

    (*content)[file_size] = '\0';
    return file_size;
}

const char *get_content_type(const char *path)
{
    const char *ext = strrchr(path, '.');
    if (!ext)
    {
        return "application/octet-stream";
    }

    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0)
    {
        return "text/html; charset=utf-8";
    }
    else if (strcmp(ext, ".css") == 0)
    {
        return "text/css";
    }
    else if (strcmp(ext, ".js") == 0)
    {
        return "application/javascript";
    }
    else if (strcmp(ext, ".json") == 0)
    {
        return "application/json";
    }
    else if (strcmp(ext, ".png") == 0)
    {
        return "image/png";
    }
    else if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)
    {
        return "image/jpeg";
    }
    else if (strcmp(ext, ".gif") == 0)
    {
        return "image/gif";
    }
    else if (strcmp(ext, ".txt") == 0)
    {
        return "text/plain; charset=utf-8";
    }

    return "application/octet-stream";
}