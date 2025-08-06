#ifndef FILE_UTILS_H
#define FILE_UTILS_H

int read_file(const char *filename, char **content);
const char *get_content_type(const char *path);

#endif