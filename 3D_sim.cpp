#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <iostream>
#include <algorithm>
#define _USE_MATH_DEFINES
#include <cmath>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <chrono>
#include <fstream>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#pragma comment(lib, "opengl32.lib")
#endif
using namespace glm;
using namespace std;
using Clock = std::chrono::high_resolution_clock;

const double G = 6.67e-11;
const double c = 299792458.0;
const double EPSILON = 1e-10;

struct Ray {
    // Cartesian Coordinates
    float x, y, z;
    // Polar Coordinates
    float r, phi, theta;
    float dr, dphi, dtheta;
    float E, L, f, dt_dl;

    Ray(vec3 position, vec3 dir, double rad_s) : x(position.x), y(position.y), z(position.z) {
        r = sqrt(x * x + y * y + z * z);

        phi = atan(y, x);

        theta = acos(z / r);

        // Calculate trig functions once
        float sin_theta = sin(theta);
        float cos_theta = cos(theta);
        float sin_phi = sin(phi);
        float cos_phi = cos(phi);

        dr = dir.x * sin_theta * cos_phi +
             dir.y * sin_theta * sin_phi +
             dir.z * cos_theta;

        dtheta = (dir.x * cos_theta * cos_phi +
                  dir.y * cos_theta * sin_phi -
                  dir.z * sin_theta) / r;

        dphi = (-dir.x * sin_phi + dir.y * cos_phi) / (r * sin_theta);

        L = r * r * sin(theta) * dphi;
        f = 1.0 - rad_s / r;
        dt_dl = sqrt((dr * dr) / f + r * r * (dtheta * dtheta + sin(theta) * sin(theta) * dphi * dphi));
        E = f * dt_dl;
    }

    void draw() {
        glPointSize(2.0f);
        glColor3f(1.0f, 1.0f, 1.0f);
        glBegin(GL_POINTS);
        glVertex2f(x, y);
        glEnd();
    }
};

struct Trail {
	vector<vec2> trail;

    Trail() {}

    void update(Ray& ray) {
		trail.push_back(vec2(ray.x, ray.y));
	}
    void draw() {
        if (trail.size() < 2) return;

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glLineWidth(1.5f);

        glBegin(GL_LINE_STRIP);
        for (size_t i = 0; i < trail.size(); ++i) {
            float alpha = float(i) / float(trail.size() - 1);
            glColor4f(1.0f, 1.0f, 1.0f, alpha);
            glVertex2f(trail[i].x, trail[i].y);
        }
        glEnd();
    }
};

vector<Trail> trails;

struct Engine {
    GLFWwindow* window;
    int HEIGHT = 750;
    int WIDTH = 1000;
    int COMPUTE_HEIGHT = 300;
	int COMPUTE_WIDTH = 400;
    double width = 1e11;
    double height = 7.5e10;

    GLuint quadVAO;
    GLuint texture;
	GLuint computeShader = 0;
    GLuint shaderProgram;

    Engine() {
        if (!glfwInit()) {
            std::cerr << "Failed to initialize GLFW" << std::endl;
            exit(EXIT_FAILURE);
        }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        window = glfwCreateWindow(WIDTH, HEIGHT, "Black Hole Simulator", NULL, NULL);
        if (!window) {
            std::cerr << "Failed to create GLFW window" << std::endl;
            glfwTerminate();
            exit(EXIT_FAILURE);
        }
        glfwMakeContextCurrent(window);
        GLenum err = glewInit();
        if (err != GLEW_OK) {
            std::cerr << "GLEW initialization failed: " << glewGetErrorString(err) << std::endl;
            exit(EXIT_FAILURE);
        }

        std::cout << "Viewport: " << width << " x " << height << " meters" << std::endl;

        this->shaderProgram = CreateShaderProgram();
        this->computeShader = loadShaders();

        auto result = QuadVAO();
        this->quadVAO = result[0];
        this->texture = result[1];
    }
    
