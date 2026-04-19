#include "hud-system.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace our
{

    void HUDSystem::initialize()
    {
        if (shader)
            return;

        shader = new ShaderProgram();
        shader->attach("assets/shaders/hud-sdf.vert", GL_VERTEX_SHADER);
        shader->attach("assets/shaders/hud-sdf.frag", GL_FRAGMENT_SHADER);
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

    void HUDSystem::destroy()
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

    void HUDSystem::renderAmmoHUD(glm::ivec2 windowSize, const WeaponComponent *weapon) const
    {
        if (!shader || !vertexArray || !weapon || windowSize.x <= 0 || windowSize.y <= 0)
            return;

        const GLboolean wasDepthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
        const GLboolean wasBlendEnabled = glIsEnabled(GL_BLEND);

        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glm::mat4 projection = glm::ortho(0.0f, (float)windowSize.x, 0.0f, (float)windowSize.y, -1.0f, 1.0f);

        float rowWidth = segmentCount * segmentSize.x + (segmentCount - 1) * segmentSpacing;
        glm::vec2 start = glm::vec2(windowSize.x - margin.x - rowWidth, margin.y);

        int loadedSegments = 0;
        if (weapon->maxAmmo > 0)
        {
            float ratio = glm::clamp((float)weapon->ammo / (float)weapon->maxAmmo, 0.0f, 1.0f);
            loadedSegments = static_cast<int>(std::round(ratio * segmentCount));
        }
        loadedSegments = std::clamp(loadedSegments, 0, segmentCount);

        const glm::vec4 loadedColor = glm::vec4(1.0f, 0.72f, 0.20f, 0.95f);
        const glm::vec4 spentColor = glm::vec4(0.10f, 0.10f, 0.10f, 0.85f);

        for (int i = 0; i < segmentCount; ++i)
        {
            glm::vec2 position = start + glm::vec2(i * (segmentSize.x + segmentSpacing), 0.0f);
            drawQuad(projection, position, segmentSize, i < loadedSegments ? loadedColor : spentColor, 1);
        }

        if (weapon->isReloading)
        {
            float progress = 1.0f;
            if (weapon->reloadTime > 0.0001f)
                progress = glm::clamp(weapon->reloadTimer / weapon->reloadTime, 0.0f, 1.0f);

            glm::vec2 barPosition = start + glm::vec2(0.0f, segmentSize.y + reloadBarGap);
            glm::vec2 barSize = glm::vec2(rowWidth * progress, reloadBarHeight);
            drawQuad(projection, barPosition, barSize, glm::vec4(1.0f, 1.0f, 1.0f, 0.95f), 0);
        }

        if (wasDepthTestEnabled)
            glEnable(GL_DEPTH_TEST);
        else
            glDisable(GL_DEPTH_TEST);

        glDepthMask(GL_TRUE);

        if (wasBlendEnabled)
            glEnable(GL_BLEND);
        else
            glDisable(GL_BLEND);
    }

    void HUDSystem::drawQuad(const glm::mat4 &projection, const glm::vec2 &position, const glm::vec2 &size, const glm::vec4 &color, int shapeMode) const
    {
        if (size.x <= 0.0f || size.y <= 0.0f)
            return;

        shader->use();
        glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(position, 0.0f));
        model = glm::scale(model, glm::vec3(size, 1.0f));

        shader->set("projection", projection);
        shader->set("model", model);
        shader->set("color", color);
        shader->set("shapeMode", shapeMode);

        glBindVertexArray(vertexArray);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

}
