#pragma once

#include <cstdint>

struct Program
{
    uint32_t ProgramID;

    Program(const char *vs, const char *fs);
    uint32_t LoadShader(const char* filename, uint32_t type);
    
    void Use();
    void SetFloat(const char* name, float value);
    void SetInt(const char* name, int value);
};