#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "Shader.h"
#include "Camera.h"
#include "Framebuffer.h"
#include "stb_image.h"

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <string>

#ifndef SHADER_DIR
#define SHADER_DIR "shaders"
#endif
#ifndef ASSET_DIR
#define ASSET_DIR "assets"
#endif

static int    g_width  = 1280;
static int    g_height = 720;
static double g_scrollDelta = 0.0;

static void framebufferSizeCallback(GLFWwindow*, int width, int height) {
    g_width  = width;
    g_height = height;
}

static void scrollCallback(GLFWwindow*, double, double yoffset) {
    g_scrollDelta += yoffset;
}

static void glfwErrorCallback(int code, const char* description) {
    std::fprintf(stderr, "GLFW error %d: %s\n", code, description);
}

static GLuint loadTexture(const std::string& path) {
    int w = 0, h = 0, channels = 0;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 0);
    if (!data) {
        std::fprintf(stderr, "Failed to load texture: %s\n", path.c_str());
        return 0;
    }

    GLenum dataFormat = GL_RGB;
    GLint  internalFormat = GL_SRGB8;
    if (channels == 1)      { dataFormat = GL_RED;  internalFormat = GL_R8; }
    else if (channels == 4) { dataFormat = GL_RGBA; internalFormat = GL_SRGB8_ALPHA8; }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, w, h, 0, dataFormat, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    return tex;
}

