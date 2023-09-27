#include "EmbeddedStepFile.h"
#include <GLES2/gl2.h>
#include <algorithm>
#include <any>
#include <chrono>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <iostream>
#include <mutex>
#include <opencascade/STEPCAFControl_Reader.hxx>
#include <opencascade/Standard_Version.hxx>
#include <opencascade/TDF_ChildIterator.hxx>
#include <opencascade/TDataStd_Name.hxx>
#include <opencascade/TDocStd_Document.hxx>
#include <opencascade/XCAFApp_Application.hxx>
#include <optional>
#include <queue>
#include <thread>

using DocHandle = Handle(TDocStd_Document);
using CallbackType = std::function<void(std::optional<DocHandle>)>;

namespace MessageType {
enum Type { SetVersionString, DrawCheckerboard, ReadStepFile, RenderStepFile };

static char const *toString(Type type) {
  switch (type) {
  case SetVersionString: return "SetVersionString";
  case DrawCheckerboard: return "DrawCheckerboard";
  case ReadStepFile: return "ReadStepFile";
  case RenderStepFile: return "RenderStepFile";
  default: return "Unknown";
  }
}
} // namespace MessageType

struct RGB {
  float r, g, b;
};

namespace Colors {
// clang-format off
const RGB Red      = {1.0f, 0.0f, 0.0f};
const RGB Green    = {0.0f, 1.0f, 0.0f};
const RGB Blue     = {0.0f, 0.0f, 1.0f};
const RGB White    = {1.0f, 1.0f, 1.0f};
const RGB Black    = {0.0f, 0.0f, 0.0f};
const RGB Grey     = {0.5f, 0.5f, 0.5f};
const RGB Platinum = {0.9f, 0.9f, 0.9f};
// clang-format on
} // namespace Colors

struct Message {
  MessageType::Type type;
  std::any data;
};

class AppContext {
public:
  AppContext() { app = XCAFApp_Application::GetApplication(); }

  Handle(XCAFApp_Application) getApp() const { return app; }

  void pushMessage(Message const &msg) {
    std::lock_guard<std::mutex> lock(queueMutex);
    messageQueue.push(msg);
  }

  std::queue<Message> drainMessageQueue() {
    std::lock_guard<std::mutex> lock(queueMutex);
    std::queue<Message> localQueue;
    std::swap(localQueue, messageQueue);
    return localQueue;
  }
  bool shouldDrawCheckerboard = false;
  GLuint shaderProgram;
  GLint canvasWidth = 0;
  GLint canvasHeight = 0;

private:
  Handle(XCAFApp_Application) app;
  std::queue<Message> messageQueue;
  std::mutex queueMutex;
};
class Timer {
public:
  Timer(std::string const &timerName)
      : name(timerName), start(std::chrono::high_resolution_clock::now()) {}

  ~Timer() {
    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();
    double seconds = static_cast<double>(duration) / 1e6;
    std::cout << "[TIMER] " << seconds << "s:" << name << std::endl;
  }

private:
  std::string name;
  std::chrono::time_point<std::chrono::high_resolution_clock> start;
};

bool arePthreadsEnabled();
void createCanvas(std::string containerId, std::string canvasId);
void setupWebGLContext(std::string const &canvasId);
void setupViewport(AppContext &context);
void compileShader(GLuint &shader, char const *source, GLenum type);
GLuint linkProgram(GLuint vertexShader, GLuint fragmentShader);
GLuint createShaderProgram(char const *vertexShaderSource,
                           char const *fragmentShaderSource);
void printLabels(TDF_Label const &label, int level = 0);
std::optional<DocHandle> readInto(std::function<DocHandle()> aNewDoc,
                                  std::istream &fromStream);
void readStepFile(AppContext &context, std::string stepFileStr,
                  CallbackType callback);
void renderStepFile(AppContext &context, DocHandle &aDoc);
void drawSquare(AppContext &context, GLfloat x, GLfloat y, GLfloat size,
                GLfloat r, GLfloat g, GLfloat b, float aspectRatio);
void draw(AppContext &context);
void drawCheckerBoard(AppContext &context);
void clearCanvas(RGB color);

void main_loop(void *arg) {
  AppContext *context = static_cast<AppContext *>(arg);
  auto localQueue = context->drainMessageQueue();

  while (!localQueue.empty()) {
    auto message = localQueue.front();
    localQueue.pop();
    std::cout << "[RCV] " << MessageType::toString(message.type) << std::endl;

    switch (message.type) {
    case MessageType::SetVersionString:
      EM_ASM(
          { document.getElementById('version').innerHTML = UTF8ToString($0); },
          std::any_cast<std::string>(message.data).c_str());
      break;
    case MessageType::DrawCheckerboard:
      context->shouldDrawCheckerboard = true;
      break;
    case MessageType::ReadStepFile: {
      std::string stepFileStr =
          std::any_cast<std::string>(message.data).c_str();

      EM_ASM(
          { document.getElementById('stepText').innerHTML = UTF8ToString($0); },
          stepFileStr.c_str());

      std::thread([&, context]() {
        readStepFile(
            *context, stepFileStr, [&](std::optional<DocHandle> docOpt) {
              if (!docOpt.has_value()) {
                std::cerr << "Failed to read STEP file: DocHandle is empty"
                          << std::endl;
                return;
              }
              Message msg;
              msg.type = MessageType::RenderStepFile;
              msg.data = docOpt.value();
              context->pushMessage(msg);
            });
      }).detach();
      break;
    }
    case MessageType::RenderStepFile:
      DocHandle aDoc = std::any_cast<DocHandle>(message.data);

      renderStepFile(*context, aDoc);

      break;
    }
  }

  draw(*context);
}

