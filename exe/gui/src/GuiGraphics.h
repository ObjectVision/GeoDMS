#pragma once
#include <string>
#include <GL/glew.h>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

static void glfw_error_callback(int error, const char* description);
void SetErrorCallBackForGLFW();
int InitGLFW();
std::string SetGLFWWindowHints();