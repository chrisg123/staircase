#ifndef MAIN_HPP
#define MAIN_HPP
#include "staircase.hpp"
#include "AppContext.hpp"
#include <GLES2/gl2.h>
#include <emscripten.h>
#include <emscripten/html5.h>
bool arePthreadsEnabled() {
#ifdef __EMSCRIPTEN_PTHREADS__
  return true;
#else
  return false;
#endif
}
void main_loop(void *arg);
void createCanvas(std::string containerId, std::string canvasId);
void setupWebGLContext(std::string const &canvasId);
void initializeOcctComponents(AppContext &context);
void setupViewport(AppContext &context);
void compileShader(GLuint &shader, char const *source, GLenum type);
GLuint linkProgram(GLuint vertexShader, GLuint fragmentShader);
GLuint createShaderProgram(char const *vertexShaderSource,
                           char const *fragmentShaderSource);
void printLabels(TDF_Label const &label, int level = 0);
std::optional<Handle(TDocStd_Document)> readInto(std::function<Handle(TDocStd_Document)()> aNewDoc,
                                  std::istream &fromStream);
void readStepFile(AppContext &context, std::string stepFileStr,
                  std::function<void(std::optional<Handle(TDocStd_Document)>)> callback);
void renderStepFile(AppContext &context, Handle(TDocStd_Document) &aDoc);
void drawSquare(AppContext &context, GLfloat x, GLfloat y, GLfloat size,
                GLfloat r, GLfloat g, GLfloat b, float aspectRatio);
void draw(AppContext &context);
void drawCheckerBoard(AppContext &context);
void drawLoadingScreen(AppContext &context, SpinnerParams &spinnerParams);
void clearCanvas(RGB color);
EM_BOOL onMouseCallback(int eventType, EmscriptenMouseEvent const *mouseEvent,
                        void *userData);
void initializeUserInteractions(AppContext &context);
#endif