int main() {
    glfwSetErrorCallback(glfwErrorCallback);

    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to initialize GLFW\n");
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(g_width, g_height, "Black Hole", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetScrollCallback(window, scrollCallback);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::fprintf(stderr, "Failed to initialize GLAD\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    glfwGetFramebufferSize(window, &g_width, &g_height);

    std::printf("OpenGL %s\n", glGetString(GL_VERSION));
    std::printf("GPU:    %s\n", glGetString(GL_RENDERER));

    glfwSwapInterval(1);

    {
        const float quadVerts[] = {
            -1.0f, -1.0f,   1.0f, -1.0f,   1.0f,  1.0f,
            -1.0f, -1.0f,   1.0f,  1.0f,  -1.0f,  1.0f,
        };

        GLuint vao = 0, vbo = 0;
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        const std::string shaderDir = SHADER_DIR;
        Shader shader(shaderDir + "/fullscreen.vert", shaderDir + "/blackhole.frag");
        Shader bright(shaderDir + "/fullscreen.vert", shaderDir + "/bright.frag");
        Shader blur  (shaderDir + "/fullscreen.vert", shaderDir + "/blur.frag");
        Shader post  (shaderDir + "/fullscreen.vert", shaderDir + "/post.frag");

        const GLuint skybox = loadTexture(std::string(ASSET_DIR) + "/starfield.jpg");
        const GLuint disk   = loadTexture(std::string(ASSET_DIR) + "/disk.png");

        // Performance: trace the (expensive) black-hole pass at a fraction of
        // the window resolution and let post.frag upscale it (linear filter).
        // The 'G' key cycles through these scales at runtime.
        const float kRenderScales[] = { 1.0f, 0.75f, 0.5f, 0.4f };
        int   renderScaleIdx = 1;            // start at 0.75x

        int fbWidth  = std::max(1, static_cast<int>(g_width  * kRenderScales[renderScaleIdx]));
        int fbHeight = std::max(1, static_cast<int>(g_height * kRenderScales[renderScaleIdx]));

        Framebuffer sceneFB;
        sceneFB.create(fbWidth, fbHeight, GL_RGBA16F);
        Framebuffer bloomFB[2];
        bloomFB[0].create(fbWidth / 2, fbHeight / 2, GL_RGBA16F);
        bloomFB[1].create(fbWidth / 2, fbHeight / 2, GL_RGBA16F);

        Camera camera(glm::vec3(0.0f, 0.0f, 5.0f),
                      glm::radians(-90.0f), 0.0f, glm::radians(60.0f));

        const float ORBIT_SENSITIVITY = 0.005f;
        const float ELEV_LIMIT = glm::radians(85.0f);
        const float MIN_DIST = 5.0f;
        const float MAX_DIST = 35.0f;

        float  azimuth   = 0.0f;
        float  elevation = glm::radians(16.0f);
        float  distance  = 20.0f;
        bool   dragging  = false;
        double lastX = 0.0, lastY = 0.0;

        if (shader.valid() && bright.valid() && blur.valid() && post.valid()) {
            glBindVertexArray(vao);

            bool  showDisk = true;
            int   metric   = 0;       // 0 = Schwarzschild, 1 = Kerr
            float spin     = 0.25f;   // Kerr spin a, clamped below M = 0.5
            int prevReloadKey = GLFW_RELEASE;
            int prevDiskKey   = GLFW_RELEASE;
            int prevMetricKey = GLFW_RELEASE;
            int prevScaleKey  = GLFW_RELEASE;

            while (!glfwWindowShouldClose(window)) {
                if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                    glfwSetWindowShouldClose(window, true);
                }

                const int reloadKey = glfwGetKey(window, GLFW_KEY_R);
                if (reloadKey == GLFW_PRESS && prevReloadKey == GLFW_RELEASE) {
                    shader.reload();
                    bright.reload();
                    blur.reload();
                    post.reload();
                    std::printf("Shaders reloaded.\n");
                }
                prevReloadKey = reloadKey;

                const int diskKey = glfwGetKey(window, GLFW_KEY_D);
                if (diskKey == GLFW_PRESS && prevDiskKey == GLFW_RELEASE) {
                    showDisk = !showDisk;
                }
                prevDiskKey = diskKey;

                const int metricKey = glfwGetKey(window, GLFW_KEY_K);
                if (metricKey == GLFW_PRESS && prevMetricKey == GLFW_RELEASE) {
                    metric = 1 - metric;
                    std::printf("Metric: %s\n", metric == 1 ? "Kerr" : "Schwarzschild");
                }
                prevMetricKey = metricKey;

                const int scaleKey = glfwGetKey(window, GLFW_KEY_G);
                if (scaleKey == GLFW_PRESS && prevScaleKey == GLFW_RELEASE) {
                    renderScaleIdx = (renderScaleIdx + 1)
                                   % static_cast<int>(std::size(kRenderScales));
                    std::printf("Render scale: %.0f%%\n",
                                kRenderScales[renderScaleIdx] * 100.0f);
                }
                prevScaleKey = scaleKey;

                // Adjust Kerr spin while held: '[' slower, ']' faster.
                if (glfwGetKey(window, GLFW_KEY_LEFT_BRACKET)  == GLFW_PRESS) spin -= 0.005f;
                if (glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS) spin += 0.005f;
                spin = glm::clamp(spin, 0.0f, 0.49f);

                double mx, my;
                glfwGetCursorPos(window, &mx, &my);
                if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
                    if (dragging) {
                        azimuth   += static_cast<float>(mx - lastX) * ORBIT_SENSITIVITY;
                        elevation += static_cast<float>(my - lastY) * ORBIT_SENSITIVITY;
                        elevation = glm::clamp(elevation, -ELEV_LIMIT, ELEV_LIMIT);
                    }
                    dragging = true;
                } else {
                    dragging = false;
                }
                lastX = mx;
                lastY = my;

                if (g_scrollDelta != 0.0) {
                    distance *= std::pow(0.9f, static_cast<float>(g_scrollDelta));
                    distance = glm::clamp(distance, MIN_DIST, MAX_DIST);
                    g_scrollDelta = 0.0;
                }

                const glm::vec3 camPos(distance * std::cos(elevation) * std::cos(azimuth),
                                       distance * std::sin(elevation),
                                       distance * std::cos(elevation) * std::sin(azimuth));
                camera.setPosition(camPos);
                const glm::vec3 fwd = glm::normalize(-camPos);
                camera.setYaw(std::atan2(fwd.z, fwd.x));
                camera.setPitch(std::asin(fwd.y));

                const int desiredW = std::max(1, static_cast<int>(g_width  * kRenderScales[renderScaleIdx]));
                const int desiredH = std::max(1, static_cast<int>(g_height * kRenderScales[renderScaleIdx]));
                if (desiredW != fbWidth || desiredH != fbHeight) {
                    fbWidth = desiredW;
                    fbHeight = desiredH;
                    sceneFB.resize(fbWidth, fbHeight);
                    bloomFB[0].resize(fbWidth / 2, fbHeight / 2);
                    bloomFB[1].resize(fbWidth / 2, fbHeight / 2);
                }

                sceneFB.bind();
                glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT);

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, skybox);
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, disk);

                shader.use();
                shader.setVec2 ("uResolution", glm::vec2(static_cast<float>(fbWidth),
                                                         static_cast<float>(fbHeight)));
                shader.setVec3 ("uCamPos",     camera.position());
                shader.setVec3 ("uCamForward", camera.forward());
                shader.setVec3 ("uCamRight",   camera.right());
                shader.setVec3 ("uCamUp",      camera.up());
                shader.setFloat("uFovY",       camera.fovY());
                shader.setInt  ("uSkybox",     0);
                shader.setInt  ("uDiskTex",    1);
                shader.setFloat("uTime",       static_cast<float>(glfwGetTime()));
                shader.setInt  ("uShowDisk",   showDisk ? 1 : 0);
                shader.setInt  ("uMetric",     metric);
                shader.setFloat("uSpin",       spin);
                glDrawArrays(GL_TRIANGLES, 0, 6);

                bloomFB[0].bind();
                glClear(GL_COLOR_BUFFER_BIT);
                bright.use();
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, sceneFB.texture());
                bright.setInt("uScene", 0);
                glDrawArrays(GL_TRIANGLES, 0, 6);

                const glm::vec2 bloomTexel(1.0f / static_cast<float>(fbWidth / 2),
                                           1.0f / static_cast<float>(fbHeight / 2));
                bool horizontal = true;
                for (int i = 0; i < 6; ++i) {
                    const int dst = horizontal ? 1 : 0;
                    const int src = horizontal ? 0 : 1;
                    bloomFB[dst].bind();
                    glClear(GL_COLOR_BUFFER_BIT);
                    blur.use();
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, bloomFB[src].texture());
                    blur.setInt ("uImage", 0);
                    blur.setInt ("uHorizontal", horizontal ? 1 : 0);
                    blur.setVec2("uTexelSize", bloomTexel);
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                    horizontal = !horizontal;
                }

                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glViewport(0, 0, g_width, g_height);
                glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT);

                post.use();
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, sceneFB.texture());
                post.setInt("uScene", 0);
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, bloomFB[0].texture());
                post.setInt("uBloom", 1);
                glDrawArrays(GL_TRIANGLES, 0, 6);

                glfwSwapBuffers(window);
                glfwPollEvents();
            }
        } else {
            std::fprintf(stderr, "Shader program failed to build\n");
        }

        glDeleteTextures(1, &skybox);
        glDeleteTextures(1, &disk);
        glDeleteBuffers(1, &vbo);
        glDeleteVertexArrays(1, &vao);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
