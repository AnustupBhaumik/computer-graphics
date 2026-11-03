#include <iostream>
#include <vector>
#include <string>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <stdexcept>
#include <algorithm>
#include <glad/gl.h>
#define USING_GLAD2 1


#include <GLFW/glfw3.h>
#include <FreeImage.h>

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

const int INITIAL_WINDOW_WIDTH = 1280;
const int INITIAL_WINDOW_HEIGHT = 720;
const char* WINDOW_TITLE = "OpenGL 4.6 - Background + GLTF Sphere";

const std::string BACKGROUND_PATH = "assets/background.jpg";
const std::string MODEL_PATH = "assets/sphere.glb";

const float SPHERE_SCALE = 1.0f;
const float SPHERE_POSITION_X = 0.0f;
const float SPHERE_POSITION_Y = 0.0f;
const float SPHERE_POSITION_Z = 0.0f;

const float SPHERE_COLOR_R = 0.20f;
const float SPHERE_COLOR_G = 0.60f;
const float SPHERE_COLOR_B = 0.95f;

int gWindowWidth = INITIAL_WINDOW_WIDTH;
int gWindowHeight = INITIAL_WINDOW_HEIGHT;

struct Vec3 {
    float x, y, z;
};

Vec3 operator+(const Vec3& a, const Vec3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 operator-(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 operator*(const Vec3& a, float value)   { return {a.x * value, a.y * value, a.z * value}; }

float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 cross(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float length(const Vec3& value) {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

Vec3 normalize(const Vec3& value) {
    float valLen = length(value);
    if (valLen <= 0.000001f) return {0.0f, 0.0f, 0.0f};
    return value * (1.0f / valLen);
}

struct Mat4 {
    float m[16];
};

Mat4 makeIdentityMatrix() {
    Mat4 result{};
    result.m[0] = 1.0f;
    result.m[5] = 1.0f;
    result.m[10] = 1.0f;
    result.m[15] = 1.0f;
    return result;
}

Mat4 makeTranslationMatrix(float x, float y, float z) {
    Mat4 result = makeIdentityMatrix();
    result.m[12] = x;
    result.m[13] = y;
    result.m[14] = z;
    return result;
}

Mat4 makeScaleMatrix(float x, float y, float z) {
    Mat4 result{};
    result.m[0] = x;
    result.m[5] = y;
    result.m[10] = z;
    result.m[15] = 1.0f;
    return result;
}

Mat4 multiplyMatrices(const Mat4& a, const Mat4& b) {
    Mat4 result{};
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float val = 0.0f;
            for (int k = 0; k < 4; ++k) {
                val += a.m[k * 4 + row] * b.m[col * 4 + k];
            }
            result.m[col * 4 + row] = val;
        }
    }
    return result;
}

float radians(float degrees) {
    return degrees * 3.14159265358979323846f / 180.0f;
}

Mat4 makePerspectiveMatrix(float fovDegrees, float aspectRatio, float nearPlane, float farPlane) {
    Mat4 result{};
    float tangent = std::tan(radians(fovDegrees) / 2.0f);
    float height = nearPlane * tangent;
    float width = height * aspectRatio;

    result.m[0]  = nearPlane / width;
    result.m[5]  = nearPlane / height;
    result.m[10] = -(farPlane + nearPlane) / (farPlane - nearPlane);
    result.m[11] = -1.0f;
    result.m[14] = -(2.0f * farPlane * nearPlane) / (farPlane - nearPlane);
    return result;
}

Mat4 makeLookAtMatrix(const Vec3& eye, const Vec3& target, const Vec3& up) {
    Vec3 forward = normalize(target - eye);
    Vec3 right = normalize(cross(forward, up));
    Vec3 cameraUp = cross(right, forward);

    Mat4 result = makeIdentityMatrix();
    result.m[0]  = right.x;
    result.m[1]  = cameraUp.x;
    result.m[2]  = -forward.x;
    result.m[4]  = right.y;
    result.m[5]  = cameraUp.y;
    result.m[6]  = -forward.y;
    result.m[8]  = right.z;
    result.m[9]  = cameraUp.z;
    result.m[10] = -forward.z;
    result.m[12] = -dot(right, eye);
    result.m[13] = -dot(cameraUp, eye);
    result.m[14] = dot(forward, eye);
    return result;
}

GLuint compileShader(GLenum shaderType, const char* source) {
    GLuint shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compileStatus = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compileStatus);
    if (compileStatus != GL_TRUE) {
        GLint logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> log(static_cast<std::size_t>(logLength) + 1);
        glGetShaderInfoLog(shader, logLength, nullptr, log.data());
        std::string message(log.data());
        glDeleteShader(shader);
        throw std::runtime_error("Shader compilation failed:\n" + message);
    }
    return shader;
}

GLuint createProgram(const char* vertexShaderSource, const char* fragmentShaderSource) {
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    GLuint program = glCreateProgram();

    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint linkStatus = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    if (linkStatus != GL_TRUE) {
        GLint logLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> log(static_cast<std::size_t>(logLength) + 1);
        glGetProgramInfoLog(program, logLength, nullptr, log.data());
        std::string message(log.data());
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        glDeleteProgram(program);
        throw std::runtime_error("Shader linking failed:\n" + message);
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return program;
}

const char* BACKGROUND_VERTEX_SHADER = R"(
#version 460 core
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aTexCoord;
out vec2 vTexCoord;
void main() {
    gl_Position = vec4(aPosition, 0.0, 1.0);
    vTexCoord = aTexCoord;
}
)";

const char* BACKGROUND_FRAGMENT_SHADER = R"(
#version 460 core
in vec2 vTexCoord;
out vec4 FragColor;
uniform sampler2D uBackground;
void main() {
    FragColor = texture(uBackground, vTexCoord);
}
)";