    GLuint CreateShaderProgram() {
        const char* vertexShaderSource = R"(
        #version 430 core
        layout (location = 0) in vec2 aPos;  // Changed to vec2
        layout (location = 1) in vec2 aTexCoord;
        out vec2 TexCoord;
        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);  // Explicit z=0
            TexCoord = aTexCoord;
        })";

        const char* fragmentShaderSource = R"(
        #version 430 core
        in vec2 TexCoord;
        out vec4 FragColor;
        uniform sampler2D screenTexture;
        void main() {
            FragColor = texture(screenTexture, TexCoord);
        })";

        // vertex shader
        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
        glCompileShader(vertexShader);

        // fragment shader
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

    vector<GLuint> QuadVAO() {
        float quadVertices[] = {
            // positions   // texCoords
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

        glTexImage2D(GL_TEXTURE_2D,
            0,             // mip
            GL_RGBA32F,      // internal format
            COMPUTE_WIDTH,
            COMPUTE_HEIGHT,
            0,
            GL_RGBA,
            GL_FLOAT,
            nullptr);

        glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

        vector<GLuint> VAOtexture = { VAO, texture };
        return VAOtexture;
    }

    void renderScene(float rad_s) {
        glUseProgram(computeShader);

        glUniform1d(glGetUniformLocation(computeShader, "rad_s"), rad_s);

        int workGroupsX = (COMPUTE_WIDTH + 15) / 16;
        int workGroupsY = (COMPUTE_HEIGHT + 15) / 16;
        int workGroupsZ = 1;
        glDispatchCompute(workGroupsX, workGroupsY, workGroupsZ);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        // clear screen and draw textured quad
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram);

        glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture);
        GLint textureLocation = glGetUniformLocation(shaderProgram, "screenTexture");
        glUniform1i(textureLocation, 0);

        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glfwSwapBuffers(window);
        glfwPollEvents();
    };

    string loadShaderSource(const char* filepath) {
        ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "Failed to open shader file: " << filepath << std::endl;
            return "";
        }

        stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    GLuint loadShaders() {
        // Load compute shader
        string computeSource = loadShaderSource("geodesic.comp");
        const char* source = computeSource.c_str();

        // Create and compile compute shader
        computeShader = glCreateShader(GL_COMPUTE_SHADER);
        glShaderSource(computeShader, 1, &source, NULL);
        glCompileShader(computeShader);

        // Check for compilation errors
        GLint success;
        GLchar infoLog[512];
        glGetShaderiv(computeShader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(computeShader, 512, NULL, infoLog);
            std::cerr << "Compute shader compilation failed:\n" << infoLog << std::endl;
            return 0;
        }

        // Create and link program
        GLuint computeProgram = glCreateProgram();
        glAttachShader(computeProgram, computeShader);
        glLinkProgram(computeProgram);

        // Check for linking errors
        glGetProgramiv(computeProgram, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(computeProgram, 512, NULL, infoLog);
            std::cerr << "Program linking failed:\n" << infoLog << std::endl;
            return 0;
        }
        computeShader = computeProgram; // Store program ID for later use

		return computeProgram;
    }
};

Engine engine;

struct BlackHole {
    vec2 pos;
    double m;
    double rad_s;

    BlackHole(vec3 position, double mass) : pos(position), m(mass) {
        rad_s = (2.0 * G * mass) / (c * c);

        std::cout << "\n=== BLACK HOLE DEBUG INFO ===" << std::endl;
        std::cout << "Position: (" << pos.x << ", " << pos.y << ")" << std::endl;
        std::cout << "Mass: " << mass << " kg" << std::endl;
        std::cout << "Calculated radius: " << rad_s << " meters" << std::endl;
        std::cout << "Radius as % of viewport width: " << (rad_s / engine.width) * 100 << "%" << std::endl;
        std::cout << "Radius as % of viewport height: " << (rad_s / engine.height) * 100 << "%" << std::endl;

        // Screen pixel calculation
        double pixelsX = (rad_s / engine.width) * (engine.WIDTH / 2.0);
        double pixelsY = (rad_s / engine.height) * (engine.HEIGHT / 2.0);
        std::cout << "Expected size on screen: ~" << pixelsX << " pixels" << std::endl;
        std::cout << "============================\n" << std::endl;
    }

    void draw() {
        glColor3f(0.0f, 0.0f, 1.0f);

        glBegin(GL_TRIANGLE_FAN);
        glVertex2d(pos.x, pos.y);

        for (int i = 0; i <= 100; i++) {
            double angle = 2.0 * M_PI * i / 100.0;
            double x = cos(angle) * rad_s + pos.x;
            double y = sin(angle) * rad_s + pos.y;
            glVertex2d(x, y);
        }
        glEnd();
    }
};

BlackHole blackhole1(vec3(0, 0, 0), 8.0e36);

struct Camera {
    vec3 position;
    vec3 direction;
    float fov;

    Camera(vec3 pos, vec3 dir, float fieldOfView) : position(pos), direction(dir), fov(fieldOfView) {}
};

Camera camera(vec3(0, 0, 1e11), vec3(0, 0, -1), 60.0f);

int main() {
    int num_rays = engine.COMPUTE_HEIGHT * engine.COMPUTE_WIDTH;

    cout << "Initialized " << num_rays << " rays." << endl;
	cout << ceil(num_rays / 16.0) << " work groups will be dispatched per frame." << endl;    

    std::cout << "Starting Black Hole Simulator..." << std::endl;

    cout << "Swartzschild Radius: " << blackhole1.rad_s << endl;

	double* rays = new double[num_rays]();

    GLuint rayBuffer;

    glGenBuffers(1, &rayBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, rayBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(double) * num_rays, rays, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, rayBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    while (!glfwWindowShouldClose(engine.window)) {
        engine.renderScene(blackhole1.rad_s);

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, rayBuffer);
        double* mappedRays = (double*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);

        int centerX = engine.COMPUTE_WIDTH / 2;
        int centerY = engine.COMPUTE_HEIGHT / 2;
        int centerIndex = centerY * engine.COMPUTE_WIDTH + centerX;

        if (mappedRays) {
            cout << mappedRays[centerIndex] << endl;
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        }
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        /*glBindBuffer(GL_SHADER_STORAGE_BUFFER, rayBuffer);
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(double) * num_rays, rays);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);*/

        //int i = 60000;
	    //cout << rays[i] << endl;
    }
    glDeleteProgram(engine.computeShader);
    glfwTerminate();

    return 0;
}