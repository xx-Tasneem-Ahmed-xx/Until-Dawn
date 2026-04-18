#include "mesh-utils.hpp"

// We will use "Tiny OBJ Loader" to read and process '.obj" files
#define TINYOBJLOADER_IMPLEMENTATION
#include <tinyobj/tiny_obj_loader.h>

#include <iostream>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <cstring>

#include <stb/stb_image.h>

our::Mesh *our::mesh_utils::loadOBJ(const std::string &filename)
{

    // The data that we will use to initialize our mesh
    std::vector<our::Vertex> vertices;
    std::vector<GLuint> elements;

    // Since the OBJ can have duplicated vertices, we make them unique using this map
    // The key is the vertex, the value is its index in the vector "vertices".
    // That index will be used to populate the "elements" vector.
    std::unordered_map<our::Vertex, GLuint> vertex_map;

    // The data loaded by Tiny OBJ Loader
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename.c_str()))
    {
        std::cerr << "Failed to load obj file \"" << filename << "\" due to error: " << err << std::endl;
        return nullptr;
    }
    if (!warn.empty())
    {
        std::cout << "WARN while loading obj file \"" << filename << "\": " << warn << std::endl;
    }

    // An obj file can have multiple shapes where each shape can have its own material
    // Ideally, we would load each shape into a separate mesh or store the start and end of it in the element buffer to be able to draw each shape separately
    // But we ignored this fact since we don't plan to use multiple materials in the examples
    for (const auto &shape : shapes)
    {
        for (const auto &index : shape.mesh.indices)
        {
            Vertex vertex = {};

            // Read the data for a vertex from the "attrib" object
            vertex.position = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]};

            vertex.normal = {
                attrib.normals[3 * index.normal_index + 0],
                attrib.normals[3 * index.normal_index + 1],
                attrib.normals[3 * index.normal_index + 2]};

            vertex.tex_coord = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                attrib.texcoords[2 * index.texcoord_index + 1]};

            vertex.color = {
                attrib.colors[3 * index.vertex_index + 0] * 255,
                attrib.colors[3 * index.vertex_index + 1] * 255,
                attrib.colors[3 * index.vertex_index + 2] * 255,
                255};

            // See if we already stored a similar vertex
            auto it = vertex_map.find(vertex);
            if (it == vertex_map.end())
            {
                // if no, add it to the vertices and record its index
                auto new_vertex_index = static_cast<GLuint>(vertices.size());
                vertex_map[vertex] = new_vertex_index;
                elements.push_back(new_vertex_index);
                vertices.push_back(vertex);
            }
            else
            {
                // if yes, just add its index in the elements vector
                elements.push_back(it->second);
            }
        }
    }

    return new our::Mesh(vertices, elements);
}

// Load a GLB file using tinygltf
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h>

namespace
{
    GLuint uploadGLTFImageToTexture(const tinygltf::Image &image, const std::filesystem::path &gltfPath)
    {
        int width = image.width;
        int height = image.height;
        int channels = image.component;
        std::vector<unsigned char> decodedPixels;
        const unsigned char *pixelData = nullptr;

        // tinygltf usually decodes image bytes into image.image for both embedded and external textures.
        if (!image.image.empty())
        {
            pixelData = image.image.data();
        }
        else if (!image.uri.empty())
        {
            // Fallback path: manually load external images if they are not already decoded.
            auto texturePath = gltfPath.parent_path() / image.uri;
            stbi_set_flip_vertically_on_load(false);
            unsigned char *loaded = stbi_load(texturePath.string().c_str(), &width, &height, &channels, 0);
            if (!loaded)
            {
                std::cerr << "Failed to load external glTF image: " << texturePath << std::endl;
                return 0;
            }
            decodedPixels.assign(loaded, loaded + (width * height * channels));
            stbi_image_free(loaded);
            pixelData = decodedPixels.data();
        }
        else
        {
            return 0;
        }

        if (!pixelData || width <= 0 || height <= 0)
        {
            return 0;
        }

        GLenum format = GL_RGBA;
        GLenum internalFormat = GL_RGBA8;
        switch (channels)
        {
        case 1:
            format = GL_RED;
            internalFormat = GL_R8;
            break;
        case 2:
            format = GL_RG;
            internalFormat = GL_RG8;
            break;
        case 3:
            format = GL_RGB;
            internalFormat = GL_SRGB8;
            break;
        case 4:
            format = GL_RGBA;
            internalFormat = GL_SRGB8_ALPHA8;
            break;
        default:
            std::cerr << "Unsupported glTF image channel count: " << channels << std::endl;
            return 0;
        }

        GLuint texture = 0;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, pixelData);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);

        return texture;
    }
}

