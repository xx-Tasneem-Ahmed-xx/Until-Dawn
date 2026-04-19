#pragma once

#include <imgui.h>
#include <imgui_impl/imgui_impl_opengl3.h>
#include <texture/texture2d.hpp>
#include <texture/texture-utils.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>

namespace our::ui
{
    inline const std::array<const char *, 5> &serifFontCandidates()
    {
        static const std::array<const char *, 5> candidates = {
            "/usr/share/fonts/truetype/dejavu/DejaVuSerif-Bold.ttf",
            "/usr/share/fonts/truetype/liberation2/LiberationSerif-Bold.ttf",
            "/usr/share/fonts/truetype/noto/NotoSerif-Bold.ttf",
            "/usr/share/fonts/truetype/freefont/FreeSerifBold.ttf",
            "assets/fonts/Cinzel-Bold.ttf"};
        return candidates;
    }

    inline void loadPreferredSerifFonts(ImFont *&titleFont, ImFont *&uiFont, float titleSize, float uiSize)
    {
        if (titleFont && uiFont)
            return;

        ImGuiIO &io = ImGui::GetIO();
        bool addedCustomFont = false;

        for (const char *path : serifFontCandidates())
        {
            if (!std::filesystem::exists(path))
                continue;

            titleFont = io.Fonts->AddFontFromFileTTF(path, titleSize);
            uiFont = io.Fonts->AddFontFromFileTTF(path, uiSize);
            addedCustomFont = (titleFont != nullptr) && (uiFont != nullptr);
            if (addedCustomFont)
                break;
        }

        if (addedCustomFont)
        {
            io.Fonts->Build();
            ImGui_ImplOpenGL3_DestroyFontsTexture();
            ImGui_ImplOpenGL3_CreateFontsTexture();
        }

        if (!titleFont)
            titleFont = io.FontDefault;
        if (!uiFont)
            uiFont = io.FontDefault;
    }

    inline void centerCurrentWindowText(const char *text, float minX = 8.0f)
    {
        ImVec2 textSize = ImGui::CalcTextSize(text);
        ImGui::SetCursorPosX(std::max(minX, (ImGui::GetWindowWidth() - textSize.x) * 0.5f));
    }

    namespace pause
    {
        constexpr float kDimmerAlpha = 0.45f;
        constexpr float kPanelMaxWidth = 700.0f;
        constexpr float kPanelWidthRatio = 0.86f;
        constexpr float kPanelCenterY = 0.53f;
        constexpr float kPanelBgAlpha = 0.75f;
        constexpr float kIconTileSize = 106.0f;

        struct Assets
        {
            our::Texture2D *musicIcon = nullptr;
            our::Texture2D *volumeIcon = nullptr;
            our::Texture2D *menuIcon = nullptr;
            our::Texture2D *continueIcon = nullptr;
            our::Texture2D *disabledOverlayIcon = nullptr;
            ImFont *titleFont = nullptr;
            ImFont *uiFont = nullptr;

            static ImTextureID asTextureId(our::Texture2D *texture)
            {
                return texture ? reinterpret_cast<ImTextureID>(static_cast<intptr_t>(texture->getOpenGLName())) : nullptr;
            }

            void loadDefaultThemeResources()
            {
                destroy();

                musicIcon = loadIcon("assets/icons/music-player.png");
                volumeIcon = loadIcon("assets/icons/volume.png");
                menuIcon = loadIcon("assets/icons/menu.png");
                continueIcon = loadIcon("assets/icons/continue.png");
                disabledOverlayIcon = loadIcon("assets/icons/nothing.png");

                loadPreferredSerifFonts(titleFont, uiFont, 46.0f, 30.0f);
            }

            void destroy()
            {
                delete musicIcon;
                delete volumeIcon;
                delete menuIcon;
                delete continueIcon;
                delete disabledOverlayIcon;
                musicIcon = nullptr;
                volumeIcon = nullptr;
                menuIcon = nullptr;
                continueIcon = nullptr;
                disabledOverlayIcon = nullptr;
                titleFont = nullptr;
                uiFont = nullptr;
            }

        private:
            static our::Texture2D *loadIcon(const std::string &path)
            {
                our::Texture2D *texture = our::texture_utils::loadImage(path, false);
                if (!texture)
                    std::cout << "[PauseUI] Failed to load icon: " << path << "\n";
                return texture;
            }
        };

