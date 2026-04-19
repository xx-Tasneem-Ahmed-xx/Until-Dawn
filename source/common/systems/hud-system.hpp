#pragma once

#include "../shader/shader.hpp"
#include "../components/weapon.hpp"

#include <glad/gl.h>
#include <glm/glm.hpp>

namespace our
{

    class HUDSystem
    {
        ShaderProgram *shader = nullptr;
        GLuint vertexArray = 0;
        GLuint vertexBuffer = 0;

        int segmentCount = 30;
        glm::vec2 segmentSize = glm::vec2(8.0f, 22.0f);
        float segmentSpacing = 4.0f;
        glm::vec2 margin = glm::vec2(32.0f, 28.0f);
        float reloadBarHeight = 3.0f;
        float reloadBarGap = 7.0f;

        void drawQuad(const glm::mat4 &projection, const glm::vec2 &position, const glm::vec2 &size, const glm::vec4 &color, int shapeMode) const;

    public:
        void initialize();
        void destroy();
        void renderAmmoHUD(glm::ivec2 windowSize, const WeaponComponent *weapon) const;
    };

}