our::Mesh *our::mesh_utils::loadGLB(const std::string &filename)
{
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, filename);

    if (!warn.empty())
    {
        std::cout << "WARN while loading glb file \"" << filename << "\": " << warn << std::endl;
    }

    if (!ret)
    {
        std::cerr << "Failed to load glb file \"" << filename << "\" due to error: " << err << std::endl;
        return nullptr;
    }

    // If there are no meshes, return nullptr
    if (model.meshes.empty())
    {
        std::cerr << "No meshes found in glb file \"" << filename << "\"" << std::endl;
        return nullptr;
    }

    std::vector<our::Vertex> vertices;
    std::vector<GLuint> elements;
    glm::vec4 meshBaseColorFactor(1.0f);
    bool meshHasBaseColorTexture = false;
    GLuint meshBaseColorTextureID = 0;
    bool baseColorMaterialInitialized = false;
    std::filesystem::path gltfPath(filename);

    // Helper function to get vec3 data from an accessor
    auto getVec3Data = [&model](int accessorIndex) -> std::vector<glm::vec3>
    {
        std::vector<glm::vec3> result;

        if (accessorIndex < 0 || accessorIndex >= static_cast<int>(model.accessors.size()))
        {
            return result;
        }

        const auto &accessor = model.accessors[accessorIndex];
        const auto &bufferView = model.bufferViews[accessor.bufferView];
        const auto &buffer = model.buffers[bufferView.buffer];

        size_t byteStride = bufferView.byteStride == 0 ? sizeof(glm::vec3) : bufferView.byteStride;

        result.resize(accessor.count);

        for (size_t i = 0; i < accessor.count; ++i)
        {
            size_t offset = bufferView.byteOffset + accessor.byteOffset + i * byteStride;
            std::memcpy(&result[i], &buffer.data[offset], sizeof(glm::vec3));
        }

        return result;
    };

    // Helper function to get vec2 data from an accessor
    // Supports FLOAT and normalized UNSIGNED_BYTE/UNSIGNED_SHORT (common glTF texcoord formats)
    auto getVec2Data = [&model](int accessorIndex) -> std::vector<glm::vec2>
    {
        std::vector<glm::vec2> result;

        if (accessorIndex < 0 || accessorIndex >= static_cast<int>(model.accessors.size()))
        {
            return result;
        }

        const auto &accessor = model.accessors[accessorIndex];
        if (accessor.bufferView < 0 || accessor.bufferView >= static_cast<int>(model.bufferViews.size()))
        {
            return result;
        }
        const auto &bufferView = model.bufferViews[accessor.bufferView];
        if (bufferView.buffer < 0 || bufferView.buffer >= static_cast<int>(model.buffers.size()))
        {
            return result;
        }
        const auto &buffer = model.buffers[bufferView.buffer];

        size_t byteStride = bufferView.byteStride == 0 ? sizeof(glm::vec2) : bufferView.byteStride;

        result.resize(accessor.count);

        for (size_t i = 0; i < accessor.count; ++i)
        {
            size_t offset = bufferView.byteOffset + accessor.byteOffset + i * byteStride;

            if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
            {
                std::memcpy(&result[i], &buffer.data[offset], sizeof(glm::vec2));
            }
            else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
            {
                uint16_t uv[2] = {0, 0};
                std::memcpy(uv, &buffer.data[offset], sizeof(uv));
                if (accessor.normalized)
                {
                    result[i] = glm::vec2(uv[0] / 65535.0f, uv[1] / 65535.0f);
                }
                else
                {
                    result[i] = glm::vec2(static_cast<float>(uv[0]), static_cast<float>(uv[1]));
                }
            }
            else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
            {
                uint8_t uv[2] = {0, 0};
                std::memcpy(uv, &buffer.data[offset], sizeof(uv));
                if (accessor.normalized)
                {
                    result[i] = glm::vec2(uv[0] / 255.0f, uv[1] / 255.0f);
                }
                else
                {
                    result[i] = glm::vec2(static_cast<float>(uv[0]), static_cast<float>(uv[1]));
                }
            }
            else
            {
                result[i] = glm::vec2(0.0f);
            }
        }

        return result;
    };

    // Helper function to get vec4 data from an accessor (for colors)
    // Supports VEC3/VEC4 with FLOAT/UNSIGNED_BYTE/UNSIGNED_SHORT components.
    auto getVec4Data = [&model](int accessorIndex) -> std::vector<glm::vec4>
    {
        std::vector<glm::vec4> result;

        if (accessorIndex < 0 || accessorIndex >= static_cast<int>(model.accessors.size()))
        {
            return result;
        }

        const auto &accessor = model.accessors[accessorIndex];
        if (accessor.bufferView < 0 || accessor.bufferView >= static_cast<int>(model.bufferViews.size()))
        {
            return result;
        }
        const auto &bufferView = model.bufferViews[accessor.bufferView];
        if (bufferView.buffer < 0 || bufferView.buffer >= static_cast<int>(model.buffers.size()))
        {
            return result;
        }
        const auto &buffer = model.buffers[bufferView.buffer];

        int componentCount = 4;
        if (accessor.type == TINYGLTF_TYPE_VEC3)
            componentCount = 3;
        else if (accessor.type == TINYGLTF_TYPE_VEC4)
            componentCount = 4;

        size_t bytesPerComponent = 4;
        if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
            bytesPerComponent = 1;
        else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
            bytesPerComponent = 2;
        else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
            bytesPerComponent = 4;

        size_t packedSize = static_cast<size_t>(componentCount) * bytesPerComponent;
        size_t byteStride = bufferView.byteStride == 0 ? packedSize : bufferView.byteStride;

        result.resize(accessor.count);

        for (size_t i = 0; i < accessor.count; ++i)
        {
            size_t offset = bufferView.byteOffset + accessor.byteOffset + i * byteStride;
            result[i] = glm::vec4(1.0f);

            if (offset + packedSize > buffer.data.size())
            {
                continue;
            }

            if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
            {
                const uint8_t *src = reinterpret_cast<const uint8_t *>(&buffer.data[offset]);
                for (int c = 0; c < componentCount; ++c)
                {
                    float v = static_cast<float>(src[c]);
                    if (accessor.normalized)
                        v /= 255.0f;
                    if (c == 0)
                        result[i].r = v;
                    else if (c == 1)
                        result[i].g = v;
                    else if (c == 2)
                        result[i].b = v;
                    else
                        result[i].a = v;
                }
            }
            else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
            {
                const uint16_t *src = reinterpret_cast<const uint16_t *>(&buffer.data[offset]);
                for (int c = 0; c < componentCount; ++c)
                {
                    float v = static_cast<float>(src[c]);
                    if (accessor.normalized)
                        v /= 65535.0f;
                    if (c == 0)
                        result[i].r = v;
                    else if (c == 1)
                        result[i].g = v;
                    else if (c == 2)
                        result[i].b = v;
                    else
                        result[i].a = v;
                }
            }
            else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
            {
                const float *src = reinterpret_cast<const float *>(&buffer.data[offset]);
                for (int c = 0; c < componentCount; ++c)
                {
                    float v = src[c];
                    if (c == 0)
                        result[i].r = v;
                    else if (c == 1)
                        result[i].g = v;
                    else if (c == 2)
                        result[i].b = v;
                    else
                        result[i].a = v;
                }
            }

            if (componentCount == 3)
                result[i].a = 1.0f;
        }

        return result;
    };

    // Loop through all meshes
    for (const auto &mesh : model.meshes)
    {
        // Loop through all primitives in this mesh
        for (const auto &primitive : mesh.primitives)
        {
            // Get the vertex count before adding new vertices (used for index offset)
            GLuint vertexOffset = static_cast<GLuint>(vertices.size());

            // Resolve primitive material first so we can select the correct UV set for baseColorTexture
            int baseColorTexCoordSet = 0;
            glm::vec4 materialColor(1.0f, 1.0f, 1.0f, 1.0f);
            if (primitive.material >= 0 && primitive.material < static_cast<int>(model.materials.size()))
            {
                const auto &material = model.materials[primitive.material];
                if (material.pbrMetallicRoughness.baseColorFactor.size() >= 4)
                {
                    materialColor = glm::vec4(
                        material.pbrMetallicRoughness.baseColorFactor[0],
                        material.pbrMetallicRoughness.baseColorFactor[1],
                        material.pbrMetallicRoughness.baseColorFactor[2],
                        material.pbrMetallicRoughness.baseColorFactor[3]);
                }

                if (!baseColorMaterialInitialized)
                {
                    meshBaseColorFactor = materialColor;
                    baseColorMaterialInitialized = true;
                }

                const auto &baseColorTexture = material.pbrMetallicRoughness.baseColorTexture;
                if (baseColorTexture.texCoord >= 0)
                {
                    baseColorTexCoordSet = baseColorTexture.texCoord;
                }
                if (!meshHasBaseColorTexture && baseColorTexture.index >= 0 && baseColorTexture.index < static_cast<int>(model.textures.size()))
                {
                    const auto &gltfTexture = model.textures[baseColorTexture.index];
                    if (gltfTexture.source >= 0 && gltfTexture.source < static_cast<int>(model.images.size()))
                    {
                        meshBaseColorTextureID = uploadGLTFImageToTexture(model.images[gltfTexture.source], gltfPath);
                        meshHasBaseColorTexture = meshBaseColorTextureID != 0;
                    }
                }
            }

            // Get position data (VEC3, FLOAT)
            std::vector<glm::vec3> positions;
            auto positionIt = primitive.attributes.find("POSITION");
            if (positionIt != primitive.attributes.end())
            {
                positions = getVec3Data(positionIt->second);
            }

            // Skip this primitive if it has no positions
            if (positions.empty())
            {
                continue;
            }

            // Get normal data (VEC3, FLOAT)
            std::vector<glm::vec3> normals;
            auto normalIt = primitive.attributes.find("NORMAL");
            if (normalIt != primitive.attributes.end())
            {
                normals = getVec3Data(normalIt->second);
            }

            // Get texcoord data (VEC2) from selected UV set for baseColorTexture
            std::vector<glm::vec2> texcoords;
            std::string texcoordSemantic = "TEXCOORD_" + std::to_string(baseColorTexCoordSet);
            auto texcoordIt = primitive.attributes.find(texcoordSemantic);
            if (texcoordIt == primitive.attributes.end())
            {
                texcoordIt = primitive.attributes.find("TEXCOORD_0");
            }
            if (texcoordIt != primitive.attributes.end())
            {
                texcoords = getVec2Data(texcoordIt->second);
            }

            // Get color data (VEC4, UNSIGNED_BYTE or FLOAT)
            std::vector<glm::vec4> vertexColors;
            auto colorIt = primitive.attributes.find("COLOR_0");
            if (colorIt != primitive.attributes.end())
            {
                vertexColors = getVec4Data(colorIt->second);
            }

            // Create vertices from the data
            for (size_t i = 0; i < positions.size(); ++i)
            {
                Vertex vertex = {};
                vertex.position = positions[i];
                vertex.normal = (i < normals.size()) ? normals[i] : glm::vec3(0.0f, 0.0f, 1.0f);
                if (i < texcoords.size())
                {
                    // glTF UV origin is top-left while OpenGL expects bottom-left texture origin.
                    vertex.tex_coord = glm::vec2(texcoords[i].x, 1.0f - texcoords[i].y);
                }
                else
                {
                    vertex.tex_coord = glm::vec2(0.0f, 0.0f);
                }

                // Use vertex color if available, otherwise use material color
                if (i < vertexColors.size())
                {
                    vertex.color = our::Color(
                        static_cast<unsigned char>(vertexColors[i].r * 255.0f),
                        static_cast<unsigned char>(vertexColors[i].g * 255.0f),
                        static_cast<unsigned char>(vertexColors[i].b * 255.0f),
                        static_cast<unsigned char>(vertexColors[i].a * 255.0f));
                }
                else
                {
                    vertex.color = our::Color(
                        static_cast<unsigned char>(materialColor.r * 255.0f),
                        static_cast<unsigned char>(materialColor.g * 255.0f),
                        static_cast<unsigned char>(materialColor.b * 255.0f),
                        static_cast<unsigned char>(materialColor.a * 255.0f));
                }
                vertices.push_back(vertex);
            }

            // Get indices (SCALAR, UNSIGNED_INT or UNSIGNED_SHORT)
            if (primitive.indices >= 0 && primitive.indices < static_cast<int>(model.accessors.size()))
            {
                const auto &indexAccessor = model.accessors[primitive.indices];

                // Validate that bufferView is valid
                if (indexAccessor.bufferView < 0 || indexAccessor.bufferView >= static_cast<int>(model.bufferViews.size()))
                {
                    std::cerr << "Invalid bufferView index in index accessor" << std::endl;
                    continue;
                }

                const auto &bufferView = model.bufferViews[indexAccessor.bufferView];

                // Validate that buffer is valid
                if (bufferView.buffer < 0 || bufferView.buffer >= static_cast<int>(model.buffers.size()))
                {
                    std::cerr << "Invalid buffer index in bufferView" << std::endl;
                    continue;
                }

                const auto &buffer = model.buffers[bufferView.buffer];

                size_t byteStride = bufferView.byteStride == 0 ? (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT ? 4 : 2) : bufferView.byteStride;

                for (size_t i = 0; i < indexAccessor.count; ++i)
                {
                    size_t offset = bufferView.byteOffset + indexAccessor.byteOffset + i * byteStride;

                    // Validate offset is within buffer bounds
                    if (offset + (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT ? 4 : 2) > buffer.data.size())
                    {
                        std::cerr << "Index offset out of buffer bounds" << std::endl;
                        continue;
                    }

                    if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                    {
                        GLuint index;
                        std::memcpy(&index, &buffer.data[offset], sizeof(GLuint));
                        elements.push_back(index + vertexOffset);
                    }
                    else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                    {
                        GLushort index;
                        std::memcpy(&index, &buffer.data[offset], sizeof(GLushort));
                        elements.push_back(static_cast<GLuint>(index) + vertexOffset);
                    }
                }
            }
            else
            {
                // If there are no indices, create them as a simple sequence
                for (size_t i = 0; i < positions.size(); ++i)
                {
                    elements.push_back(static_cast<GLuint>(i) + vertexOffset);
                }
            }
        }
    }

    if (vertices.empty() || elements.empty())
    {
        std::cerr << "No vertex or index data found in glb file \"" << filename << "\"" << std::endl;
        return nullptr;
    }

    auto *mesh = new our::Mesh(vertices, elements);
    mesh->setGLTFBaseColorFactor(meshBaseColorFactor);
    if (meshHasBaseColorTexture)
    {
        mesh->setGLTFBaseColorTexture(meshBaseColorTextureID);
    }

    return mesh;
}