        inline void drawBackdrop(float alpha = kDimmerAlpha)
        {
            const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
            ImDrawList *foreground = ImGui::GetForegroundDrawList();
            foreground->AddRectFilled(ImVec2(0.0f, 0.0f), displaySize, IM_COL32(0, 0, 0, static_cast<int>(alpha * 255.0f)));
        }

        inline void setupPanelWindow(
            float maxWidth = kPanelMaxWidth,
            float widthRatio = kPanelWidthRatio,
            float centerY = kPanelCenterY,
            float bgAlpha = kPanelBgAlpha)
        {
            const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
            const float panelWidth = std::min(maxWidth, displaySize.x * widthRatio);
            ImGui::SetNextWindowPos(ImVec2(displaySize.x * 0.5f, displaySize.y * centerY), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(panelWidth, 0.0f), ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(bgAlpha);
        }

        inline ImGuiWindowFlags panelWindowFlags()
        {
            return ImGuiWindowFlags_NoDecoration |
                   ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_AlwaysAutoResize;
        }

        inline void centerButtonInCurrentColumn(float buttonWidth = kIconTileSize)
        {
            const float currentX = ImGui::GetCursorPosX();
            const float columnWidth = ImGui::GetColumnWidth();
            const float centeredX = currentX + std::max(0.0f, (columnWidth - buttonWidth) * 0.5f);
            ImGui::SetCursorPosX(centeredX);
        }

        inline bool drawIconTileButton(
            const char *id,
            ImTextureID iconTexture,
            const char *label,
            bool enabledState,
            ImTextureID disabledOverlayTexture,
            bool flipIconVertically = false)
        {
            ImGui::PushID(id);
            const ImVec2 buttonSize(kIconTileSize, kIconTileSize);
            const ImVec2 iconPadding(16.0f, 16.0f);
            constexpr float labelTopGap = 9.0f;
            constexpr float labelBottomPadding = 10.0f;
            const ImVec2 topLeft = ImGui::GetCursorScreenPos();

            ImGui::InvisibleButton("IconButton", buttonSize);

            const bool hovered = ImGui::IsItemHovered();
            const bool clicked = ImGui::IsItemClicked();

            ImDrawList *drawList = ImGui::GetWindowDrawList();
            const ImVec2 bottomRight(topLeft.x + buttonSize.x, topLeft.y + buttonSize.y);
            const ImU32 bgColor = hovered ? IM_COL32(245, 245, 245, 255) : IM_COL32(230, 230, 230, 255);
            drawList->AddRectFilled(topLeft, bottomRight, bgColor, 10.0f);
            drawList->AddRect(topLeft, bottomRight, IM_COL32(28, 28, 28, 255), 10.0f, 0, 1.8f);

            if (iconTexture)
            {
                const ImVec2 iconMin(topLeft.x + iconPadding.x, topLeft.y + iconPadding.y);
                const ImVec2 iconMax(bottomRight.x - iconPadding.x, bottomRight.y - iconPadding.y);
                if (flipIconVertically)
                {
                    drawList->AddImage(iconTexture, iconMin, iconMax, ImVec2(0, 1), ImVec2(1, 0));
                }
                else
                {
                    drawList->AddImage(iconTexture, iconMin, iconMax);
                }
            }

            if (!enabledState && disabledOverlayTexture)
            {
                const ImVec2 offMin(topLeft.x + iconPadding.x * 0.65f, topLeft.y + iconPadding.y * 0.65f);
                const ImVec2 offMax(bottomRight.x - iconPadding.x * 0.65f, bottomRight.y - iconPadding.y * 0.65f);
                drawList->AddImage(disabledOverlayTexture, offMin, offMax, ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 50, 50, 235));
            }

            ImVec2 labelSize = ImGui::CalcTextSize(label);
            drawList->AddText(ImVec2(topLeft.x + (buttonSize.x - labelSize.x) * 0.5f, topLeft.y + buttonSize.y + labelTopGap), IM_COL32(240, 240, 240, 255), label);

            const float reservedHeight = labelTopGap + labelSize.y + labelBottomPadding;
            ImGui::Dummy(ImVec2(buttonSize.x, reservedHeight));
            ImGui::PopID();
            return clicked;
        }
    }
}
