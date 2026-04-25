#pragma once

// // ============================================================
// //  CrosshairRenderer
// //
// //  Draws a simple screen-space crosshair at the position
// //  reported by PlayerControllerSystem::getCrosshairScreenPos().
// //
// //  Usage (inside your PlayState::onDraw or HUD pass):
// //
// //      crosshairRenderer.draw(screenX, screenY, windowW, windowH);
// //
// //  The renderer uses a tiny fullscreen-quad shader that draws
// //  four lines (top, bottom, left, right) with a gap in the
// //  middle, in white with a subtle black outline.
// //
// //  It owns its own VAO/VBO/shader so it does not interfere
// //  with the main rendering pipeline.  Call init() once after
// //  your GL context is created, and destroy() when done.
// // ============================================================

// #include <glad/gl.h>   // or your project's GL loader
// #include <glm/glm.hpp>
// #include <string>

// namespace our
// {

// class CrosshairRenderer
// {
// public:
//     // Must be called once with an active GL context.
//     void init();

//     // Draw the crosshair at (screenX, screenY) in window pixels.
//     // Typically called at the end of your frame, after the 3-D scene.
//     void draw(float screenX, float screenY, int windowW, int windowH);

//     // Free all GPU resources.
//     void destroy();

// private:
//     GLuint vao_     = 0;
//     GLuint vbo_     = 0;
//     GLuint shader_  = 0;

//     // Crosshair visual tunables
//     float lineLen_  = 10.0f;  // pixels from centre to tip of each arm
//     float gapSize_  =  4.0f;  // pixels of empty space around the centre
//     float thickness_ = 1.5f;  // line width in pixels

//     GLuint compileShader(GLenum type, const char *src);
//     GLuint linkProgram(GLuint vert, GLuint frag);
// };

// } // namespace our


// ──────────────────────────────────────────────────────────────────────────────
// Implementation  (keep in a .cpp in your project)
// ──────────────────────────────────────────────────────────────────────────────
//
 #include "crosshair-renderer.hpp"

 namespace our {

 // Vertex shader: receives NDC positions directly
 static const char *kVertSrc = R"glsl(
     #version 330 core
     layout(location = 0) in vec2 aPos;
     void main() { gl_Position = vec4(aPos, 0.0, 1.0); }
 )glsl";

 // Fragment shader: flat white
 static const char *kFragSrc = R"glsl(
     #version 330 core
     uniform vec4 uColor;
     out vec4 fragColor;
     void main() { fragColor = uColor; }
 )glsl";

 void CrosshairRenderer::init()
 {
     GLuint v = compileShader(GL_VERTEX_SHADER,   kVertSrc);
     GLuint f = compileShader(GL_FRAGMENT_SHADER, kFragSrc);
     shader_  = linkProgram(v, f);
     glDeleteShader(v); glDeleteShader(f);

     glGenVertexArrays(1, &vao_);
     glGenBuffers(1, &vbo_);

     glBindVertexArray(vao_);
     glBindBuffer(GL_ARRAY_BUFFER, vbo_);
     // 4 lines × 2 vertices × 2 floats = 16 floats (filled each frame)
     glBufferData(GL_ARRAY_BUFFER, 16 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
     glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
     glEnableVertexAttribArray(0);
     glBindVertexArray(0);
 }

 void CrosshairRenderer::draw(float sx, float sy, int ww, int wh)
 {
     // Convert pixel position → NDC
     auto toNDC = [&](float px, float py) -> glm::vec2 {
         return { 2.0f * px / ww - 1.0f,
                  1.0f - 2.0f * py / wh };  // screen Y flipped
     };

     float g = gapSize_;
     float L = g + lineLen_;

     glm::vec2 verts[8] = {
         toNDC(sx,  sy - L), toNDC(sx,  sy - g), // top arm
         toNDC(sx,  sy + g), toNDC(sx,  sy + L), // bottom arm
         toNDC(sx - L, sy), toNDC(sx - g, sy),   // left arm
         toNDC(sx + g, sy), toNDC(sx + L, sy),   // right arm
     };

     glBindBuffer(GL_ARRAY_BUFFER, vbo_);
     glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

     glUseProgram(shader_);
     // 1. Draw black outline (slightly wider, black)
     glUniform4f(glGetUniformLocation(shader_, "uColor"), 0, 0, 0, 0.8f);
     glLineWidth(thickness_ + 1.5f);
     glBindVertexArray(vao_);
     glDrawArrays(GL_LINES, 0, 8);
     // 2. Draw white inner line
     glUniform4f(glGetUniformLocation(shader_, "uColor"), 1, 1, 1, 1.0f);
     glLineWidth(thickness_);
     glDrawArrays(GL_LINES, 0, 8);
     glBindVertexArray(0);
     glLineWidth(1.0f);
 }

 void CrosshairRenderer::destroy()
 {
     glDeleteVertexArrays(1, &vao_);
     glDeleteBuffers(1,     &vbo_);
     glDeleteProgram(shader_);
 }

 GLuint CrosshairRenderer::compileShader(GLenum type, const char *src)
 {
     GLuint s = glCreateShader(type);
     glShaderSource(s, 1, &src, nullptr);
     glCompileShader(s);
     return s;
 }

 GLuint CrosshairRenderer::linkProgram(GLuint v, GLuint f)
 {
     GLuint p = glCreateProgram();
     glAttachShader(p, v); glAttachShader(p, f);
     glLinkProgram(p);
     return p;
 }

 } // namespace our