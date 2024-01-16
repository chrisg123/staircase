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
  InitEmptyScene,
  InitStepFile,
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
  case InitEmptyScene: return "InitEmptyScene";
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


template <typename... MessageTypes>
std::shared_ptr<Staircase::Message> chain(MessageTypes... types) {
  std::shared_ptr<Staircase::Message> head = nullptr;
  std::shared_ptr<Staircase::Message> current = nullptr;

  auto createAndLink = [&head, &current](MessageType::Type type) {
    auto newMsg = std::make_shared<Staircase::Message>(type);
    if (!head) {
      head = newMsg;
    } else {
      current->nextMessage = newMsg;
    }
    current = newMsg;
  };
  (createAndLink(types), ...);
  return head;
}

#endif // STAIRCASE_HPP