const char* MODEL_VERTEX_SHADER = R"(
#version 460 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
out vec3 vWorldPosition;
out vec3 vWorldNormal;
void main() {
    vec4 worldPosition = uModel * vec4(aPosition, 1.0);
    vWorldPosition = worldPosition.xyz;
    mat3 normalMatrix = transpose(inverse(mat3(uModel)));
    vWorldNormal = normalize(normalMatrix * aNormal);
    gl_Position = uProjection * uView * worldPosition;
}
)";

const char* MODEL_FRAGMENT_SHADER = R"(
#version 460 core
in vec3 vWorldPosition;
in vec3 vWorldNormal;
out vec4 FragColor;
uniform vec3 uObjectColor;
uniform vec3 uLightPosition;
uniform vec3 uViewPosition;
void main() {
    vec3 normal = normalize(vWorldNormal);
    vec3 lightDirection = normalize(uLightPosition - vWorldPosition);
    float diffuse = max(dot(normal, lightDirection), 0.0);
    vec3 viewDirection = normalize(uViewPosition - vWorldPosition);
    vec3 reflectedLight = reflect(-lightDirection, normal);
    float specular = pow(max(dot(viewDirection, reflectedLight), 0.0), 64.0);
    float ambient = 0.18;
    vec3 lighting = vec3(ambient + 0.75 * diffuse + 0.30 * specular);
    FragColor = vec4(uObjectColor * lighting, 1.0);
}
)";

struct BackgroundTexture {
    GLuint texture;
    int width;
    int height;
};

