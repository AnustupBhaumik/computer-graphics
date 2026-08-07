#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>

const char* vertex_shader_src = R"(
    #version 330 core
    layout (location = 0) in vec2 vPos;
    uniform float u_time;
    uniform float u_id; // 0.0, 1.0, or 2.0
    out vec3 vColor;

    void main() {
        // diff speeds for diff squares 
        float speed = 1.0 + u_id * 0.8;
        float angle = u_time * speed + u_id * 2.0;

        // rotation
        float c = cos(angle);
        float s = sin(angle);
        mat2 rot = mat2(c, s, -s, c);

        // trans
        vec2 offset = vec2(
            sin(u_time * (1.1 + u_id * 0.4) + u_id),
            cos(u_time * (1.5 - u_id * 0.3) + u_id)
        ) * 0.8;

        // colours
        if (u_id == 0.0)      vColor = vec3(0.9, 0.3, 0.3); 
        else if (u_id == 1.0) vColor = vec3(0.3, 0.9, 0.4); 
        else                  vColor = vec3(0.3, 0.5, 0.9); 

        // Apply scale (0.15 size), rotation, and translation
        gl_Position = vec4((rot * (vPos * 0.75)) + offset, 0.0, 1.0);
    }
)";

const char* fragment_shader_src = R"(
    #version 330 core
    in vec3 vColor;
    out vec4 FragColor;
    void main() { 
        FragColor = vec4(vColor, 1.0); 
    }
)";

int main() {
    try {
        if (!glfwInit()) throw std::runtime_error("Failed to initialize GLFW.");

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        GLFWwindow* win = glfwCreateWindow(800, 600, "3 Bouncing Squares", nullptr, nullptr);
        if (!win) throw std::runtime_error("Failed to create window.");
        
        glfwMakeContextCurrent(win);
        glewExperimental = GL_TRUE;
        if (glewInit() != GLEW_OK) throw std::runtime_error("Failed to initialize GLEW.");

        // Compile Shaders
        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &vertex_shader_src, nullptr);
        glCompileShader(vs);

        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &fragment_shader_src, nullptr);
        glCompileShader(fs);

        GLuint shader_program = glCreateProgram();
        glAttachShader(shader_program, vs);
        glAttachShader(shader_program, fs);
        glLinkProgram(shader_program);

        
        float vertices[] = {
            -0.2f, -0.2f,   0.2f, -0.2f,   0.2f,  0.2f,
             0.2f,  0.2f,  -0.2f,  0.2f,  -0.2f, -0.2f
        };

        GLuint vao, vbo;
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        GLint time_loc = glGetUniformLocation(shader_program, "u_time");
        GLint id_loc   = glGetUniformLocation(shader_program, "u_id");

        while (!glfwWindowShouldClose(win)) {
            glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            glUseProgram(shader_program);
            glUniform1f(time_loc, (float)glfwGetTime());
            glBindVertexArray(vao);

            
            for (int i = 0; i < 3; ++i) {
                glUniform1f(id_loc, (float)i); 
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }

            glfwSwapBuffers(win);
            glfwPollEvents();
        }

        glfwTerminate();
    } 
    catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
    return 0;
}
