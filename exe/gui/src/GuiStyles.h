#pragma once
#include <map>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>
#include "GuiBase.h"
#include "imgui.h"

enum GuiTextureID
{
    TV_container = 0,
    TV_container_table = 1,
    TV_globe = 2,
    TV_palette = 3,
    TV_table = 4,
    TV_unit_transparant = 5,
    TV_unit_white = 6,
    GV_save = 7,
    GV_copy = 8,
    GV_vcopy = 9,
    GV_group_by = 10,
    MV_toggle_layout_1 = 11,
    MV_toggle_layout_2 = 12,
    MV_toggle_layout_3 = 13,
    MV_toggle_palette = 14,
    MV_toggle_scalebar = 15,
    MV_zoom_active_layer = 16,
    MV_zoom_all_layers = 17,
    MV_zoom_selected = 18,
    MV_zoomin_button = 19,
    MV_zoomout_button = 20,
    MV_table_show_first_selected = 21,
    MV_table_select_row = 22,
    MV_select_all = 23,
    MV_select_none = 24,
    MV_show_selected_features = 25,
    MV_toggle_needle = 26,
    MV_select_object = 27,
    MV_select_rect = 28,
    MV_select_circle = 29,
    MV_select_poly = 30,
    MV_select_district = 31
};

class GuiIcon
{
public:
    GuiIcon(std::string relativeIconPath);
    ~GuiIcon();

    GuiIcon(GuiIcon&& src) noexcept
    {
        operator = (std::move(src));
    }

    void operator = (GuiIcon&& src) noexcept
    {
        std::swap(m_Image, src.m_Image);

        m_Width = src.m_Width;
        m_Height = src.m_Height;
    }

    GLuint GetImage();
    int GetWidth();
    int GetHeight();

private:
    // Simple helper function to load an image into a OpenGL texture with common settings
    bool LoadTextureFromFile(std::string iconPath);

    GLuint m_Image = 0;
    int m_Width    = 0;
    int m_Height   = 0;
};

void InitializeGuiTextures();
GuiIcon& GetIcon(GuiTextureID id);
void SetDmsWindowIcon(GLFWwindow* window);
void SetGuiFont(std::string font_filename);

