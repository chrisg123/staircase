#ifndef GRAPHICSUTILITIES_HPP
#define GRAPHICSUTILITIES_HPP

#include "staircase.hpp"
#include "AppContext.hpp"
#include <GLES2/gl2.h>

void clearCanvas(RGB color);

void drawSquare(AppContext &context, GLfloat x, GLfloat y, GLfloat size,
                GLfloat r, GLfloat g, GLfloat b, float aspectRatio);

void drawCircle(AppContext &context, GLfloat centerX, GLfloat centerY,
                GLfloat radius, RGB color, float aspectRatio);

void drawLine(AppContext &context, GLfloat x1, GLfloat y1, GLfloat x2,
              GLfloat y2, GLfloat thickness, RGB color);

void drawCheckerBoard(AppContext &context);

void drawLoadingScreen(AppContext &context, SpinnerParams &spinnerParams);

void setupWebGLContext(std::string const &canvasId);

void setupViewport(AppContext &context);

void compileShader(GLuint &shader, char const *source, GLenum type);

GLuint linkProgram(GLuint vertexShader, GLuint fragmentShader);

GLuint createShaderProgram(char const *vertexShaderSource,
                           char const *fragmentShaderSource);
#endif
