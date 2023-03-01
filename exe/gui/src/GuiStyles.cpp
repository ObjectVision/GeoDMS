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

bool GuiIcon::LoadTextureFromFile(std::string iconPath) // TODO: adaptive use of OpenGL 2/3 functions depending on platform, see https://github.com/ocornut/imgui/blob/master/backends/imgui_impl_opengl2.cpp#L238
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
    s_GuiTextureIcons.emplace_back("misc\\icons\\MV_toggle_needle.bmp");
    s_GuiTextureIcons.emplace_back("misc\\icons\\MV_select_object.bmp");
    s_GuiTextureIcons.emplace_back("misc\\icons\\MV_select_rect.bmp");
    s_GuiTextureIcons.emplace_back("misc\\icons\\MV_select_circle.bmp");
    s_GuiTextureIcons.emplace_back("misc\\icons\\MV_select_poly.bmp");
    s_GuiTextureIcons.emplace_back("misc\\icons\\MV_select_district.bmp");
    
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

FontSpecification CreateNotoSansMediumFontSpec()
{
    FontSpecification result_spec;
    result_spec.filename = "misc/fonts/NotoSans-Medium.ttf";
    result_spec.size = 17.0f;
    result_spec.y_offset = -2.0f;

    // https://fonts.googleapis.com/css2?family=Noto+Sans Latin, Greek and Cyrillic
    result_spec.ranges.push_back(0x20);
    result_spec.ranges.push_back(0xFFFF);
    result_spec.ranges.push_back(0);

    return result_spec;
}

FontSpecification CreateNotoSansMathFontSpec()
{
    FontSpecification result_spec;
    result_spec.filename = "misc/fonts/NotoSansMath-Regular.ttf";
    result_spec.size = 17.0f;
    result_spec.y_offset = -4.0f;

    // https://fonts.googleapis.com/css2?family=Noto+Sans+Math
    result_spec.ranges.push_back(0x20);
    result_spec.ranges.push_back(0xFFFF);
    result_spec.ranges.push_back(0);

    return result_spec;
}

FontSpecification CreateRemixIconsFontSpec()
{
    FontSpecification result_spec;
    result_spec.filename = "misc/fonts/remixicon.ttf";
    result_spec.size = 15.0f;
    result_spec.y_offset = 1.0f;

    result_spec.ranges.push_back(0xEA01);
    result_spec.ranges.push_back(0xf2DE);
    result_spec.ranges.push_back(0);

    return result_spec;
}

ImFont* SetGuiFont(FontBuilderRecipy& recipy)
{
    // Noto-Sans font lookup ranges: https://fonts.googleapis.com/css2?family=Noto+Sans+Math or https://notofonts.github.io/overview/
    
    ImFont* font_ptr = nullptr;
    ImGuiIO& io = ImGui::GetIO();
    auto exePath = GetExeFilePath();
    ImFontConfig config;

    for (auto& font_spec : recipy.recipy)
    {
        std::string fontFileName = exePath + font_spec.filename;
        config.GlyphOffset = ImVec2(0.0f, font_spec.y_offset);
        if (std::filesystem::exists(fontFileName))
        {
            font_ptr = io.Fonts->AddFontFromFileTTF(fontFileName.c_str(), font_spec.size, &config, font_spec.ranges.Data);
            // 17.0f
            //io.Fonts->AddFontFromFileTTF(fontFileName.c_str(), 17.0f);
        }
        config.MergeMode = true; // in case the recipy contains multiple fonts
    }
    io.Fonts->Build();



    

    
    //std::string fontFileName = exePath + font_filenames.at(0); //"misc/fonts/DroidSans.ttf";
    
    //std::string iconFontFileName = exePath + "misc/fonts/remixicon.ttf";
    
    //config.GlyphOffset = ImVec2(0.0f, font_y_offset);//-2.0f);
    //ImWchar ranges_text_font[] = { 0x20, 0xFFFF, 0 }; // TODO: set range dynamically using range builder?
    /*if (std::filesystem::exists(fontFileName))
    {
        font_ptr = io.Fonts->AddFontFromFileTTF(fontFileName.c_str(), font_size, &config, ranges_text_font);
        io.Fonts->Build();
        // 17.0f
        //io.Fonts->AddFontFromFileTTF(fontFileName.c_str(), 17.0f);
    }*/

    return font_ptr;
    /*config.MergeMode = true;
    config.GlyphOffset = ImVec2(0.0f,1.0f);
    ImWchar ranges_icon_font[] = { 0xEA01, 0xf2DE, 0 };
    io.Fonts->AddFontFromFileTTF(iconFontFileName.c_str(), 15.0f, &config, ranges_icon_font);
    io.Fonts->Build();*/
}