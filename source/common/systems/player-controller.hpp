#pragma once

// ============================================================
//  PlayerControllerSystem
//
//  Replaces the old free-camera mouse-look with a crosshair-
//  based 3rd-person aiming scheme:
//
//  • Mouse delta  →  moves an on-screen crosshair (NDC space)
//  • Crosshair position  →  derives an aim direction ray
//  • Player body  →  smoothly yaws to face the aim direction
//  • Camera       →  follows the player via CameraFollowComponent
//                    (unchanged – no mouse pitch on the camera)
//
//  Call order inside your game loop:
//      system.update(world, deltaTime, mouseDeltaX, mouseDeltaY,
//                    windowWidth, windowHeight,
//                    keys);   // WASD bitmask or whatever your input uses
// ============================================================

#include "../ecs/world.hpp"
#include "../components/player.hpp"
#include "../components/camera-follow.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>

namespace our
{

    class PlayerControllerSystem
    {
    public:
        // Call once per frame from your PlayState::onIdle (or equivalent).
        //
        // @param world          – the active ECS world
        // @param deltaTime      – seconds since last frame
        // @param mouseDeltaX    – raw mouse X delta this frame (pixels)
        // @param mouseDeltaY    – raw mouse Y delta this frame (pixels)
        // @param windowW/H      – current framebuffer dimensions
        // @param window         – GLFW window handle (for key queries)
        void update(World        *world,
                    float         deltaTime,
                    double        mouseDeltaX,
                    double        mouseDeltaY,
                    int           windowW,
                    int           windowH,
                    GLFWwindow   *window);

        // Returns the crosshair position in screen pixels for HUD rendering.
        // Call after update().
        glm::vec2 getCrosshairScreenPos(int windowW, int windowH,
                                        const PlayerComponent *player) const;

    private:
        // Smoothly rotate a body yaw angle toward a target yaw using the
        // shortest angular path.  Returns the updated angle (radians).
        static float smoothYaw(float current, float target,
                               float turnSpeedDeg, float dt);
    };

} // namespace our