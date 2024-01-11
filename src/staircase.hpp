#ifndef STAIRCASE_HPP
#define STAIRCASE_HPP
#include "StaircaseViewController.hpp"
#include <any>
#include <iostream>

namespace MessageType {
enum Type {
  SetVersionString,
  ClearScreen,
  DrawCheckerboard,
  DrawLoadingScreen,
  ReadStepFile,
  InitDemoScene,
  NextFrame,
  SetStepFileContent,
};

static char const *toString(Type type) {
  switch (type) {
  case SetVersionString: return "SetVersionString";
  case ClearScreen: return "ClearScreen";
  case DrawCheckerboard: return "DrawCheckerboard";
  case DrawLoadingScreen: return "DrawLoadingScreen";
  case ReadStepFile: return "ReadStepFile";
  case InitDemoScene: return "InitDemoScene";
  case NextFrame: return "NextFrame";
  case SetStepFileContent: return "SetStepFileContent";
  default: return "Unknown";
  }
}
} // namespace MessageType

namespace Staircase {
struct Message {
  MessageType::Type type;
  std::shared_ptr<void> data;
  std::shared_ptr<Message> nextMessage;

  Message(MessageType::Type type, std::shared_ptr<void> data = nullptr)
      : type(type), data(data), nextMessage(nullptr) {}
};
} // namespace Staircase

int const AIS_WIREFRAME_MODE = 0;
int const AIS_SHADED_MODE = 1;

struct RGB {
  float r, g, b;
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
  ClearScreen,
  DrawCheckerboard,
  DrawLoadingScreen,
  RenderStepFile
};

inline void createCanvas(std::string containerId, std::string canvasId) {
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
#endif // STAIRCASE_HPP