int main() {
  AppContext context;
  std::string occt_ver_str =
      "OCCT Version: " + std::string(OCC_VERSION_COMPLETE);
  std::cout << occt_ver_str << std::endl;

  if (!arePthreadsEnabled()) {
    std::cerr << "Pthreads are required." << std::endl;
    return 1;
  }
  std::cout << "Pthreads enabled" << std::endl;

  std::string containerId = "staircase-container";
  std::string canvasId = "staircase-canvas";

  createCanvas(containerId, canvasId);
  setupWebGLContext(canvasId);
  setupViewport(context);

  std::cout << "Canvas size: " << context.canvasWidth << "x"
            << context.canvasHeight << std::endl;

  context.shaderProgram = createShaderProgram(
      /*vertexShaderSource=*/
      "attribute vec3 position;"
      "void main() {"
      "  gl_Position  = vec4(position, 1.0);"
      "}",
      /*fragmentShaderSource=*/
      "precision mediump float;"
      "uniform vec4 color;"
      "void main() {"
      "  gl_FragColor = color;"
      "}");

  context.pushMessage({MessageType::SetVersionString, std::any{occt_ver_str}});
  context.pushMessage({MessageType::DrawCheckerboard, std::any{}});
  context.pushMessage({MessageType::ReadStepFile, std::any{embeddedStepFile}});

  emscripten_set_main_loop_arg(main_loop, &context, 0, 1);

  return 0;
}

void clearCanvas(RGB color) {
  glClearColor(color.r, color.g, color.b, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void createCanvas(std::string containerId, std::string canvasId) {
  EM_ASM(
      {
        var divElement = document.getElementById(UTF8ToString($0));

        if (divElement) {
          var canvas = document.createElement('canvas');
          canvas.id = UTF8ToString($1);
          divElement.appendChild(canvas);

          // Align canvas size with device pixels
          var computedStyle = window.getComputedStyle(canvas);
          var cssWidth = parseInt(computedStyle.getPropertyValue('width'), 10);
          var cssHeight =
              parseInt(computedStyle.getPropertyValue('height'), 10);
          var devicePixelRatio = window.devicePixelRatio || 1;
          canvas.width = cssWidth * devicePixelRatio;
          canvas.height = cssHeight * devicePixelRatio;
        }
      },
      containerId.c_str(), canvasId.c_str());
}

void setupWebGLContext(std::string const &canvasId) {
  EmscriptenWebGLContextAttributes attrs;
  emscripten_webgl_init_context_attributes(&attrs);

  attrs.alpha = 0;
  attrs.depth = 0;
  attrs.stencil = 0;
  attrs.antialias = 0;
  attrs.preserveDrawingBuffer = 0;
  attrs.failIfMajorPerformanceCaveat = 0;
  attrs.enableExtensionsByDefault = 1;
  attrs.premultipliedAlpha = 0;
  attrs.majorVersion = 1;
  attrs.minorVersion = 0;

  EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx =
      emscripten_webgl_create_context(("#" + canvasId).c_str(), &attrs);
  emscripten_webgl_make_context_current(ctx);
}

void setupViewport(AppContext &context) {
  GLint viewport[4];
  glGetIntegerv(GL_VIEWPORT, viewport);
  context.canvasWidth = viewport[2];
  context.canvasHeight = viewport[3];
}

void compileShader(GLuint &shader, char const *source, GLenum type) {
  shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, NULL);
  glCompileShader(shader);

  GLint success;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    GLchar infoLog[512];
    std::cerr << "Error compiling shader: " << infoLog << std::endl;
  }
}

GLuint linkProgram(GLuint vertexShader, GLuint fragmentShader) {
  GLuint shaderProgram = glCreateProgram();
  glAttachShader(shaderProgram, vertexShader);
  glAttachShader(shaderProgram, fragmentShader);
  glLinkProgram(shaderProgram);
  glUseProgram(shaderProgram);

  GLint success;
  glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
  if (!success) {
    GLchar infoLog[512];
    glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
    std::cerr << "Error linking shader program: " << infoLog << std::endl;
  }

  return shaderProgram;
}