BackgroundTexture loadBackgroundImage(const std::string& filePath) {
    BackgroundTexture result{};
    FREE_IMAGE_FORMAT format = FreeImage_GetFileType(filePath.c_str(), 0);
    if (format == FIF_UNKNOWN) {
        format = FreeImage_GetFIFFromFilename(filePath.c_str());
    }
    if (format == FIF_UNKNOWN || !FreeImage_FIFSupportsReading(format)) {
        throw std::runtime_error("FreeImage cannot read format: " + filePath);
    }

    FIBITMAP* originalBitmap = FreeImage_Load(format, filePath.c_str());
    if (!originalBitmap) {
        throw std::runtime_error("Could not load background image: " + filePath);
    }

    FIBITMAP* bitmap = FreeImage_ConvertTo32Bits(originalBitmap);
    FreeImage_Unload(originalBitmap);
    if (!bitmap) {
        throw std::runtime_error("Could not convert background image to 32-bit.");
    }

    if (!FreeImage_FlipVertical(bitmap)) {
        FreeImage_Unload(bitmap);
        throw std::runtime_error("Could not vertically flip background image.");
    }

    result.width = static_cast<int>(FreeImage_GetWidth(bitmap));
    result.height = static_cast<int>(FreeImage_GetHeight(bitmap));
    BYTE* pixels = FreeImage_GetBits(bitmap);
    if (!pixels) {
        FreeImage_Unload(bitmap);
        throw std::runtime_error("Could not access image pixels.");
    }

    glGenTextures(1, &result.texture);
    glBindTexture(GL_TEXTURE_2D, result.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, result.width, result.height, 0, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);

    FreeImage_Unload(bitmap);
    glBindTexture(GL_TEXTURE_2D, 0);
    return result;
}

class BackgroundRenderer {
public:
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint shaderProgram = 0;
    GLint backgroundTextureLocation = -1;
    GLint uvScaleLocation = -1;
    GLint uvOffsetLocation = -1;

    void initialize() {
        const float vertices[] = {
            -1.0f, -1.0f, 0.0f, 0.0f,
             1.0f, -1.0f, 1.0f, 0.0f,
             1.0f,  1.0f, 1.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f,
             1.0f,  1.0f, 1.0f, 1.0f,
            -1.0f,  1.0f, 0.0f, 1.0f
        };

        shaderProgram = createProgram(BACKGROUND_VERTEX_SHADER, BACKGROUND_FRAGMENT_SHADER);
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        backgroundTextureLocation = glGetUniformLocation(shaderProgram, "uBackground");
        uvScaleLocation = glGetUniformLocation(shaderProgram, "uUVScale");
        uvOffsetLocation = glGetUniformLocation(shaderProgram, "uUVOffset");
    }

    void render(const BackgroundTexture& background) {
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        glUseProgram(shaderProgram);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, background.texture);
        glUniform1i(backgroundTextureLocation, 0);
        glUniform2f(uvScaleLocation, 1.0f, 1.0f);
        glUniform2f(uvOffsetLocation, 0.0f, 0.0f);

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);

        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
    }

    void destroy() {
        if (vbo != 0) { glDeleteBuffers(1, &vbo); vbo = 0; }
        if (vao != 0) { glDeleteVertexArrays(1, &vao); vao = 0; }
        if (shaderProgram != 0) { glDeleteProgram(shaderProgram); shaderProgram = 0; }
    }
};

struct Vertex {
    float position[3];
    float normal[3];
};

struct GltfMeshPart {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLsizei indexCount = 0;
    GLsizei vertexCount = 0;
    Mat4 nodeTransform = makeIdentityMatrix();
};

class GltfModel {
public:
    std::vector<GltfMeshPart> parts;

