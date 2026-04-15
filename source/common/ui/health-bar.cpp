#include "health-bar.hpp"

#include "../material/pipeline-state.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace our
{

    void HealthBar::initialize()
    {
        if (shader)
            return;

        shader = new ShaderProgram();
        shader->attach("assets/shaders/ui-unlit.vert", GL_VERTEX_SHADER);
        shader->attach("assets/shaders/ui-unlit.frag", GL_FRAGMENT_SHADER);
        shader->link();

        const GLfloat quadVertices[] = {
            0.0f, 0.0f,
            1.0f, 0.0f,
            1.0f, 1.0f,

            0.0f, 0.0f,
            1.0f, 1.0f,
            0.0f, 1.0f};

        glGenVertexArrays(1, &vertexArray);
        glBindVertexArray(vertexArray);

        glGenBuffers(1, &vertexBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (void *)0);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void HealthBar::destroy()
    {
        if (vertexBuffer)
        {
            glDeleteBuffers(1, &vertexBuffer);
            vertexBuffer = 0;
        }

        if (vertexArray)
        {
            glDeleteVertexArrays(1, &vertexArray);
            vertexArray = 0;
        }

        delete shader;
        shader = nullptr;
    }

    void HealthBar::setHealth(float currentHealth, float maxHealth, float deltaTime)
    {
        float safeMaxHealth = std::max(1.0f, maxHealth);
        targetPercentage = glm::clamp(currentHealth / safeMaxHealth, 0.0f, 1.0f);

        if (!smoothTransition || deltaTime <= 0.0f || smoothingSpeed <= 0.0f)
        {
            displayedPercentage = targetPercentage;
            return;
        }

        float interpolationAlpha = 1.0f - std::exp(-smoothingSpeed * deltaTime);
        displayedPercentage = glm::mix(displayedPercentage, targetPercentage, glm::clamp(interpolationAlpha, 0.0f, 1.0f));
    }

    void HealthBar::render(glm::ivec2 windowSize) const
    {
        if (!shader || !vertexArray || windowSize.x <= 0 || windowSize.y <= 0)
            return;

        PipelineState uiPipelineState{};
        uiPipelineState.depthTesting.enabled = false;
        uiPipelineState.depthMask = false;
        uiPipelineState.blending.enabled = true;
        uiPipelineState.blending.equation = GL_FUNC_ADD;
        uiPipelineState.blending.sourceFactor = GL_SRC_ALPHA;
        uiPipelineState.blending.destinationFactor = GL_ONE_MINUS_SRC_ALPHA;
        uiPipelineState.setup();

        glm::mat4 projection = glm::ortho(0.0f, (float)windowSize.x, (float)windowSize.y, 0.0f, -1.0f, 1.0f);

        if (borderThickness > 0.0f)
        {
            drawQuad(projection,
                     position - glm::vec2(borderThickness),
                     size + glm::vec2(borderThickness * 2.0f),
                     glm::vec4(0.05f, 0.05f, 0.05f, 0.95f));
        }

        drawQuad(projection, position, size, glm::vec4(0.14f, 0.14f, 0.14f, 0.85f));

        float fillWidth = size.x * glm::clamp(displayedPercentage, 0.0f, 1.0f);
        if (fillWidth > 0.0f)
        {
            drawQuad(projection,
                     position,
                     glm::vec2(fillWidth, size.y),
                     computeFillColor(displayedPercentage));
        }
    }

    void HealthBar::drawQuad(const glm::mat4 &projection, glm::vec2 quadPosition, glm::vec2 quadSize, glm::vec4 color) const
    {
        if (quadSize.x <= 0.0f || quadSize.y <= 0.0f)
            return;

        shader->use();
        glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(quadPosition, 0.0f));
        model = glm::scale(model, glm::vec3(quadSize, 1.0f));

        shader->set("projection", projection);
        shader->set("model", model);
        shader->set("color", color);

        glBindVertexArray(vertexArray);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    glm::vec4 HealthBar::computeFillColor(float percentage) const
    {
        percentage = glm::clamp(percentage, 0.0f, 1.0f);

        const glm::vec3 red(0.90f, 0.18f, 0.18f);
        const glm::vec3 orange(0.95f, 0.62f, 0.18f);
        const glm::vec3 green(0.20f, 0.85f, 0.25f);

        glm::vec3 result;
        if (percentage <= 0.30f)
        {
            result = red;
        }
        else if (percentage < 0.70f)
        {
            float t = (percentage - 0.30f) / 0.40f;
            result = glm::mix(red, orange, t);
        }
        else
        {
            float t = (percentage - 0.70f) / 0.30f;
            result = glm::mix(orange, green, t);
        }

        return glm::vec4(result, 0.95f);
    }

}
