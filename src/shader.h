#pragma once

#include <cstdint>
#include "math/kraft_math.h"

struct Program
{
    uint32_t ProgramID;

    Program(const char *vs, const char *fs);
    uint32_t LoadShader(const char* filename, uint32_t type);
    
    void Use();
    void SetFloat(const char* name, float value);
    void SetInt(const char* name, int value);
    void SetMat4(const char* name, Mat4f value);
};