#include <cstdio>

#define GLAD_GL_IMPLEMENTATION
#include <glad/gl.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <GLFW/glfw3.h>

#include "util.h"
#include "shader.h"
#include "texture.h"

#ifdef WIN32
#include "Windows.h"
#endif

#include "math.h"

internal void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    glViewport(0, 0, width, height);
}

internal void process_input(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

internal void read_token(char **src, char *token)
{
    // Skip any initial spaces
    while (**src == ' ' || **src == '\n' || **src == '\t')
        (*src)++;

    // Read the token
    while (**src != ' ' && **src != '\n' && **src != '\t')
    {
        *token = **src;
        (*src)++;
        token++;
    }

    *token = 0;
}

internal void render()
{
    float2 vec1, vec2, vec3;
    vec1.x = 12;
    vec1.y = 10;
    vec2.x = 5;
    vec2.y = 6;

    vec1 += vec2;
    
    float3 color3(1.0f, 0.0f, 1.0f);
    float4 color;
    color.rgb = color3;

    Mat4 mat1, mat2, mat3;
    mat1 = mat2 * mat3;

    // printf("%f, %f\n", color.r, color.g);
}

int main()
{
    if (!glfwInit())
    {
        printf("Failed to initialize glfw");
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

#if __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    const int width = 1024;
    const int height = 768;
    GLFWwindow *window = glfwCreateWindow(width, height, "Bored", nullptr, nullptr);
    if (!window)
    {
        printf("Failed to create window");

        glfwTerminate();
        return -1;
    }

// Center the window
#if WIN32
    int maxWidth = GetSystemMetrics(SM_CXSCREEN);
    int maxHeight = GetSystemMetrics(SM_CYSCREEN);
    glfwSetWindowPos(window, (maxWidth / 2) - (width / 2), (maxHeight / 2) - (height / 2));
#endif

    glfwShowWindow(window);
    glfwMakeContextCurrent(window);
    // if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress))
    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress))
    {
        printf("Failed to initialize opengl context");
        return -1;
    }

    // glfwSwapInterval(1);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // Set the viewport once
    // framebuffer_size_callback(window, width, height);

    // Create the vertex data
    uint32_t vao;
    uint32_t indicesCount;
    {
        float vertices[] = {
            -0.5f, -0.5f, 0.f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f,
            -0.5f, +0.5f, 0.f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
            +0.5f, +0.5f, 0.f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f,
            +0.5f, -0.5f, 0.f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f};

        uint32_t indices[] = {
            0, 1, 2,
            2, 3, 0};

        indicesCount = sizeof(indices) / sizeof(indices[0]);

        // Generate VAO
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        // Generate VBO
        uint32_t vbo;
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void *)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void *)(7 * sizeof(float)));
        glEnableVertexAttribArray(2);

        // Generate EBO
        uint32_t ebo;
        glGenBuffers(1, &ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    Program *program = new Program("res/shaders/vertex.vert", "res/shaders/fragment.frag");
    Texture *texture = new Texture("res/textures/wall.jpg");
    Texture *raven = new Texture("res/textures/ravenicon.jpg");

    while (!glfwWindowShouldClose(window))
    {
        auto t = TranslationMatrix(float3(0.5f, 0.5f, 0.0f));
        PrintMatrix(t);
        
        process_input(window);

        glClearColor(0.2f, 0.1f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        render();
        // glUseProgram(shaderProgramID);
        // program->SetFloat("offset", 0.05);
        program->Use();
        raven->Bind(1);
        program->SetInt("textureData", 1);

        texture->Bind(0);
        program->SetInt("texture2Data", 0);
        program->SetMat4("transform", t);

        glBindVertexArray(vao);
        // glDrawArrays(GL_TRIANGLES, 0, 3);
        glDrawElements(GL_TRIANGLES, indicesCount, GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}