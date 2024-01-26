#ifndef GRAPHICSUTILITIES_HPP
#define GRAPHICSUTILITIES_HPP

#include "staircase.hpp"
#include "ViewerContext.hpp"
#include <GLES2/gl2.h>

void clearCanvas(RGB color);

void drawSquare(GLuint shaderProgram, GLfloat x, GLfloat y, GLfloat size,
                GLfloat r, GLfloat g, GLfloat b, float aspectRatio);

void drawCircle(GLuint shaderProgram, GLfloat centerX, GLfloat centerY,
                GLfloat radius, RGB color, float aspectRatio);

void drawLine(GLuint shaderProgram, GLfloat x1, GLfloat y1, GLfloat x2,
              GLfloat y2, GLfloat thickness, RGB color);

void drawCheckerBoard(GLuint shaderProgram);
void drawCheckerBoard(GLuint shaderProgram, Graphic3d_Vec2i windowSize);

void drawLoadingScreen(GLuint shaderProgram, SpinnerParams &spinnerParams);

void setupWebGLContext(std::string const &canvasId);

void setupViewport(ViewerContext &context);
void compileShader(GLuint &shader, char const *source, GLenum type);

GLuint linkProgram(GLuint vertexShader, GLuint fragmentShader);

GLuint createShaderProgram(char const *vertexShaderSource,
                           char const *fragmentShaderSource);
#endif
