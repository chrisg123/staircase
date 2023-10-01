#include "EmbeddedStepFile.h"
#include <AIS_InteractiveContext.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <GLES2/gl2.h>
#include <OpenGl_GraphicDriver.hxx>
#include <V3d_View.hxx>
#include <Wasm_Window.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <algorithm>
#include <any>
#include <chrono>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <iostream>
#include <mutex>
#include <opencascade/AIS_Shape.hxx>
#include <opencascade/STEPCAFControl_Reader.hxx>
#include <opencascade/Standard_Version.hxx>
#include <opencascade/TDF_ChildIterator.hxx>
#include <opencascade/TDataStd_Name.hxx>
#include <opencascade/TDocStd_Document.hxx>
#include <opencascade/XCAFApp_Application.hxx>
#include <opencascade/XCAFDoc_ShapeTool.hxx>
#include <optional>
#include <queue>
#include <thread>

using DocHandle = Handle(TDocStd_Document);
using CallbackType = std::function<void(std::optional<DocHandle>)>;

namespace MessageType {
enum Type {
  SetVersionString,
  DrawCheckerboard,
  DrawLoadingScreen,
  ReadStepFile,
  RenderStepFile
};

static char const *toString(Type type) {
  switch (type) {
  case SetVersionString: return "SetVersionString";
  case DrawCheckerboard: return "DrawCheckerboard";
  case DrawLoadingScreen: return "DrawLoadingScreen";
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
const RGB Gray     = {0.5f, 0.5f, 0.5f};
const RGB Platinum = {0.9f, 0.9f, 0.9f};
// clang-format on
} // namespace Colors

struct SpinnerParams {
  // clang-format off
  float radius        = 15.0f;
  float lineThickness = 2.0f;
  float speed         = 0.1f;
  float initialAngle  = 0.0f;
  RGB color           = Colors::Gray;
  // clang-format on
};

enum class RenderingMode {
  None,
  DrawCheckerboard,
  DrawLoadingScreen,
  RenderStepFile
};

namespace Staircase {
struct Message {
  MessageType::Type type;
  std::any data;
};
} // namespace Staircase

class AppContext {
public:
  AppContext() { app = XCAFApp_Application::GetApplication(); }

  Handle(XCAFApp_Application) getApp() const { return app; }

  void pushMessage(Staircase::Message const &msg) {
    std::lock_guard<std::mutex> lock(queueMutex);
    messageQueue.push(msg);
  }

  std::queue<Staircase::Message> drainMessageQueue() {
    std::lock_guard<std::mutex> lock(queueMutex);
    std::queue<Staircase::Message> localQueue;
    std::swap(localQueue, messageQueue);
    return localQueue;
  }
  RenderingMode renderingMode = RenderingMode::None;
  DocHandle currentlyViewingDoc;

  GLuint shaderProgram;
  GLint canvasWidth = 0;
  GLint canvasHeight = 0;

  Handle(V3d_Viewer) viewer;
  Handle(AIS_InteractiveContext) viewerContext;
  Handle(V3d_View) view;
  Handle(AIS_InteractiveContext) aisContext;

  bool stepFileLoaded = false;
  bool occtComponentsInitialized = false;
  bool shouldRotate = true;
  SpinnerParams spinnerParams;
  std::string canvasId;

private:
  Handle(XCAFApp_Application) app;
  std::queue<Staircase::Message> messageQueue;
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
void initializeOcctComponents(AppContext &context);
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
void drawLoadingScreen(AppContext &context, SpinnerParams &spinnerParams);
void clearCanvas(RGB color);

EM_BOOL onMouseCallback(int eventType, EmscriptenMouseEvent const *mouseEvent,
                        void *userData) {
  AppContext *context = static_cast<AppContext *>(userData);

  switch (eventType) {
  case EMSCRIPTEN_EVENT_MOUSEDOWN:
    std::cout << "[EVT] EMSCRIPTEN_EVENT_MOUSEDOWN" << std::endl;
    context->shouldRotate = !context->shouldRotate;
    break;
  default:
    std::cout << "Unhandled mouse event type: " << eventType << std::endl;
    break;
  }
  return EM_TRUE;
}

void initializeUserInteractions(AppContext &context) {
  const EM_BOOL toUseCapture = EM_TRUE;

  emscripten_set_mousedown_callback(("#" + context.canvasId).c_str(), &context,
                                    toUseCapture, onMouseCallback);
}

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
      context->renderingMode = RenderingMode::DrawCheckerboard;
      break;
    case MessageType::DrawLoadingScreen:
      context->renderingMode = RenderingMode::DrawLoadingScreen;
      break;
    case MessageType::ReadStepFile: {
      std::string stepFileStr =
          std::any_cast<std::string>(message.data).c_str();
      context->renderingMode = RenderingMode::DrawLoadingScreen;
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
              Staircase::Message msg;
              msg.type = MessageType::RenderStepFile;
              msg.data = docOpt.value();
              context->pushMessage(msg);
            });
      }).detach();
      break;
    }
    case MessageType::RenderStepFile:
      DocHandle aDoc = std::any_cast<DocHandle>(message.data);
      context->currentlyViewingDoc = aDoc;
      context->renderingMode = RenderingMode::RenderStepFile;
      printLabels(aDoc->Main());
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
  context.canvasId = "staircase-canvas";

  createCanvas(containerId, context.canvasId);
  initializeUserInteractions(context);
  setupWebGLContext(context.canvasId);
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

  std::thread([&]() {
    // Delay the STEP file loading to briefly show the initial checkerboard state.
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    context.pushMessage(
        {MessageType::ReadStepFile, std::any{embeddedStepFile}});
  }).detach();

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
        // Set the canvas to emscripten Module object
        Module['canvas'] = canvas;
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