    void load(const std::string& filePath) {
        cgltf_options options{};
        cgltf_data* data = nullptr;

        cgltf_result parseResult = cgltf_parse_file(&options, filePath.c_str(), &data);
        if (parseResult != cgltf_result_success) {
            throw std::runtime_error("cgltf could not parse the model: " + filePath);
        }

        cgltf_result validationResult = cgltf_validate(data);
        if (validationResult != cgltf_result_success) {
            cgltf_free(data);
            throw std::runtime_error("The GLTF/GLB file failed validation: " + filePath);
        }

        cgltf_result bufferResult = cgltf_load_buffers(&options, data, filePath.c_str());
        if (bufferResult != cgltf_result_success) {
            cgltf_free(data);
            throw std::runtime_error("cgltf could not load the model buffers: " + filePath);
        }

        std::cout << "[DEBUG] Parsing " << filePath << ": Found "
                  << data->meshes_count << " meshes and " 
                  << data->nodes_count << " nodes." << std::endl;

        // Traverse all nodes across all scenes
        if (data->scenes_count > 0) {
            for (cgltf_size s = 0; s < data->scenes_count; ++s) {
                for (cgltf_size i = 0; i < data->scenes[s].nodes_count; ++i) {
                    processNode(data->scenes[s].nodes[i]);
                }
            }
        } else {
            for (cgltf_size i = 0; i < data->nodes_count; ++i) {
                if (data->nodes[i].parent == nullptr) {
                    processNode(&data->nodes[i]);
                }
            }
        }

        // Direct mesh scan fallback: bypasses unlinked node graphs entirely
        if (parts.empty() && data->meshes_count > 0) {
            for (cgltf_size m = 0; m < data->meshes_count; ++m) {
                processMesh(nullptr, &data->meshes[m]);
            }
        }

        cgltf_free(data);

        if (parts.empty()) {
            throw std::runtime_error("No renderable triangle meshes were found in the model.");
        }
    }

    void destroy() {
        for (auto& part : parts) {
            if (part.ebo != 0) glDeleteBuffers(1, &part.ebo);
            if (part.vbo != 0) glDeleteBuffers(1, &part.vbo);
            if (part.vao != 0) glDeleteVertexArrays(1, &part.vao);
        }
        parts.clear();
    }

private:
    void processNode(const cgltf_node* node) {
        if (!node) return;

        if (node->mesh != nullptr) {
            processMesh(node, node->mesh);
        }
        for (cgltf_size i = 0; i < node->children_count; ++i) {
            processNode(node->children[i]);
        }
    }

    void processMesh(const cgltf_node* node, const cgltf_mesh* mesh) {
        if (!mesh) return;

        for (cgltf_size i = 0; i < mesh->primitives_count; ++i) {
            const cgltf_primitive* primitive = &mesh->primitives[i];

            // In glTF 2.0, triangles == 4. If unset, 0/default evaluates to triangles.
            if (primitive->type != cgltf_primitive_type_triangles && primitive->type != 0) {
                continue;
            }

            createMeshPart(node, primitive);
        }
    }

