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
const float EPSILON = 1e-10;

struct Ray {
    // Cartesian Coordinates
    float x, y, z;
    // Polar Coordinates
    float r, phi, theta;
    float dr, dphi, dtheta;
    float E, L, f, dt_dl;

    Ray(vec3 position, vec3 dir, float rad_s) : x(position.x), y(position.y), z(position.z) {
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

vector<Ray> rays;

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
    double width = 1e11;
    double height = 7.5e10;

	GLuint computeShader;
    GLuint rayBuffer;

    Engine() {
        if (!glfwInit()) {
            std::cerr << "Failed to initialize GLFW" << std::endl;
            exit(EXIT_FAILURE);
        }
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
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

        std::cout << "Viewport: " << width << " x " << height << " meters" << std::endl;
    }

    void start() {
        glClear(GL_COLOR_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        double left = -width;
        double right = width;
        double bottom = -height;
        double top = height;
        glOrtho(left, right, bottom, top, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    }

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

    void loadShaders() {
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
            return;
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
            return;
        }
        computeShader = computeProgram; // Store program ID for later use
    }

    void createBuffers(vector<Ray> rays) {
        // Create and bind ray buffer
        glGenBuffers(1, &rayBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, rayBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(Ray) * rays.size(), rays.data(), GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, rayBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    void runComputeShader(GLuint computeShader, vector<Ray> rays) {
        int workGroupsX = ceil((rays.size()) / 256.0);
        int workGroupsY = 1;
        int workGroupsZ = 1;
        glDispatchCompute(workGroupsX, workGroupsY, workGroupsZ);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    void deleteShaders() {
        glDeleteProgram(computeShader);
	}
};

Engine engine;

struct BlackHole {
    vec2 pos;
    double m;
    float rad_s;

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

int main() {
    float v0 = c;
    for (float y = -engine.height; y < engine.height; y += 0.2e10) {
        rays.push_back(Ray(vec3(-engine.width, y, 0.0), vec3(v0, 0.0, 0.0), blackhole1.rad_s));
        trails.push_back(Trail());
        rays.push_back(Ray(vec3(engine.width, y, 0.0), vec3(-v0, 0.0, 0.0), blackhole1.rad_s));
        trails.push_back(Trail());
    }

    cout << "Initialized " << rays.size() << " rays." << endl;
	cout << ceil(rays.size() / 16.0) << " work groups will be dispatched per frame." << endl;

    engine.loadShaders();
    engine.createBuffers(rays);
	glUseProgram(engine.computeShader);
    glUniform1f(glGetUniformLocation(engine.computeShader, "rad_s"), blackhole1.rad_s);

    std::cout << "Starting Black Hole Simulator..." << std::endl;

    engine.start();

    while (!glfwWindowShouldClose(engine.window)) {
		glClear(GL_COLOR_BUFFER_BIT);
        engine.runComputeShader(engine.computeShader, rays);

        glMemoryBarrier(GL_ALL_BARRIER_BITS);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, engine.rayBuffer);
		glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(Ray) * rays.size(), rays.data());
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        for (size_t i = 0; i < rays.size(); ++i) {
            trails[i].update(rays[i]);
        }

        blackhole1.draw();

        // Draw rays and their trails
        for (size_t i = 0; i < rays.size(); ++i) {
            rays[i].draw();
            trails[i].draw();
        }

        glfwSwapBuffers(engine.window);
        glfwPollEvents();
    }
    glDeleteBuffers(1, &engine.rayBuffer);
    glDeleteProgram(engine.computeShader);
    glfwTerminate();

    return 0;
}