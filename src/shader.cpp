#include "shader.h"
#include "util.h"

#include <glad/gl.h>

Program::Program(const char *vs, const char *fs)
{
    ProgramID = glCreateProgram();

    int success;
    uint32_t vertexShaderID, fragmentShaderID;
    vertexShaderID = this->LoadShader(vs, GL_VERTEX_SHADER);
    fragmentShaderID = this->LoadShader(fs, GL_FRAGMENT_SHADER);

    glAttachShader(ProgramID, vertexShaderID);
    glAttachShader(ProgramID, fragmentShaderID);
    glLinkProgram(ProgramID);
    glGetProgramiv(ProgramID, GL_LINK_STATUS, &success);
    if (!success)
    {
        char errorLog[512];
        glGetProgramInfoLog(ProgramID, 512, NULL, errorLog);
        printf("Shader linking failed with error %s", errorLog);

        return;
    }

    glDeleteShader(vertexShaderID);
    glDeleteShader(fragmentShaderID);
}

uint32_t Program::LoadShader(const char *filename, uint32_t type)
{
    char *shader = read_file(filename, NULL);

    uint32_t shaderID = glCreateShader(type);
    glShaderSource(shaderID, 1, &shader, NULL);
    glCompileShader(shaderID);

    int success;
    glGetShaderiv(shaderID, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char errorLog[512];
        glGetShaderInfoLog(shaderID, 512, NULL, errorLog);

        printf("Shader compilation failed with error %s", errorLog);
    }

    free(shader);
    return shaderID;
}

void Program::Use() 
{
    glUseProgram(ProgramID);
}

void Program::SetFloat(const char* name, float value)
{
    glUniform1f(glGetUniformLocation(ProgramID, name), value);
}

void Program::SetInt(const char* name, int value)
{
    glUniform1i(glGetUniformLocation(ProgramID, name), value);
}