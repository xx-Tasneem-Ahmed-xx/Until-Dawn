#include "material.hpp"

#include "../asset-loader.hpp"
#include "deserialize-utils.hpp"

namespace our
{

    // This function should setup the pipeline state and set the shader to be used
    void Material::setup() const
    {
        // TODO: (Req 7) Write this function
        pipelineState.setup();
        shader->use();
    }

    // This function read the material data from a json object
    void Material::deserialize(const nlohmann::json &data)
    {
        if (!data.is_object())
            return;

        if (data.contains("pipelineState"))
        {
            pipelineState.deserialize(data["pipelineState"]);
        }
        shader = AssetLoader<ShaderProgram>::get(data["shader"].get<std::string>());
        transparent = data.value("transparent", false);
    }

    // This function should call the setup of its parent and
    // set the "tint" uniform to the value in the member variable tint
    void TintedMaterial::setup() const
    {
        // TODO: (Req 7) Write this function
        Material::setup();
        shader->set("tint", tint);
        shader->set("hasTexture", 0);
    }

    // This function read the material data from a json object
    void TintedMaterial::deserialize(const nlohmann::json &data)
    {
        Material::deserialize(data);
        if (!data.is_object())
            return;
        tint = data.value("tint", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    }

    // This function should call the setup of its parent and
    // set the "alphaThreshold" uniform to the value in the member variable alphaThreshold
    // Then it should bind the texture and sampler to a texture unit and send the unit number to the uniform variable "tex"
    void TexturedMaterial::setup() const
    {
        // TODO: (Req 7) Write this function
        TintedMaterial::setup();
        shader->set("alphaThreshold", alphaThreshold);

        glActiveTexture(GL_TEXTURE0);
        if (texture) texture->bind();
        else glBindTexture(GL_TEXTURE_2D, 0);
        if (sampler) sampler->bind(0);
        shader->set("tex", 0);
        shader->set("has_albedo_map", texture ? 1 : 0);
        shader->set("uv_scale", uvScale);
    }

    // This function read the material data from a json object
    void TexturedMaterial::deserialize(const nlohmann::json &data)
    {
        TintedMaterial::deserialize(data);
        if (!data.is_object())
            return;
        alphaThreshold = data.value("alphaThreshold", 0.0f);
        texture = AssetLoader<Texture2D>::get(data.value("texture", ""));
        sampler = AssetLoader<Sampler>::get(data.value("sampler", ""));
        uvScale = data.value("uv_scale", uvScale);
    }

    void LitMaterial::setup() const {
        TexturedMaterial::setup(); // Sets up albedo on texture unit 0

        // Bind additional maps
        glActiveTexture(GL_TEXTURE1);
        if (specular) specular->bind();
        if (sampler) sampler->bind(1);
        shader->set("tex_specular", 1);
        shader->set("has_specular_map", specular ? 1 : 0);

        glActiveTexture(GL_TEXTURE2);
        if (roughness) roughness->bind();
        if (sampler) sampler->bind(2);
        shader->set("tex_roughness", 2);
        shader->set("has_roughness_map", roughness ? 1 : 0);

        glActiveTexture(GL_TEXTURE3);
        if (ambient_occlusion) ambient_occlusion->bind();
        if (sampler) sampler->bind(3);
        shader->set("tex_ambient_occlusion", 3);
        shader->set("has_ambient_occlusion_map", ambient_occlusion ? 1 : 0);

        glActiveTexture(GL_TEXTURE4);
        if (emission) emission->bind();
        if (sampler) sampler->bind(4);
        shader->set("tex_emission", 4);
        shader->set("has_emission_map", emission ? 1 : 0);
        
        glActiveTexture(GL_TEXTURE0); // restore default
    }

    void LitMaterial::deserialize(const nlohmann::json& data){
        TexturedMaterial::deserialize(data);
        if(!data.is_object()) return;
        
        specular = AssetLoader<Texture2D>::get(data.value("specular", ""));
        roughness = AssetLoader<Texture2D>::get(data.value("roughness", ""));
        ambient_occlusion = AssetLoader<Texture2D>::get(data.value("ambient_occlusion", ""));
        emission = AssetLoader<Texture2D>::get(data.value("emission", ""));

        // If a shared sampler isn't defined explicitly for these, use the albedo sampler
        sampler = AssetLoader<Sampler>::get(data.value("sampler", ""));
    }

}