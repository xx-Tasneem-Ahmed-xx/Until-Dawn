#pragma once

#include "../shader/shader.hpp"

#include <glad/gl.h>
#include <glm/glm.hpp>

namespace our
{

    class HealthBar
    {
        ShaderProgram *shader = nullptr;
        GLuint vertexArray = 0;
        GLuint vertexBuffer = 0;

        float targetPercentage = 1.0f;
        float displayedPercentage = 1.0f;

        glm::vec2 position = glm::vec2(24.0f, 24.0f);
        glm::vec2 size = glm::vec2(260.0f, 22.0f);
        float borderThickness = 2.0f;

        bool smoothTransition = true;
        float smoothingSpeed = 12.0f;

        void drawQuad(const glm::mat4 &projection, glm::vec2 quadPosition, glm::vec2 quadSize, glm::vec4 color) const;
        glm::vec4 computeFillColor(float percentage) const;

    public:
        void initialize();
        void destroy();

        void setHealth(float currentHealth, float maxHealth, float deltaTime = 0.0f);

        void setPosition(glm::vec2 value) { position = value; }
        void setSize(glm::vec2 value) { size = glm::max(value, glm::vec2(1.0f)); }
        void setBorderThickness(float value) { borderThickness = glm::max(value, 0.0f); }
        void setSmoothTransition(bool value) { smoothTransition = value; }
        void setSmoothingSpeed(float value) { smoothingSpeed = glm::max(value, 0.0f); }

        void render(glm::ivec2 windowSize) const;
    };

}