// Create a sphere (the vertex order in the triangles are CCW from the outside)
// Segments define the number of divisions on the both the latitude and the longitude
our::Mesh *our::mesh_utils::sphere(const glm::ivec2 &segments)
{
    std::vector<our::Vertex> vertices;
    std::vector<GLuint> elements;

    // We populate the sphere vertices by looping over its longitude and latitude
    for (int lat = 0; lat <= segments.y; lat++)
    {
        float v = (float)lat / segments.y;
        float pitch = v * glm::pi<float>() - glm::half_pi<float>();
        float cos = glm::cos(pitch), sin = glm::sin(pitch);
        for (int lng = 0; lng <= segments.x; lng++)
        {
            float u = (float)lng / segments.x;
            float yaw = u * glm::two_pi<float>();
            glm::vec3 normal = {cos * glm::cos(yaw), sin, cos * glm::sin(yaw)};
            glm::vec3 position = normal;
            glm::vec2 tex_coords = glm::vec2(u, v);
            our::Color color = our::Color(255, 255, 255, 255);
            vertices.push_back({position, color, tex_coords, normal});
        }
    }

    for (int lat = 1; lat <= segments.y; lat++)
    {
        int start = lat * (segments.x + 1);
        for (int lng = 1; lng <= segments.x; lng++)
        {
            int prev_lng = lng - 1;
            elements.push_back(lng + start);
            elements.push_back(lng + start - segments.x - 1);
            elements.push_back(prev_lng + start - segments.x - 1);
            elements.push_back(prev_lng + start - segments.x - 1);
            elements.push_back(prev_lng + start);
            elements.push_back(lng + start);
        }
    }

    return new our::Mesh(vertices, elements);
}