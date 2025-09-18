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
#include <sstream>
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

struct Ray;
void rk4step(Ray& ray, double rad_s, double dl, vec4 trig_funcs);

struct Engine {
    GLFWwindow* window;
    int HEIGHT = 750;
    int WIDTH = 1000;
    double width = 1e11;
    double height = 7.5e10;

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

    std::string loadShaderSource(const char* filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "Failed to open shader file: " << filepath << std::endl;
            return "";
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    void createBuffers() {
        // Load your compute shader file
        std::string computeSource = loadShaderSource("geodesic.comp");
        const char* source = computeSource.c_str();

        // Create and compile the compute shader
        GLuint computeShader = glCreateShader(GL_COMPUTE_SHADER);
        glShaderSource(computeShader, 1, &source, NULL);
        glCompileShader(computeShader);

        // Create and bind buffers
        GLuint RayBuffer, ScreenBuffer;
        glGenBuffers(1, &RayBuffer);
        glGenBuffers(1, &ScreenBuffer);

        // Bind buffers to shader binding points
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, RayBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ScreenBuffer);

        // Dispatch compute shader
		int workGroupsX = (WIDTH + 15) / 16;
		int workGroupsY = (HEIGHT + 15) / 16;
		int workGroupsZ = 1;
        glUseProgram(computeShader);
        glDispatchCompute(workGroupsX, workGroupsY, workGroupsZ);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
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

struct Ray {
    // Cartesian Coordinates
    double x; double y; double z;
    // Polar Coordinates
    double r; double phi; double theta;
	double dr; double dphi; double dtheta;

    vector<vec2> trail;
    double E, L;

    Ray(vec3 position, vec3 dir) : x(position.x), y(position.y), z(position.z) {
        r = sqrt(x * x + y * y + z * z);
        phi = atan2(y, x);
        theta = acos(z / r);
        dr = dir.x * sin(theta) * cos(phi) + dir.y * sin(theta) * sin(phi) + cos(theta) * dir.z;
        dphi = (dir.x * -sin(phi) + dir.y * sin(phi)) / (r * sin(theta));
        dtheta = (dir.x * cos(theta) * cos(phi) + dir.y * cos(theta) * sin(phi) - sin(theta) * dir.z) / r;

        L = r * r * sin(theta) * dphi;
        double f = 1.0 - blackhole1.rad_s / r;
        double dt_dL = sqrt((dr * dr) / f + r * r * dtheta * dtheta + sin(theta) * sin(theta) * dphi * dphi);
        E = f * dt_dL;

        trail.push_back({ x,y });
    }

    void draw() {
        glPointSize(2.0f);
        glColor3f(1.0f, 1.0f, 1.0f);
        glBegin(GL_POINTS);
        glVertex2f(x, y);
        glEnd();

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glLineWidth(2.0f);

        size_t N = trail.size();
        if (N < 2) return;

        glBegin(GL_LINE_STRIP);
        for (size_t i = 0; i < N; ++i) {
            float alpha = float(i) / float(N - 1);
            glColor4f(1.0, 1.0, 1.0, alpha);
            glVertex2d(trail[i].x, trail[i].y);
        }
        glEnd();
    }

    void step(BlackHole blackhole, double dl) {
        if (r <= blackhole.rad_s) return;

        double sin_theta = sin(theta);
        double cos_theta = cos(theta);
        double sin_phi = sin(phi);
        double cos_phi = cos(phi);

		if (abs(sin_theta) < EPSILON) sin_theta = 0;
		if (abs(cos_theta) < EPSILON) cos_theta = 0;
		if (abs(sin_phi) < EPSILON) sin_phi = 0;
		if (abs(cos_phi) < EPSILON) cos_phi = 0;

        vec4 trig_funcs = vec4(sin_theta, cos_theta, sin_phi, cos_phi);

        rk4step(*this, blackhole.rad_s, dl, trig_funcs);

        x = r * sin_theta * cos_phi;
        y = r * sin_theta * sin_phi;
        z = r * cos_theta;

        trail.push_back({ x, y });
    }
};

vector<Ray> rays;

int main() {
    double v0 = 1;
    for (float y = -engine.height; y < engine.height; y += 0.2e10) {
        rays.push_back(Ray(vec3(-engine.width, y, 1.0e9), vec3(v0, 0.0, 0.0)));
        rays.push_back(Ray(vec3(engine.width, y, -1.0e9), vec3(-v0, 0.0, 0.0)));
    }

    std::cout << "Starting Black Hole Simulator..." << std::endl;

    while (!glfwWindowShouldClose(engine.window)) {
        engine.start();
        blackhole1.draw();

        for (auto& ray : rays) {
            ray.step(blackhole1, 5e7);
            ray.draw();
        }
        glfwSwapBuffers(engine.window);
        glfwPollEvents();
    }
    glfwTerminate();

    return 0;
}