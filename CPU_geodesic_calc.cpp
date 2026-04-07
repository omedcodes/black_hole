#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <iostream>
#define _USE_MATH_DEFINES
#include <cmath>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <chrono>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
using namespace glm;
using namespace std;
using Clock = std::chrono::high_resolution_clock;

double lastPrintTime = 0.0;
int    framesCount   = 0;
double c = 299792458.0;
double G = 6.67430e-11;
bool useGeodesics = false;

struct Camera {
    vec3 pos;
    vec3 target;
    float fovY;
    float azimuth, elevation, radius;
    float minRadius = 1e12f, maxRadius = 1e20f;
    bool dragging = false;
    bool panning = false;
    double lastX = 0, lastY = 0;

    // Adjustable speeds
    float orbitSpeed = 0.008f;
    float panSpeed = 0.001f;
    float zoomSpeed = 1.08f; // closer to 1 = slower zoom

    Camera() : azimuth(0), elevation(M_PI / 2.0f), radius(6.34194e10), fovY(60.0f) {
        target = vec3(0, 0, 0);
        updateVectors();
    }

    void updateVectors() {
        pos.x = target.x + radius * sin(elevation) * cos(azimuth);
        pos.y = target.y + radius * cos(elevation);
        pos.z = target.z + radius * sin(elevation) * sin(azimuth);
    }
    void processMouse(GLFWwindow* window, double xpos, double ypos) {
        float dx = float(xpos - lastX), dy = float(ypos - lastY);
        if (dragging && !panning) {
            // Orbit
            azimuth   -= dx * orbitSpeed;
            elevation -= dy * orbitSpeed;
            elevation = glm::clamp(elevation, 0.01f, float(M_PI)-0.01f);
        } else if (panning) {
            // Pan (move target in camera plane)
            vec3 forward = normalize(target - pos);
            vec3 right = normalize(cross(forward, vec3(0,1,0)));
            vec3 up = cross(right, forward);
            target += -right * dx * panSpeed * radius + up * dy * panSpeed * radius;
        }
        updateVectors();
        lastX = xpos; lastY = ypos;
    }
    void processScroll(double yoffset) {
        // Zoom (dolly in/out)
        if (yoffset < 0)
            radius *= pow(zoomSpeed, -yoffset);
        else
            radius /= pow(zoomSpeed, yoffset);
        radius = glm::clamp(radius, minRadius, maxRadius);
        updateVectors();
    }
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
        Camera* cam = (Camera*)glfwGetWindowUserPointer(window);
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            if (action == GLFW_PRESS) {
                cam->dragging = true;
                cam->panning = (mods & GLFW_MOD_SHIFT);
                double x, y; glfwGetCursorPos(window, &x, &y);
                cam->lastX = x; cam->lastY = y;
            } else if (action == GLFW_RELEASE) {
                cam->dragging = false;
                cam->panning = false;
            }
        }
    }
    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
        Camera* cam = (Camera*)glfwGetWindowUserPointer(window);
        cam->processMouse(window, xpos, ypos);
    }
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
        Camera* cam = (Camera*)glfwGetWindowUserPointer(window);
        cam->processScroll(yoffset);
    }
};
Camera camera;

struct Ray;
void rk4Step(Ray& ray, double dλ, double rs);

struct Engine {
    GLFWwindow* window;
    GLuint quadVAO;
    GLuint texture;
    GLuint shaderProgram;
    int WIDTH = 800;
    int HEIGHT = 600;
    float width = 100000000000.0f; // Width of the viewport in meters
    float height = 75000000000.0f; // Height of the viewport in meters
    
    Engine() {
        if (!glfwInit()) {
            cerr << "GLFW init failed\n";
            exit(EXIT_FAILURE);
        }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        window = glfwCreateWindow(WIDTH, HEIGHT, "Black Hole", nullptr, nullptr);
        if (!window) {
            cerr << "Failed to create GLFW window\n";
            glfwTerminate();
            exit(EXIT_FAILURE);
        }
        glfwMakeContextCurrent(window);
        glewExperimental = GL_TRUE;
        GLenum glewErr = glewInit();
        if (glewErr != GLEW_OK) {
            cerr << "Failed to initialize GLEW: "
                << (const char*)glewGetErrorString(glewErr)
                << "\n";
            glfwTerminate();
            exit(EXIT_FAILURE);
        }
        cout << "OpenGL " << glGetString(GL_VERSION) << "\n";
        this->shaderProgram = CreateShaderProgram();

        auto result = QuadVAO();
        this->quadVAO = result[0];
        this->texture = result[1];
    }
    GLuint CreateShaderProgram(){
        const char* vertexShaderSource = R"(
        #version 330 core
        layout (location = 0) in vec2 aPos;  // Changed to vec2
        layout (location = 1) in vec2 aTexCoord;
        out vec2 TexCoord;
        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);  // Explicit z=0
            TexCoord = aTexCoord;
        })";

        const char* fragmentShaderSource = R"(
        #version 330 core
        in vec2 TexCoord;
        out vec4 FragColor;
        uniform sampler2D screenTexture;
        void main() {
            FragColor = texture(screenTexture, TexCoord);
        })";

        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
        glCompileShader(vertexShader);

        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
        glCompileShader(fragmentShader);

        GLuint shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        return shaderProgram;
    };
    vector<GLuint> QuadVAO(){
        float quadVertices[] = {
            -1.0f,  1.0f,  0.0f, 1.0f,  // top left
            -1.0f, -1.0f,  0.0f, 0.0f,  // bottom left
            1.0f, -1.0f,  1.0f, 0.0f,  // bottom right

            -1.0f,  1.0f,  0.0f, 1.0f,  // top left
            1.0f, -1.0f,  1.0f, 0.0f,  // bottom right
            1.0f,  1.0f,  1.0f, 1.0f   // top right

        };
        
        GLuint VAO, VBO;
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);

        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        vector<GLuint> VAOtexture = {VAO, texture};
        return VAOtexture;
    }
    void renderScene(const vector<unsigned char>& pixels, int texWidth, int texHeight) {
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texWidth, texHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram);

        GLint textureLocation = glGetUniformLocation(shaderProgram, "screenTexture");
        glUniform1i(textureLocation, 0);

        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glfwSwapBuffers(window);
        glfwPollEvents();
    };
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        if (action == GLFW_PRESS) {
            if (key == GLFW_KEY_G) {
                useGeodesics = !useGeodesics;
                cout << "Geodesics: " << (useGeodesics ? "ON\n" : "OFF\n");
            }
        }
    }
};
Engine engine;
struct BlackHole {
    vec3 position;
    double mass;
    double radius;
    double r_s;

    BlackHole(vec3 pos, float m) : position(pos), mass(m) {r_s = 2.0 * G * mass / (c*c);}
    bool Intercept(float px, float py, float pz) const {
        float dx = px - position.x;
        float dy = py - position.y;
        float dz = pz - position.z;
        float dist2 = dx * dx + dy * dy + dz * dz;
        return dist2 < r_s * r_s;
    }
};