void initializeOcctComponents(AppContext &context) {

  float myDevicePixelRatio = emscripten_get_device_pixel_ratio();
  Handle(Aspect_DisplayConnection) aDisp;
  Handle(OpenGl_GraphicDriver) aDriver = new OpenGl_GraphicDriver(aDisp, false);
  aDriver->ChangeOptions().buffersNoSwap = true;
  aDriver->ChangeOptions().buffersOpaqueAlpha = true;

  if (!aDriver->InitContext()) {
    std::cerr << "Error: EGL initialization failed" << std::endl;
    return;
  }

  Handle(V3d_Viewer) aViewer = new V3d_Viewer(aDriver);
  aViewer->SetComputedMode(false);
  aViewer->SetDefaultShadingModel(Graphic3d_TypeOfShadingModel_Phong);
  aViewer->SetDefaultLights();
  aViewer->SetLightOn();
  for (V3d_ListOfLight::Iterator aLightIter(aViewer->ActiveLights());
       aLightIter.More(); aLightIter.Next()) {
    const Handle(V3d_Light) &aLight = aLightIter.Value();
    if (aLight->Type() == Graphic3d_TypeOfLightSource_Directional) {
      aLight->SetCastShadows(true);
    }
  }

  Handle(Wasm_Window) aWindow = new Wasm_Window(context.canvasId.c_str());
  aWindow->Size(context.canvasWidth, context.canvasHeight);

  Handle(Prs3d_TextAspect) myTextStyle = new Prs3d_TextAspect();
  myTextStyle->SetFont(Font_NOF_ASCII_MONO);
  myTextStyle->SetHeight(12);
  myTextStyle->Aspect()->SetColor(Quantity_NOC_GRAY95);
  myTextStyle->Aspect()->SetColorSubTitle(Quantity_NOC_BLACK);
  myTextStyle->Aspect()->SetDisplayType(Aspect_TODT_SHADOW);
  myTextStyle->Aspect()->SetTextFontAspect(Font_FA_Bold);
  myTextStyle->Aspect()->SetTextZoomable(false);
  myTextStyle->SetHorizontalJustification(Graphic3d_HTA_LEFT);
  myTextStyle->SetVerticalJustification(Graphic3d_VTA_BOTTOM);

  Handle(V3d_View) aView = aViewer->CreateView();
  aView->Camera()->SetProjectionType(Graphic3d_Camera::Projection_Perspective);
  aView->SetImmediateUpdate(false);
  aView->ChangeRenderingParams().IsShadowEnabled = false;
  aView->ChangeRenderingParams().Resolution =
      (unsigned int)(96.0 * myDevicePixelRatio + 0.5);
  aView->ChangeRenderingParams().ToShowStats = true;
  aView->ChangeRenderingParams().StatsTextAspect = myTextStyle->Aspect();
  aView->ChangeRenderingParams().StatsTextHeight = (int)myTextStyle->Height();
  aView->SetWindow(aWindow);

  Handle(AIS_InteractiveContext) aContext = new AIS_InteractiveContext(aViewer);

  context.view = aView;
  context.viewer = aViewer;
  context.aisContext = aContext;
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

std::vector<TopoDS_Shape> getShapesFromDoc(DocHandle const aDoc) {
  std::vector<TopoDS_Shape> shapes;

  TDF_Label mainLabel = aDoc->Main();
  Handle(XCAFDoc_ShapeTool) shapeTool =
      XCAFDoc_DocumentTool::ShapeTool(mainLabel);
  for (TDF_ChildIterator it(mainLabel, Standard_True); it.More(); it.Next()) {
    TDF_Label label = it.Value();
    if (!shapeTool->IsShape(label)) { continue; }
    TopoDS_Shape shape;
    if (!shapeTool->GetShape(label, shape) || shape.IsNull()) {
      std::cerr << "Failed to get shape from label." << std::endl;
      continue;
    }
    shapes.push_back(shape);
  }
  return shapes;
}

void renderStepFile(AppContext &context) {
  DocHandle aDoc = context.currentlyViewingDoc;
  Handle(AIS_InteractiveContext) aisContext = context.aisContext;
  Handle(V3d_View) aView = context.view;
  if (aDoc.IsNull() || context.aisContext.IsNull()) {
    std::cerr << "No document or AIS context available to render." << std::endl;
    return;
  }

  if (!context.stepFileLoaded) {
    std::vector<TopoDS_Shape> shapes = getShapesFromDoc(aDoc);
    {
      Timer timer = Timer("Populate aisContext with shapes;");
      for (auto const &shape : shapes) {
        Handle(AIS_Shape) aisShape = new AIS_Shape(shape);
        aisContext->Display(aisShape, Standard_True);
      }
    }

    context.stepFileLoaded = true;
  }

  if (context.shouldRotate) {
    static double angle = 0.0;
    angle += 0.01;
    if (angle >= 2 * M_PI) { angle = 0.0; }

    gp_Dir aDir(sin(angle), cos(angle), aView->Camera()->Direction().Z());
    aView->Camera()->SetDirection(aDir);
    aView->Redraw();
  }
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

void drawCircle(AppContext &context, GLfloat centerX, GLfloat centerY,
                GLfloat radius, RGB color, float aspectRatio) {
  int const numSegments = 100; // Increase for a smoother circle

  GLfloat vertices[numSegments * 3];

  for (int i = 0; i < numSegments; ++i) {
    float theta = 2.0f * M_PI * float(i) / float(numSegments);
    GLfloat x = radius * cos(theta);
    GLfloat y = radius * sin(theta);
    vertices[i * 3] = x + centerX;
    vertices[i * 3 + 1] = (y * aspectRatio) + centerY;
    vertices[i * 3 + 2] = 0.0f;
  }

  GLint colorUniform = glGetUniformLocation(context.shaderProgram, "color");
  GLint posAttrib = glGetAttribLocation(context.shaderProgram, "position");

  glUniform4f(colorUniform, color.r, color.g, color.b, 1.0f);

  GLuint vertexBuffer;
  glGenBuffers(1, &vertexBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE, 0, 0);
  glEnableVertexAttribArray(posAttrib);

  glDrawArrays(GL_LINE_LOOP, 0, numSegments);

  glDeleteBuffers(1, &vertexBuffer);
}

void drawLine(AppContext &context, GLfloat x1, GLfloat y1, GLfloat x2,
              GLfloat y2, GLfloat thickness, RGB color) {

  GLfloat vertices[] = {x1, y1, 0.0f, x2, y2, 0.0f};

  GLint colorUniform = glGetUniformLocation(context.shaderProgram, "color");
  GLint posAttrib = glGetAttribLocation(context.shaderProgram, "position");

  glUniform4f(colorUniform, color.r, color.g, color.b, 1.0f);

  GLuint vertexBuffer;
  glGenBuffers(1, &vertexBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE, 0, 0);
  glEnableVertexAttribArray(posAttrib);
  glLineWidth(thickness);

  glDrawArrays(GL_LINES, 0, 2);
  glDeleteBuffers(1, &vertexBuffer);
}

void drawCheckerBoard(AppContext &context) {
  float aspectRatio = static_cast<float>(context.canvasWidth) /
                      static_cast<float>(context.canvasHeight);

  float const coordMin = -1.0f;
  float const coordMax = 1.0f;
  float const coordRange = coordMax - coordMin;

  // determine rows and columns based arbitrarily on 5% of the longer side.
  float fivePercentLongestSide =
      std::max(context.canvasHeight, context.canvasWidth) * 0.05f;
  int rows = static_cast<int>(context.canvasHeight / fivePercentLongestSide);
  int cols = static_cast<int>(context.canvasWidth / fivePercentLongestSide);

  // Adjust square size to fit perfectly within canvas width
  float squareSize = coordRange / cols;
  if (squareSize * rows < coordRange) { rows += 1; }

  for (int i = 0; i < cols; ++i) {
    for (int j = 0; j < rows; ++j) {
      RGB color = (i + j) % 2 == 0 ? Colors::Green : Colors::Blue;
      float x = coordMin + (i * squareSize);
      // Scale y by aspect ratio since square size is based on canvas width.
      float y = (coordMin / aspectRatio) + (j * squareSize);
      drawSquare(context, x, y, squareSize, color.r, color.g, color.b,
                 aspectRatio);
    }
  }
}
void drawLoadingScreen(AppContext &context, SpinnerParams &spinnerParams) {
  float aspectRatio = static_cast<float>(context.canvasWidth) /
                      static_cast<float>(context.canvasHeight);

  float const coordMin = -1.0f;
  float const coordMax = 1.0f;
  float const coordRange = coordMax - coordMin;

  float centerX = 0;
  float centerY = 0;
  float handFactor = 0.75f;
  float offsetX =
      coordRange *
      ((spinnerParams.radius * handFactor * cos(spinnerParams.initialAngle)) /
       context.canvasWidth);
  float offsetY =
      coordRange *
      ((spinnerParams.radius * handFactor * sin(spinnerParams.initialAngle)) /
       context.canvasHeight);

  float startX = centerX;
  float startY = centerY;
  float endX = centerX + offsetX;
  float endY = centerY + offsetY;

  drawCircle(context, 0, 0,
             coordRange * (spinnerParams.radius / context.canvasWidth),
             spinnerParams.color, aspectRatio);

  drawLine(context, startX, startY, endX, endY, spinnerParams.lineThickness,
           spinnerParams.color);

  float dotSize = coordRange *
                  ((spinnerParams.lineThickness * 1.1) / context.canvasWidth) *
                  1.1;
  drawSquare(context, centerX - dotSize / 2, centerY - dotSize / 2, dotSize,
             spinnerParams.color.r, spinnerParams.color.g,
             spinnerParams.color.b,
             aspectRatio); // Aspect ratio set to 1.0f for square

  spinnerParams.initialAngle -= spinnerParams.speed;
  if (spinnerParams.initialAngle >= 2 * M_PI) {
    spinnerParams.initialAngle = 0;
  }
}

void draw(AppContext &context) {
  GLenum err = glGetError();
  if (err != GL_NO_ERROR) {
    std::cerr << "OpenGL error: " << err << std::endl;
    return;
  }

  switch (context.renderingMode) {
  case RenderingMode::DrawCheckerboard: drawCheckerBoard(context); break;
  case RenderingMode::RenderStepFile:
    if (!context.occtComponentsInitialized) {
      initializeOcctComponents(context);
      context.occtComponentsInitialized = true;
    }
    renderStepFile(context);
    break;
  case RenderingMode::DrawLoadingScreen: {
    clearCanvas(Colors::Platinum);
    drawLoadingScreen(context, context.spinnerParams);
    break;
  }
  default: break;
  }
}
