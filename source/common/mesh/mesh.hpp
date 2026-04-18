#pragma once

#include <glad/gl.h>
#include <cstddef>
#include <vector>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include "vertex.hpp"

namespace our
{

#define ATTRIB_LOC_POSITION 0
#define ATTRIB_LOC_COLOR 1
#define ATTRIB_LOC_TEXCOORD 2
#define ATTRIB_LOC_NORMAL 3
#define ATTRIB_LOC_JOINTS 4
#define ATTRIB_LOC_WEIGHTS 5

    class Mesh
    {
        // Here, we store the object names of the 3 main components of a mesh:
        // A vertex array object, A vertex buffer and an element buffer
        unsigned int VBO, EBO;
        unsigned int VAO;
        // We need to remember the number of elements that will be drawn by glDrawElements
        GLsizei elementCount;

        // Optional glTF PBR base color material data
        glm::vec4 gltfBaseColorFactor = glm::vec4(1.0f);
        bool gltfHasBaseColorTexture = false;
        GLuint gltfBaseColorTextureID = 0;

        // Optional skinning data
        bool hasSkinningData = false;
        std::vector<int> skinJointNodes;
        std::vector<glm::mat4> inverseBindMatrices;

    public:
        // The constructor takes two vectors:
        // - vertices which contain the vertex data.
        // - elements which contain the indices of the vertices out of which each triangles will be constructed.
        // The mesh class does not keep these data on the RAM. Instead, it should create
        // a vertex buffer to store the vertex data on the VRAM,
        // an element buffer to store the element data on the VRAM,
        // a vertex array object to define how to read the vertex & element buffer during rendering
        Mesh(const std::vector<Vertex> &vertices, const std::vector<unsigned int> &elements)
        {
            // TODO: (Req 2) Write this function
            //  remember to store the number of elements in "elementCount" since you will need it for drawing
            //  For the attribute locations, use the constants defined above: ATTRIB_LOC_POSITION, ATTRIB_LOC_COLOR, etc
            elementCount = static_cast<GLsizei>(elements.size());

            glGenVertexArrays(1, &VAO);
            glBindVertexArray(VAO);

            glGenBuffers(1, &VBO);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

            glGenBuffers(1, &EBO);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, elements.size() * sizeof(unsigned int), elements.data(), GL_STATIC_DRAW);

            glEnableVertexAttribArray(ATTRIB_LOC_POSITION);
            glVertexAttribPointer(ATTRIB_LOC_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, position));

            glEnableVertexAttribArray(ATTRIB_LOC_COLOR);
            glVertexAttribPointer(ATTRIB_LOC_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void *)offsetof(Vertex, color));

            glEnableVertexAttribArray(ATTRIB_LOC_TEXCOORD);
            glVertexAttribPointer(ATTRIB_LOC_TEXCOORD, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, tex_coord));

            glEnableVertexAttribArray(ATTRIB_LOC_NORMAL);
            glVertexAttribPointer(ATTRIB_LOC_NORMAL, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, normal));

            glEnableVertexAttribArray(ATTRIB_LOC_JOINTS);
            glVertexAttribIPointer(ATTRIB_LOC_JOINTS, 4, GL_UNSIGNED_INT, sizeof(Vertex), (void *)offsetof(Vertex, joints));

            glEnableVertexAttribArray(ATTRIB_LOC_WEIGHTS);
            glVertexAttribPointer(ATTRIB_LOC_WEIGHTS, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, weights));

            glBindVertexArray(0);
        }

        // this function should render the mesh
        void draw()
        {
            // TODO: (Req 2) Write this function
            glBindVertexArray(VAO);
            glDrawElements(GL_TRIANGLES, elementCount, GL_UNSIGNED_INT, nullptr);
            glBindVertexArray(0);
        }

        // this function should delete the vertex & element buffers and the vertex array object
        ~Mesh()
        {
            // TODO: (Req 2) Write this function
            glDeleteBuffers(1, &VBO);
            glDeleteBuffers(1, &EBO);
            glDeleteVertexArrays(1, &VAO);

            if (gltfHasBaseColorTexture && gltfBaseColorTextureID != 0)
            {
                glDeleteTextures(1, &gltfBaseColorTextureID);
                gltfBaseColorTextureID = 0;
                gltfHasBaseColorTexture = false;
            }
        }

        void setGLTFBaseColorFactor(const glm::vec4 &factor)
        {
            gltfBaseColorFactor = factor;
        }

        glm::vec4 getGLTFBaseColorFactor() const
        {
            return gltfBaseColorFactor;
        }

        void setGLTFBaseColorTexture(GLuint textureID)
        {
            gltfBaseColorTextureID = textureID;
            gltfHasBaseColorTexture = textureID != 0;
        }

        bool hasGLTFBaseColorTexture() const
        {
            return gltfHasBaseColorTexture;
        }

        GLuint getGLTFBaseColorTextureID() const
        {
            return gltfBaseColorTextureID;
        }

        void setSkinData(const std::vector<int> &jointNodes, const std::vector<glm::mat4> &inverseBind)
        {
            skinJointNodes = jointNodes;
            inverseBindMatrices = inverseBind;
            hasSkinningData = !skinJointNodes.empty() && skinJointNodes.size() == inverseBindMatrices.size();
        }

        bool hasSkinning() const
        {
            return hasSkinningData;
        }

        const std::vector<int> &getSkinJointNodes() const
        {
            return skinJointNodes;
        }

        const std::vector<glm::mat4> &getInverseBindMatrices() const
        {
            return inverseBindMatrices;
        }

        Mesh(Mesh const &) = delete;
        Mesh &operator=(Mesh const &) = delete;
    };

}