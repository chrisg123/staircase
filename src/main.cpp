#include "EmbeddedStepFile.h"
#include <chrono>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <iostream>
#include <opencascade/STEPCAFControl_Reader.hxx>
#include <opencascade/Standard_Version.hxx>
#include <opencascade/TDF_ChildIterator.hxx>
#include <opencascade/TDataStd_Name.hxx>
#include <opencascade/TDocStd_Document.hxx>
#include <opencascade/XCAFApp_Application.hxx>
#include <optional>
#include <thread>
#include <queue>
#include <mutex>
#include <any>
#include <algorithm>
#include <GLES2/gl2.h>

using DocHandle = Handle(TDocStd_Document);
using CallbackType = std::function<void(std::optional<DocHandle>)>;

enum class MessageType {
  DrawCheckerboard,
  ReadDemoStepFile,
  ReadStepFileDone
};

struct Message {
  MessageType type;
  std::any data;
};

class AppContext {
public:
  AppContext() {
    app = XCAFApp_Application::GetApplication();
  }

  Handle(XCAFApp_Application) getApp() const {
    return app;
  }

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
void printLabels(TDF_Label const &label, int level = 0);
std::optional<DocHandle> readInto(std::function<DocHandle()> aNewDoc,
                   std::istream &fromStream);

void readDemoSTEPFile(AppContext &context, CallbackType callback);
void readStepFileDone();

void drawSquare(AppContext &context, GLfloat x, GLfloat y, GLfloat size,
                GLfloat r, GLfloat g, GLfloat b, float aspectRatio) {
  GLfloat adjY = y * aspectRatio;
  GLfloat adjSize = size * aspectRatio;

  GLfloat vertices[] = {x, adjY, 0.0f, x + size, adjY, 0.0f,
                        x + size, adjY + adjSize, 0.0f, x, adjY + adjSize, 0.0f};


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

void draw(AppContext &context) {
  GLenum err = glGetError();
  if (err != GL_NO_ERROR) {
    std::cerr << "OpenGL error: " << err << std::endl;
    return;
  }

  if (context.shouldDrawCheckerboard) {
    int rows = 14;
    int cols = 16;

    float aspectRatio = static_cast<float>(context.canvasWidth) / static_cast<float>(context.canvasHeight);

    float squareSize = 2.0f / std::max(rows, cols);

    for (int i = 0; i < cols; ++i) {
      for (int j = 0; j < rows; ++j) {
        float r, g, b;
        if ((i + j) % 2 == 0) {
          r = 0.0f; g = 1.0f; b = 0.0f;  // Green
        } else {
          r = 0.0f; g = 0.0f; b = 1.0f;  // Blue
        }

        float x = -1.0f + i * squareSize;
        float y = -1.0f + j * squareSize;

        drawSquare(context, x, y, squareSize, r, g, b, aspectRatio);
      }
    }

  }
}

void main_loop(void *arg) {
  AppContext *context = static_cast<AppContext*>(arg);
  auto localQueue = context->drainMessageQueue();

  while (!localQueue.empty()) {
    auto message = localQueue.front();
    localQueue.pop();

    switch (message.type) {
    case MessageType::DrawCheckerboard:
      std::cout << "[REC MSG] MessageType::DrawCheckerboard" << std::endl;
      context->shouldDrawCheckerboard = true;
      break;
    case MessageType::ReadDemoStepFile:
      std::cout << "[REC MSG] MessageType::ReadDemoStepFile" << std::endl;
      std::thread([&, context]() {
        readDemoSTEPFile(*context, [&](std::optional<DocHandle> docOpt) {
          Message msg;
          msg.type = MessageType::ReadStepFileDone;
          msg.data = docOpt.value();
          context->pushMessage(msg);
        });

      }).detach();
      break;
    case MessageType::ReadStepFileDone:
      std::cout << "[REC MSG] MessageType::ReadStepFileDone" << std::endl;
      readStepFileDone();
      break;
    }
  }

  draw(*context);
}

int main() {
  AppContext context;
  if (!arePthreadsEnabled()) {
    std::cerr << "Pthreads are required." << std::endl;
    return 1;
  }
  std::cout << "Pthreads enabled" << std::endl;

  std::string occt_version_str =
      "OCCT Version: " + std::string(OCC_VERSION_COMPLETE);

  std::cout << occt_version_str << std::endl;

  std::string containerId = "staircase-container";
  std::string canvasId = "staircase-canvas";

  EM_ASM({
    var divElement = document.getElementById(UTF8ToString($0));

    if (divElement) {
      var canvas = document.createElement('canvas');
      canvas.id = UTF8ToString($1);
      divElement.appendChild(canvas);

      // Align canvas size with device pixels
      var computedStyle = window.getComputedStyle(canvas);
      var cssWidth = parseInt(computedStyle.getPropertyValue('width'), 10);
      var cssHeight = parseInt(computedStyle.getPropertyValue('height'), 10);
      var devicePixelRatio = window.devicePixelRatio || 1;
      canvas.width = cssWidth * devicePixelRatio;
      canvas.height = cssHeight * devicePixelRatio;

    }
  }, containerId.c_str(), canvasId.c_str());

  // webgl setup
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

  EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_create_context(("#" + canvasId).c_str(), &attrs);
  emscripten_webgl_make_context_current(ctx);

  GLint viewport[4];
  glGetIntegerv(GL_VIEWPORT, viewport);
  context.canvasWidth = viewport[2];
  context.canvasHeight = viewport[3];

  std::cout << "Canvas size: " << context.canvasWidth << "x" << context.canvasHeight << std::endl;

  char const *vertexShaderSource =
    "attribute vec3 position;"
    "void main() {"
    "  gl_Position  = vec4(position, 1.0);"
    "}";

  char const *fragmentShaderSource =
    "precision mediump float;"
    "uniform vec4 color;"
    "void main() {"
    "  gl_FragColor = color;"
    "}";

  GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
  glCompileShader(vertexShader);

  GLint success;
  glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
  if (!success) {
      GLchar infoLog[512];
      glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
      std::cerr << "Error compiling vertex shader: " << infoLog << std::endl;
      return 1;
  }

  GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
  glCompileShader(fragmentShader);

  glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
  if (!success) {
      GLchar infoLog[512];
      glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
      std::cerr << "Error compiling fragment shader: " << infoLog << std::endl;
      return 1;
  }


  GLuint shaderProgram = glCreateProgram();
  glAttachShader(shaderProgram, vertexShader);
  glAttachShader(shaderProgram, fragmentShader);
  glLinkProgram(shaderProgram);
  glUseProgram(shaderProgram);

  glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
  if (!success) {
    GLchar infoLog[512];
    glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
    std::cerr << "Error linking shader program: " << infoLog << std::endl;
  }

  context.shaderProgram = shaderProgram;

  EM_ASM({ document.getElementById('version').innerHTML = UTF8ToString($0); },
         occt_version_str.c_str());
  EM_ASM({ document.getElementById('stepText').innerHTML = UTF8ToString($0); },
         embeddedStepFile.c_str());


  context.pushMessage({MessageType::DrawCheckerboard, std::any{}});
  context.pushMessage({MessageType::ReadDemoStepFile, std::any{}});

  emscripten_set_main_loop_arg(main_loop, &context, 0, 1);

  return 0;
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

void readDemoSTEPFile(AppContext &context, CallbackType callback) {

  auto aNewDoc = [&]() -> DocHandle {
    Handle(TDocStd_Document) aDoc;
    context.getApp()->NewDocument("MDTV-XCAF", aDoc);
    return aDoc;
  };

  std::istringstream fromStream(embeddedStepFile);

  std::optional<DocHandle> docOpt;
  {
    Timer timer = Timer("readInto(aNewDoc, fromStream)");
    docOpt = readInto(aNewDoc, fromStream);
  }
  if (docOpt.has_value()) { printLabels(docOpt.value()->Main()); }

  callback(docOpt);
}

void readStepFileDone() {
  std::cout << "TODO: readStepFileDone" << std::endl;
}