GLuint createShaderProgram(char const *vertexShaderSource,
                           char const *fragmentShaderSource) {
  GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
  compileShader(vertexShader, vertexShaderSource, GL_VERTEX_SHADER);

  GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  compileShader(fragmentShader, fragmentShaderSource, GL_FRAGMENT_SHADER);

  return linkProgram(vertexShader, fragmentShader);
}

std::optional<DocHandle> readInto(std::function<DocHandle()> aNewDoc,
                                  std::istream &fromStream) {

  DocHandle aDoc = aNewDoc();
  STEPCAFControl_Reader aStepReader;

  IFSelect_ReturnStatus aStatus =
      aStepReader.ReadStream("Embedded STEP Data", fromStream);

  if (aStatus != IFSelect_RetDone) {
    std::cerr << "Error reading STEP file." << std::endl;
    return std::nullopt;
  }

  bool success = aStepReader.Transfer(aDoc);

  if (!success) {
    std::cerr << "Transfer failed." << std::endl;
    return std::nullopt;
  }

  return aDoc;
}

bool arePthreadsEnabled() {
#ifdef __EMSCRIPTEN_PTHREADS__
  return true;
#else
  return false;
#endif
}

/**
 * Recursively prints the hierarchy of labels from a TDF_Label tree.
 *
 * @param label The root label to start the traversal from.
 * @param level Indentation level for better readability (default is 0).
 */
void printLabels(TDF_Label const &label, int level) {
  for (int i = 0; i < level; ++i) {
    std::cout << "  ";
  }

  Handle(TDataStd_Name) name;
  if (label.FindAttribute(TDataStd_Name::GetID(), name)) {
    TCollection_ExtendedString nameStr = name->Get();
    std::cout << "Label: " << nameStr;
  } else {
    std::cout << "Unnamed Label";
  }
  std::cout << std::endl;

  for (TDF_ChildIterator it(label); it.More(); it.Next()) {
    printLabels(it.Value(), level + 1);
  }
}

void readStepFile(AppContext &context, std::string stepFileStr,
                  CallbackType callback) {

  auto aNewDoc = [&]() -> DocHandle {
    Handle(TDocStd_Document) aDoc;
    context.getApp()->NewDocument("MDTV-XCAF", aDoc);
    return aDoc;
  };

  std::istringstream fromStream(stepFileStr);

  std::optional<DocHandle> docOpt;
  {
    Timer timer = Timer("readInto(aNewDoc, fromStream)");
    docOpt = readInto(aNewDoc, fromStream);
  }

  callback(docOpt);
}

void renderStepFile(AppContext &context, DocHandle &aDoc) {
  if (aDoc.IsNull()) {
    std::cerr << "aDoc cannot be null" << std::endl;
    return;
  }
  printLabels(aDoc->Main());
}

void drawSquare(AppContext &context, GLfloat x, GLfloat y, GLfloat size,
                GLfloat r, GLfloat g, GLfloat b, float aspectRatio) {
  GLfloat adjY = y * aspectRatio;
  GLfloat adjSize = size * aspectRatio;

  GLfloat vertices[] = {x,    adjY, 0.0f,           x + size,
                        adjY, 0.0f, x + size,       adjY + adjSize,
                        0.0f, x,    adjY + adjSize, 0.0f};

  GLint colorUniform = glGetUniformLocation(context.shaderProgram, "color");
  GLint posAttrib = glGetAttribLocation(context.shaderProgram, "position");

  glUniform4f(colorUniform, r, g, b, 1.0f);

  GLuint vertexBuffer;
  glGenBuffers(1, &vertexBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE, 0, 0);
  glEnableVertexAttribArray(posAttrib);
  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
  glDeleteBuffers(1, &vertexBuffer);
}

void drawCheckerBoard(AppContext &context) {
  float squareSizePixels = context.canvasWidth * 0.05f;

  int rows = static_cast<int>(context.canvasHeight / squareSizePixels);
  int cols = static_cast<int>(context.canvasWidth / squareSizePixels);

  if (squareSizePixels * rows < context.canvasHeight) { rows += 1; }
  if (squareSizePixels * cols < context.canvasWidth) { cols += 1; }

  float aspectRatio = static_cast<float>(context.canvasWidth) /
                      static_cast<float>(context.canvasHeight);

  float const coordMin = -1.0f;
  float const coordMax = 1.0f;
  float const coordRange = coordMax - coordMin;

  float squareSize = coordRange / std::max(rows, cols);

  for (int i = 0; i < cols; ++i) {
    for (int j = 0; j < rows; ++j) {
      RGB color = (i + j) % 2 == 0 ? Colors::Green: Colors::Blue;
      float x = coordMin + (i * squareSize);
      float y = (coordMin / aspectRatio) + (j * squareSize);

      drawSquare(context, x, y, squareSize, color.r, color.g, color.b, aspectRatio);
    }
  }
}

void draw(AppContext &context) {
  GLenum err = glGetError();
  if (err != GL_NO_ERROR) {
    std::cerr << "OpenGL error: " << err << std::endl;
    return;
  }

  if (context.shouldDrawCheckerboard) { drawCheckerBoard(context); }
}
