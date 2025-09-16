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

double G = 6.67e-11;
double c = 3.00e8;

struct Ray;
void rk4step(Ray& ray, double rad_s, double dl);

struct Engine {
    GLFWwindow* window;
    int HEIGHT = 600;
    int WIDTH = 800;
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
};

Engine engine;

struct BlackHole {
    vec2 pos;
    double m;
    double rad_s;

    BlackHole(vec2 position, double mass) : pos(position), m(mass) {
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

BlackHole blackhole1(vec2(0, 0), 8.0e36);

struct Ray {
    // Cartesian Coordinates
    double x; double y;
    // Polar Coordinates
    double r; double phi;
    double dr; double dphi;

    vector<vec2> trail;
    double E, L;

    Ray(vec2 position, vec2 dir) : x(position.x), y(position.y), r(sqrt(position.x* position.x + position.y * position.y)), 
        phi(atan2(position.y, position.x)), dr(dir.x), dphi(dir.y) {
        r = hypot(x, y);
        phi = atan(y, x);
        dr = dir.x * cos(phi) + dir.y * sin(phi);
        dphi = (-dir.x * sin(phi) + dir.y * cos(phi)) / r;

        L = r * r * dphi;
        double f = 1.0 - blackhole1.rad_s / r;
        double dt_dl = sqrt((dr * dr) / (f * f) + (r * r * dphi * dphi) / f);

        E = f * dt_dl;
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
        if (r < blackhole.rad_s) return;

		rk4step(*this, blackhole.rad_s, dl);

        x = cos(phi) * r;
        y = sin(phi) * r;

        trail.push_back({ x, y });
    }
};

vec4 geodesic(Ray& ray, double rad_s, double dl, double y0[4], double temp[4]) {
    double r = ray.r;
    double dr = ray.dr;
    double dphi = ray.dphi;
    double E = ray.E;

    double f = 1.0 - rad_s / r;

    double dt_dl = E / f;

    double d2r = -(rad_s / (2 * r * r)) * f * (dt_dl * dt_dl)
                + (rad_s / (2 * r * r * f)) * (dr * dr)
                + (r - rad_s) * (dphi * dphi);

    double d2phi = -2.0 * dr * dphi / r;

	temp[0] = y0[0] + dr * dl;
	temp[1] = y0[1] + dphi * dl;
	temp[2] = y0[2] + d2r * dl;
    temp[3] = y0[3] + d2phi * dl;

    return vec4(dr, dphi, d2r, d2phi);
};

void rk4step(Ray& ray, double rad_s, double dl) {
	double y0[4] = { ray.r, ray.phi, ray.dr, ray.dphi };
    double temp[4];
    vec4 dray1 = geodesic(ray, rad_s, dl / 2.0, y0, temp);
    Ray r2 = ray;  r2.r = temp[0]; r2.phi = temp[1]; r2.dr = temp[2]; r2.dphi = temp[3];
    vec4 dray2 = geodesic(r2, rad_s, dl / 2.0, y0, temp);
    Ray r3 = ray; r3.r = temp[0]; r3.phi = temp[1]; r3.dr = temp[2]; r3.dphi = temp[3];
    vec4 dray3 = geodesic(r3, rad_s, dl, y0, temp);
    Ray r4 = ray; r4.r = temp[0]; r4.phi = temp[1]; r4.dr = temp[2]; r4.dphi = temp[3];
    vec4 dray4 = geodesic(r4, rad_s, dl, y0, temp);

    ray.r += (dl / 6.0) * (dray1[0] + 2 * dray2[0] + 2 * dray3[0] + dray4[0]);
    ray.phi += (dl / 6.0) * (dray1[1] + 2 * dray2[1] + 2 * dray3[1] + dray4[1]);
    ray.dr += (dl / 6.0) * (dray1[2] + 2 * dray2[2] + 2 * dray3[2] + dray4[2]);
    ray.dphi += (dl / 6.0) * (dray1[3] + 2 * dray2[3] + 2 * dray3[3] + dray4[3]);
};

vector<Ray> rays;

int main() {
    for (float y = -engine.height; y < engine.height; y += 0.3e10) {
        rays.push_back(Ray(vec2(-engine.width, y), vec2(1.0, 0.0)));
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