    void createMeshPart(const cgltf_node* node, const cgltf_primitive* primitive) {
        const cgltf_accessor* positionAccessor = nullptr;
        const cgltf_accessor* normalAccessor = nullptr;

        for (cgltf_size i = 0; i < primitive->attributes_count; ++i) {
            if (primitive->attributes[i].type == cgltf_attribute_type_position) {
                positionAccessor = primitive->attributes[i].data;
            } else if (primitive->attributes[i].type == cgltf_attribute_type_normal) {
                normalAccessor = primitive->attributes[i].data;
            }
        }

        if (!positionAccessor || positionAccessor->count == 0) {
            return;
        }

        const cgltf_size vertexCount = positionAccessor->count;
        std::vector<Vertex> vertices(static_cast<std::size_t>(vertexCount));

        for (cgltf_size i = 0; i < vertexCount; ++i) {
            float pos[3] = {0.0f, 0.0f, 0.0f};
            cgltf_accessor_read_float(positionAccessor, i, pos, 3);
            vertices[i].position[0] = pos[0];
            vertices[i].position[1] = pos[1];
            vertices[i].position[2] = pos[2];

            if (normalAccessor != nullptr) {
                float norm[3] = {0.0f, 0.0f, 0.0f};
                cgltf_accessor_read_float(normalAccessor, i, norm, 3);
                vertices[i].normal[0] = norm[0];
                vertices[i].normal[1] = norm[1];
                vertices[i].normal[2] = norm[2];
            } else {
                vertices[i].normal[0] = 0.0f;
                vertices[i].normal[1] = 0.0f;
                vertices[i].normal[2] = 1.0f;
            }
        }

        std::vector<GLuint> indices;
        if (primitive->indices != nullptr) {
            indices.resize(primitive->indices->count);
            for (cgltf_size i = 0; i < primitive->indices->count; ++i) {
                indices[i] = static_cast<GLuint>(cgltf_accessor_read_index(primitive->indices, i));
            }
        }

        GltfMeshPart part;
        part.vertexCount = static_cast<GLsizei>(vertices.size());
        part.indexCount = static_cast<GLsizei>(indices.size());

        glGenVertexArrays(1, &part.vao);
        glGenBuffers(1, &part.vbo);

        glBindVertexArray(part.vao);
        glBindBuffer(GL_ARRAY_BUFFER, part.vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

        if (!indices.empty()) {
            glGenBuffers(1, &part.ebo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, part.ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);
        }

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        parts.push_back(part);
    }
};

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT, WINDOW_TITLE, nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);


    if (!gladLoadGL(glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    // 1. Declare and load the model
    GltfModel model;
    try {
        model.load(MODEL_PATH);
    } catch (const std::exception& e) {
        std::cerr << "GLTF load error: " << e.what() << "\n";
        return -1;
    }

    // 2. Declare and compile the shader program
    GLuint modelProgram = createProgram(MODEL_VERTEX_SHADER, MODEL_FRAGMENT_SHADER);

    std::cout << "Model parts count: " << model.parts.size() << "\n";
    for (size_t i = 0; i < model.parts.size(); ++i) {
        std::cout << "Part " << i 
                  << " | VAO: " << model.parts[i].vao
                  << " | Vertices: " << model.parts[i].vertexCount
                  << " | Indices: " << model.parts[i].indexCount << "\n";
    }

    // 3. Render loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(modelProgram);

        Mat4 projection = makePerspectiveMatrix(45.0f, static_cast<float>(gWindowWidth) / static_cast<float>(gWindowHeight), 0.1f, 100.0f);
        Mat4 view = makeLookAtMatrix({0.0f, 0.0f, 5.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
        Mat4 modelMat = multiplyMatrices(
            makeTranslationMatrix(SPHERE_POSITION_X, SPHERE_POSITION_Y, SPHERE_POSITION_Z),
            makeScaleMatrix(SPHERE_SCALE, SPHERE_SCALE, SPHERE_SCALE)
        );

        glUniformMatrix4fv(glGetUniformLocation(modelProgram, "uProjection"), 1, GL_FALSE, projection.m);
        glUniformMatrix4fv(glGetUniformLocation(modelProgram, "uView"), 1, GL_FALSE, view.m);
        glUniformMatrix4fv(glGetUniformLocation(modelProgram, "uModel"), 1, GL_FALSE, modelMat.m);

        glUniform3f(glGetUniformLocation(modelProgram, "uObjectColor"), SPHERE_COLOR_R, SPHERE_COLOR_G, SPHERE_COLOR_B);
        glUniform3f(glGetUniformLocation(modelProgram, "uLightPosition"), 2.0f, 4.0f, 5.0f);
        glUniform3f(glGetUniformLocation(modelProgram, "uViewPosition"), 0.0f, 0.0f, 5.0f);

        for (const auto& part : model.parts) {
            glBindVertexArray(part.vao);
            if (part.indexCount > 0) {
                glDrawElements(GL_TRIANGLES, part.indexCount, GL_UNSIGNED_INT, 0);
            } else if (part.vertexCount > 0) {
                glDrawArrays(GL_TRIANGLES, 0, part.vertexCount);
            }
            glBindVertexArray(0);
        }

        glfwSwapBuffers(window);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}