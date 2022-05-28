#pragma once

#include <stdlib.h>
#include <stdio.h>

#define internal static

internal char *read_file(const char *path, size_t *filesize)
{
    FILE *f = fopen(path, "rb");
    size_t length;

    if (!f)
        return 0;

    fseek(f, 0, SEEK_END);
    length = ftell(f);

    if (filesize)
        *filesize = length;

    char *data = (char *)malloc(length + 1);
    if (data == 0)
        return 0;

    fseek(f, 0, SEEK_SET);
    fread(data, sizeof(char), length, f);
    fclose(f);

    data[length] = 0;
    return data;
}