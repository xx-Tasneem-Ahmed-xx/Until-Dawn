#include "forward-renderer.hpp"
#include "../mesh/mesh-utils.hpp"
#include "../texture/texture-utils.hpp"
#include "../components/zombie.hpp"
#include <glm/gtx/euler_angles.hpp>

namespace
{
    constexpr int MAX_SKIN_BONES = 128;
    constexpr int MAX_LIGHTS = 16;
}

namespace our
{

    void ForwardRenderer::initialize(glm::ivec2 windowSize, const nlohmann::json &config)
    {
        // First, we store the window size for later use
        this->windowSize = windowSize;

        // Create a vertex array to use for drawing fullscreen passes
        glGenVertexArrays(1, &postProcessVertexArray);

        // Create the shader used to draw a centered screen-space crosshair
        crosshairShader = new ShaderProgram();
        crosshairShader->attach("assets/shaders/fullscreen.vert", GL_VERTEX_SHADER);
        crosshairShader->attach("assets/shaders/crosshair.frag", GL_FRAGMENT_SHADER);
        crosshairShader->link();

        healthBar.initialize();

        // Then we check if there is a sky texture in the configuration
        if (config.contains("sky"))
        {
            // First, we create a sphere which will be used to draw the sky
            this->skySphere = mesh_utils::sphere(glm::ivec2(16, 16));

            // We can draw the sky using the same shader used to draw textured objects
            ShaderProgram *skyShader = new ShaderProgram();
            skyShader->attach("assets/shaders/textured.vert", GL_VERTEX_SHADER);
            skyShader->attach("assets/shaders/textured.frag", GL_FRAGMENT_SHADER);
            skyShader->link();

            // TODO: (Req 10) Pick the correct pipeline state to draw the sky
            //  Hints: the sky will be draw after the opaque objects so we would need depth testing but which depth funtion should we pick?
            //  We will draw the sphere from the inside, so what options should we pick for the face culling.
            PipelineState skyPipelineState{};
            skyPipelineState.depthTesting.enabled = true;
            skyPipelineState.depthTesting.function = GL_LEQUAL;
            skyPipelineState.faceCulling.enabled = true;
            skyPipelineState.faceCulling.culledFace = GL_FRONT;

            // Load the sky texture (note that we don't need mipmaps since we want to avoid any unnecessary blurring while rendering the sky)
            std::string skyTextureFile = config.value<std::string>("sky", "");
            Texture2D *skyTexture = texture_utils::loadImage(skyTextureFile, false);

            // Setup a sampler for the sky
            Sampler *skySampler = new Sampler();
            skySampler->set(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            skySampler->set(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            skySampler->set(GL_TEXTURE_WRAP_S, GL_REPEAT);
            skySampler->set(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            // Combine all the aforementioned objects (except the mesh) into a material
            this->skyMaterial = new TexturedMaterial();
            this->skyMaterial->shader = skyShader;
            this->skyMaterial->texture = skyTexture;
            this->skyMaterial->sampler = skySampler;
            this->skyMaterial->pipelineState = skyPipelineState;
            this->skyMaterial->tint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
            this->skyMaterial->alphaThreshold = 1.0f;
            this->skyMaterial->transparent = false;
        }

        // Then we check if there is a postprocessing shader in the configuration
        if (config.contains("postprocess"))
        {
            // TODO: (Req 11) Create a framebuffer
            glGenFramebuffers(1, &postprocessFrameBuffer);
            glBindFramebuffer(GL_FRAMEBUFFER, postprocessFrameBuffer);

            // TODO: (Req 11) Create a color and a depth texture and attach them to the framebuffer
            //  Hints: The color format can be (Red, Green, Blue and Alpha components with 8 bits for each channel).
            //  The depth format can be (Depth component with 24 bits).
            colorTarget = new Texture2D();
            colorTarget->bind();
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, windowSize.x, windowSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTarget->getOpenGLName(), 0);

            depthTarget = new Texture2D();
            depthTarget->bind();
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, windowSize.x, windowSize.y, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTarget->getOpenGLName(), 0);

            // TODO: (Req 11) Unbind the framebuffer just to be safe
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            // Create a sampler to use for sampling the scene texture in the post processing shader
            Sampler *postprocessSampler = new Sampler();
            postprocessSampler->set(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            postprocessSampler->set(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            postprocessSampler->set(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            postprocessSampler->set(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            // Create the post processing shader
            ShaderProgram *postprocessShader = new ShaderProgram();
            postprocessShader->attach("assets/shaders/fullscreen.vert", GL_VERTEX_SHADER);
            postprocessShader->attach(config.value<std::string>("postprocess", ""), GL_FRAGMENT_SHADER);
            postprocessShader->link();

            // Create a post processing material
            postprocessMaterial = new TexturedMaterial();
            postprocessMaterial->shader = postprocessShader;
            postprocessMaterial->texture = colorTarget;
            postprocessMaterial->sampler = postprocessSampler;
            // The default options are fine but we don't need to interact with the depth buffer
            // so it is more performant to disable the depth mask
            postprocessMaterial->pipelineState.depthMask = false;
        }
    }

    void ForwardRenderer::destroy()
    {
        // Delete all objects related to the sky
        if (skyMaterial)
        {
            delete skySphere;
            delete skyMaterial->shader;
            delete skyMaterial->texture;
            delete skyMaterial->sampler;
            delete skyMaterial;
        }
        // Delete all objects related to post processing
        if (postprocessMaterial)
        {
            glDeleteFramebuffers(1, &postprocessFrameBuffer);
            delete colorTarget;
            delete depthTarget;
            delete postprocessMaterial->sampler;
            delete postprocessMaterial->shader;
            delete postprocessMaterial;
        }

        if (postProcessVertexArray)
        {
            glDeleteVertexArrays(1, &postProcessVertexArray);
            postProcessVertexArray = 0;
        }

        delete crosshairShader;
        crosshairShader = nullptr;

        healthBar.destroy();
    }

    void ForwardRenderer::render(World *world)
    {
        // First of all, we search for a camera and for all the mesh renderers
        CameraComponent *camera = nullptr;
        opaqueCommands.clear();
        transparentCommands.clear();
        for (auto entity : world->getEntities())
        {
            // If we hadn't found a camera yet, we look for a camera in this entity
            if (!camera)
                camera = entity->getComponent<CameraComponent>();
            // If this entity has a mesh renderer component
            if (auto meshRenderer = entity->getComponent<MeshRendererComponent>(); meshRenderer)
            {
                if (!meshRenderer->mesh || !meshRenderer->material || !meshRenderer->material->shader)
                    continue;

                // We construct a command from it
                RenderCommand command;
                command.localToWorld = meshRenderer->getOwner()->getLocalToWorldMatrix();
                command.center = glm::vec3(command.localToWorld * glm::vec4(0, 0, 0, 1));
                command.mesh = meshRenderer->mesh;
                command.material = meshRenderer->material;

                if (auto zombie = entity->getComponent<ZombieComponent>())
                {
                    command.skinMatrices = &zombie->skinMatrices;
                    command.skinJointCount = static_cast<int>(zombie->skinMatrices.size());
                }

                // if it is transparent, we add it to the transparent commands list
                if (command.material->transparent)
                {
                    transparentCommands.push_back(command);
                }
                else
                {
                    // Otherwise, we add it to the opaque command list
                    opaqueCommands.push_back(command);
                }
            }
        }

        // Collect all lights in the scene
        std::vector<LightComponent *> lights;
        for (auto entity : world->getEntities())
        {
            if (auto light = entity->getComponent<LightComponent>(); light)
            {
                lights.push_back(light);
            }
        }

        // If there is no camera, we return (we cannot render without a camera)
        if (camera == nullptr)
            return;

        // TODO: (Req 9) Modify the following line such that "cameraForward" contains a vector pointing the camera forward direction
        //  HINT: See how you wrote the CameraComponent::getViewMatrix, it should help you solve this one
        glm::vec3 cameraPosition = glm::vec3(camera->getOwner()->getLocalToWorldMatrix() * glm::vec4(0.0, 0.0, 0.0, 1.0));
        std::sort(transparentCommands.begin(), transparentCommands.end(), [cameraPosition](const RenderCommand &first, const RenderCommand &second)
                  {
            //TODO: (Req 9) Finish this function
            // HINT: the following return should return true "first" should be drawn before "second". 
            return glm::distance(first.center, cameraPosition) > glm::distance(second.center, cameraPosition); });

        // TODO: (Req 9) Get the camera ViewProjection matrix and store it in VP
        glm::mat4 VP = camera->getProjectionMatrix(windowSize) * camera->getViewMatrix();

        // TODO: (Req 9) Set the OpenGL viewport using viewportStart and viewportSize
        glViewport(0, 0, windowSize.x, windowSize.y);

        // TODO: (Req 9) Set the clear color to black and the clear depth to 1
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClearDepth(1.0f);

        // TODO: (Req 9) Set the color mask to true and the depth mask to true (to ensure the glClear will affect the framebuffer)
        glColorMask(true, true, true, true);
        glDepthMask(true);

        // If there is a postprocess material, bind the framebuffer
        if (postprocessMaterial)
        {
            // TODO: (Req 11) bind the framebuffer
            glBindFramebuffer(GL_FRAMEBUFFER, postprocessFrameBuffer);
        }

        // TODO: (Req 9) Clear the color and depth buffers
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // TODO: (Req 9) Draw all the opaque commands
        //  Don't forget to set the "transform" uniform to be equal the model-view-projection matrix for each render command
        for (auto &command : opaqueCommands)
        {
            command.material->setup();

            glm::mat4 transform = VP * command.localToWorld;
            command.material->shader->set("transform", transform);
            command.material->shader->set("M", command.localToWorld);
            command.material->shader->set("M_IT", glm::transpose(glm::inverse(command.localToWorld)));
            command.material->shader->set("eye_position", cameraPosition);

            int lightCount = std::min(static_cast<int>(lights.size()), MAX_LIGHTS);
            command.material->shader->set("light_count", lightCount);
            for (int i = 0; i < lightCount; ++i)
            {
                auto *light = lights[i];
                glm::mat4 lightWorld = light->getOwner()->getLocalToWorldMatrix();
                glm::vec3 lightPosition = glm::vec3(lightWorld * glm::vec4(0, 0, 0, 1));
                glm::vec3 lightDirection = glm::normalize(glm::vec3(lightWorld * glm::vec4(0, 0, -1, 0)));

                std::string prefix = "lights[" + std::to_string(i) + "].";
                command.material->shader->set(prefix + "type", static_cast<int>(light->lightType));
                command.material->shader->set(prefix + "position", lightPosition);
                command.material->shader->set(prefix + "direction", lightDirection);
                command.material->shader->set(prefix + "color", light->diffuse);
                command.material->shader->set(prefix + "attenuation", light->attenuation);
                command.material->shader->set(prefix + "cone_angles", light->cone_angles);
            }

            if (command.mesh->hasGLTFBaseColorTexture())
            {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, command.mesh->getGLTFBaseColorTextureID());
                command.material->shader->set("uBaseColorTex", 0);
                command.material->shader->set("hasTexture", 1);
            }
            else
            {
                command.material->shader->set("hasTexture", 0);
            }

            if (command.mesh->hasSkinning() && command.skinMatrices && !command.skinMatrices->empty())
            {
                int boneCount = std::min({static_cast<int>(command.skinMatrices->size()),
                                          static_cast<int>(command.mesh->getSkinJointNodes().size()),
                                          MAX_SKIN_BONES});
                command.material->shader->set("hasSkinning", 1);
                command.material->shader->set("boneCount", boneCount);
                if (boneCount > 0)
                {
                    command.material->shader->setMat4Array("uBones", command.skinMatrices->data(), boneCount);
                }
            }
            else
            {
                command.material->shader->set("hasSkinning", 0);
                command.material->shader->set("boneCount", 0);
            }
            command.mesh->draw();
        }
        // If there is a sky material, draw the sky
        if (this->skyMaterial)
        {
            // TODO: (Req 10) setup the sky material
            this->skyMaterial->setup();

            // TODO: (Req 10) Get the camera position
            glm::vec3 cameraPosition = glm::vec3(camera->getOwner()->getLocalToWorldMatrix() * glm::vec4(0.0, 0.0, 0.0, 1.0));

            // TODO: (Req 10) Create a model matrix for the sy such that it always follows the camera (sky sphere center = camera position)
            glm::mat4 skyModelMatrix = glm::translate(glm::mat4(1.0f), cameraPosition);

            // TODO: (Req 10) We want the sky to be drawn behind everything (in NDC space, z=1)
            //  We can acheive the is by multiplying by an extra matrix after the projection but what values should we put in it?
            glm::mat4 alwaysBehindTransform(
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 1.0f);
            // TODO: (Req 10) set the "transform" uniform
            glm::mat4 skyTransform = alwaysBehindTransform * VP * skyModelMatrix;
            this->skyMaterial->shader->set("transform", skyTransform);

            // TODO: (Req 10) draw the sky sphere
            this->skySphere->draw();
        }
        // TODO: (Req 9) Draw all the transparent commands
        //  Don't forget to set the "transform" uniform to be equal the model-view-projection matrix for each render command
        for (auto &command : transparentCommands)
        {
            command.material->setup();

            glm::mat4 transform = VP * command.localToWorld;
            command.material->shader->set("transform", transform);
            command.material->shader->set("M", command.localToWorld);
            command.material->shader->set("M_IT", glm::transpose(glm::inverse(command.localToWorld)));
            command.material->shader->set("eye_position", cameraPosition);

            int lightCount = std::min(static_cast<int>(lights.size()), MAX_LIGHTS);
            command.material->shader->set("light_count", lightCount);
            for (int i = 0; i < lightCount; ++i)
            {
                auto *light = lights[i];
                glm::mat4 lightWorld = light->getOwner()->getLocalToWorldMatrix();
                glm::vec3 lightPosition = glm::vec3(lightWorld * glm::vec4(0, 0, 0, 1));
                glm::vec3 lightDirection = glm::normalize(glm::vec3(lightWorld * glm::vec4(0, 0, -1, 0)));

                std::string prefix = "lights[" + std::to_string(i) + "].";
                command.material->shader->set(prefix + "type", static_cast<int>(light->lightType));
                command.material->shader->set(prefix + "position", lightPosition);
                command.material->shader->set(prefix + "direction", lightDirection);
                command.material->shader->set(prefix + "color", light->diffuse);
                command.material->shader->set(prefix + "attenuation", light->attenuation);
                command.material->shader->set(prefix + "cone_angles", light->cone_angles);
            }

            if (command.mesh->hasGLTFBaseColorTexture())
            {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, command.mesh->getGLTFBaseColorTextureID());
                command.material->shader->set("uBaseColorTex", 0);
                command.material->shader->set("hasTexture", 1);
            }
            else
            {
                command.material->shader->set("hasTexture", 0);
            }

            if (command.mesh->hasSkinning() && command.skinMatrices && !command.skinMatrices->empty())
            {
                int boneCount = std::min({static_cast<int>(command.skinMatrices->size()),
                                          static_cast<int>(command.mesh->getSkinJointNodes().size()),
                                          MAX_SKIN_BONES});
                command.material->shader->set("hasSkinning", 1);
                command.material->shader->set("boneCount", boneCount);
                if (boneCount > 0)
                {
                    command.material->shader->setMat4Array("uBones", command.skinMatrices->data(), boneCount);
                }
            }
            else
            {
                command.material->shader->set("hasSkinning", 0);
                command.material->shader->set("boneCount", 0);
            }
            command.mesh->draw();
        }

        // If there is a postprocess material, apply postprocessing
        if (postprocessMaterial)
        {
            // TODO: (Req 11) Return to the default framebuffer
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            // TODO: (Req 11) Setup the postprocess material and draw the fullscreen triangle
            this->postprocessMaterial->setup();
            this->postprocessMaterial->shader->set("time", elapsedTime);
            this->postprocessMaterial->shader->set("flashCenter", muzzleFlashCenter);
            this->postprocessMaterial->shader->set("flash", muzzleFlashStrength);
            glBindVertexArray(postProcessVertexArray);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }

        // Draw a centered crosshair (+) overlay on top of everything
        healthBar.render(windowSize);

        if (crosshairShader)
        {
            PipelineState crosshairPipelineState{};
            crosshairPipelineState.depthTesting.enabled = false;
            crosshairPipelineState.depthMask = false;
            crosshairPipelineState.blending.enabled = true;
            crosshairPipelineState.blending.equation = GL_FUNC_ADD;
            crosshairPipelineState.blending.sourceFactor = GL_SRC_ALPHA;
            crosshairPipelineState.blending.destinationFactor = GL_ONE_MINUS_SRC_ALPHA;
            crosshairPipelineState.setup();

            crosshairShader->use();
            crosshairShader->set("center", glm::vec2(0.5f, 0.5f));
            crosshairShader->set("halfLength", 0.014f);
            crosshairShader->set("halfThickness", 0.0018f);
            crosshairShader->set("color", glm::vec4(1.0f, 1.0f, 1.0f, 0.95f));

            glBindVertexArray(postProcessVertexArray);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
    }

}