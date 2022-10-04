#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers
//#include <GL/glext.h>
#include "imgui_impl_opengl3.h"

#include "GuiBase.h"
#include "imgui.h"

#include "GuiStyles.h"
#include "utl/Environment.h"
#include "filesystem"

GuiIcon::GuiIcon(std::string relativeIconPath)
{
    std::string exePath = GetExeFilePath();
    LoadTextureFromFile((exePath +relativeIconPath).c_str());
}

GuiIcon::~GuiIcon()
{
    if (m_Image)
        glDeleteTextures(1, &m_Image);
}

GLuint GuiIcon::GetImage()
{
    return m_Image;
}

int GuiIcon::GetWidth()
{
    return m_Width;
}

int GuiIcon::GetHeight()
{
    return m_Height;
}

bool GuiIcon::LoadTextureFromFile(std::string iconPath)
{
    // Load from file
    unsigned char* image_data;
    image_data = stbi_load(iconPath.c_str(), &m_Width, &m_Height, NULL, 4);
    if (image_data == NULL)
        return false;

    IM_ASSERT(image_data);

    glCreateTextures(GL_TEXTURE_2D, 1, &m_Image); // glGenTextures and glBindTexture
    glTextureParameteri(m_Image, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(m_Image, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureStorage2D(m_Image, 1, GL_RGBA8, m_Width, m_Height);
    glTextureSubImage2D(m_Image, 0, 0, 0, m_Width, m_Height, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
    stbi_image_free(image_data);

    return true;
}

static std::vector<GuiIcon> s_GuiTextureIcons;

void InitializeGuiTextures()
{
    // order is important, GuiTextureID enum act as index to singleton
    s_GuiTextureIcons.emplace_back("misc\\icons\\TV_container.bmp");        // TV_container
    s_GuiTextureIcons.emplace_back("misc\\icons\\TV_container_table.bmp");  // TV_container_table
    s_GuiTextureIcons.emplace_back("misc\\icons\\TV_globe.bmp");            // TV_globe
    s_GuiTextureIcons.emplace_back("misc\\icons\\TV_palette.bmp");          // TV_palette
    s_GuiTextureIcons.emplace_back("misc\\icons\\TV_table.bmp");            // TV_table
    s_GuiTextureIcons.emplace_back("misc\\icons\\TV_unit_transparant.bmp"); // TV_unit_transparant
    s_GuiTextureIcons.emplace_back("misc\\icons\\TV_unit_white.bmp");       // TV_unit_white
    s_GuiTextureIcons.emplace_back("misc\\icons\\GV_save.bmp");             // tableview save icon
    s_GuiTextureIcons.emplace_back("misc\\icons\\GV_copy.bmp");             // tableview copy icon
    s_GuiTextureIcons.emplace_back("misc\\icons\\GV_vcopy.bmp");            // tableview copy visible icon
    s_GuiTextureIcons.emplace_back("misc\\icons\\GV_group_by.bmp");          // tableview group by icon 
    s_GuiTextureIcons.emplace_back("misc\\icons\\MV_toggle_layout_1.bmp");          
    s_GuiTextureIcons.emplace_back("misc\\icons\\MV_toggle_layout_2.bmp");         
    s_GuiTextureIcons.emplace_back("misc\\icons\\MV_toggle_layout_3.bmp");          
    s_GuiTextureIcons.emplace_back("misc\\icons\\MV_toggle_palette.bmp");         
    s_GuiTextureIcons.emplace_back("misc\\icons\\MV_toggle_scalebar.bmp");
    s_GuiTextureIcons.emplace_back("misc\\icons\\MV_zoom_active_layer.bmp");       
    s_GuiTextureIcons.emplace_back("misc\\icons\\MV_zoom_all_layers.bmp");           
    s_GuiTextureIcons.emplace_back("misc\\icons\\MV_zoom_selected.bmp");       
    s_GuiTextureIcons.emplace_back("misc\\icons\\MV_zoomin_button.bmp");         
    s_GuiTextureIcons.emplace_back("misc\\icons\\MV_zoomout_button.bmp");
    s_GuiTextureIcons.emplace_back("misc\\icons\\MV_table_show_first_selected.bmp");
    s_GuiTextureIcons.emplace_back("misc\\icons\\MV_table_select_row.bmp");
    s_GuiTextureIcons.emplace_back("misc\\icons\\MV_select_all.bmp");
    s_GuiTextureIcons.emplace_back("misc\\icons\\MV_select_none.bmp");
    s_GuiTextureIcons.emplace_back("misc\\icons\\MV_show_selected_features.bmp");
}

GuiIcon& GetIcon(GuiTextureID id)
{
    return s_GuiTextureIcons.at(id);
}

void SetDmsWindowIcon(GLFWwindow* window)
{
    //auto exePath = GetExeFilePath();
    std::string exePath = GetExeFilePath();
    std::string iconFileName = exePath + "misc/icons/GeoDmsGui-0.png";
    if (std::filesystem::exists(iconFileName))
    {
        GLFWimage images[1];
        images[0].pixels = stbi_load(iconFileName.c_str(), &images[0].width, &images[0].height, 0, 4); //rgba channels 
        glfwSetWindowIcon(window, 1, images);
        stbi_image_free(images[0].pixels);
    }
}

void SetGuiFont(std::string font_filename)
{
    ImGuiIO& io = ImGui::GetIO();
    auto exePath = GetExeFilePath();
    //std::string fontFileName = exePath + "misc/fonts/Cousine-Regular.ttf";
    std::string fontFileName = exePath + font_filename; //"misc/fonts/DroidSans.ttf";
    std::string iconFontFileName = exePath + "misc/fonts/remixicon.ttf";
    if (std::filesystem::exists(fontFileName))
        auto test = io.Fonts->AddFontFromFileTTF(fontFileName.c_str(), 15.0f);

    static ImWchar ranges[] = { 0xEA01, 0xf2DE, 0 };
    ImFontConfig config;
    config.MergeMode = true;

    if (std::filesystem::exists(iconFontFileName))
    {
        auto test = io.Fonts->AddFontFromFileTTF(iconFontFileName.c_str(), 15.0f, &config, ranges);
        int i = 0;
    }
    ImGui_ImplOpenGL3_CreateFontsTexture();
    io.Fonts->Build();
}