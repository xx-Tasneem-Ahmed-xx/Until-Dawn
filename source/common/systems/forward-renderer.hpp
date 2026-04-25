#pragma once

#include "../ecs/world.hpp"
#include "../components/camera.hpp"
#include "../components/mesh-renderer.hpp"
#include "../components/light.hpp"
#include "../asset-loader.hpp"
#include "../ui/health-bar.hpp"

#include <glad/gl.h>
#include <vector>
#include <algorithm>

namespace our
{

    // The render command stores command that tells the renderer that it should draw
    // the given mesh at the given localToWorld matrix using the given material
    // The renderer will fill this struct using the mesh renderer components
    struct RenderCommand
    {
        glm::mat4 localToWorld;
        glm::vec3 center;
        Mesh *mesh;
        Material *material;
        const std::vector<glm::mat4> *skinMatrices = nullptr;
        int skinJointCount = 0;
    };

    // A forward renderer is a renderer that draw the object final color directly to the framebuffer
    // In other words, the fragment shader in the material should output the color that we should see on the screen
    // This is different from more complex renderers that could draw intermediate data to a framebuffer before computing the final color
    // In this project, we only need to implement a forward renderer
    class ForwardRenderer
    {
        // These window size will be used on multiple occasions (setting the viewport, computing the aspect ratio, etc.)
        glm::ivec2 windowSize;
        // These are two vectors in which we will store the opaque and the transparent commands.
        // We define them here (instead of being local to the "render" function) as an optimization to prevent reallocating them every frame
        std::vector<RenderCommand> opaqueCommands;
        std::vector<RenderCommand> transparentCommands;
        // Objects used for rendering a skybox
        Mesh *skySphere = nullptr;
        TexturedMaterial *skyMaterial = nullptr;
        GLuint postprocessFrameBuffer = 0, postProcessVertexArray = 0;
        Texture2D *colorTarget = nullptr, *depthTarget = nullptr;
        TexturedMaterial *postprocessMaterial = nullptr;
        // Objects used for crosshair overlay
        ShaderProgram *crosshairShader = nullptr;
        HealthBar healthBar;
        float muzzleFlashStrength = 0.0f;
        glm::vec2 muzzleFlashCenter;
        // Elapsed time in seconds — fed to animated postprocess shaders (e.g. film grain)
        float elapsedTime = 0.0f;
        // Global scene exposure factor for postprocessing (dark-to-bright transitions).
        float sceneExposure = 1.0f;
        bool overlaysVisible = true;

    public:
        // Initialize the renderer including the sky and the Postprocessing objects.
        // windowSize is the width & height of the window (in pixels).
        void initialize(glm::ivec2 windowSize, const nlohmann::json &config);
        // Clean up the renderer
        void destroy();
        // This function should be called every frame to draw the given world
        void render(World *world);
        // Update elapsed time so animated postprocess shaders receive a time uniform
        void setTime(float t) { elapsedTime = t; }
        // Sets global scene exposure used by postprocessing shader.
        // Expected range is [0, 2] where 1 is neutral.
        void setSceneExposure(float value) { sceneExposure = glm::clamp(value, 0.0f, 2.0f); }

        // Sets the screen flash intensity used by postprocessing shaders.
        // Expected range is [0, 1].
        void setMuzzleFlashStrength(float value) { muzzleFlashStrength = glm::clamp(value, 0.0f, 1.0f); }
        // Sets the UV center of the muzzle flash on the screen.
        void setMuzzleFlashCenter(glm::vec2 value) { muzzleFlashCenter = glm::clamp(value, glm::vec2(0.0f), glm::vec2(1.0f)); }

        // Updates health bar input values. maxHealth values <= 0 are handled safely.
        void setHealth(float currentHealth, float maxHealth, float deltaTime = 0.0f) { healthBar.setHealth(currentHealth, maxHealth, deltaTime); }

        // Shows/hides HUD overlays drawn by renderer (health bar + crosshair).
        void setOverlaysVisible(bool value) { overlaysVisible = value; }
    };

}