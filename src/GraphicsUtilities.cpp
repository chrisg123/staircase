#include "GraphicsUtilities.hpp"
#include <emscripten.h>
#include <emscripten/html5.h>

void clearCanvas(RGB color) {
  glClearColor(color.r, color.g, color.b, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
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
