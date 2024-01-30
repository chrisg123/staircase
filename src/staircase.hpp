#ifndef STAIRCASE_HPP
#define STAIRCASE_HPP
#include "StaircaseViewController.hpp"
#include <any>
#include <iostream>

#ifdef DEBUG_BUILD
#include <chrono>
#include <iomanip>
#include <sstream>
#endif

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
  LoadStepFile,
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
  case LoadStepFile: return "LoadStepFile";
  default: return "Unknown";
  }
}
} // namespace MessageType

namespace Staircase {
struct Message {
  MessageType::Type type;
  void *data;
  std::shared_ptr<Message> nextMessage;

  Message(MessageType::Type type, void *data = nullptr)
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

#ifdef DEBUG_BUILD
#define DEBUG_EXECUTE(CodeBlock) CodeBlock
#else
#define DEBUG_EXECUTE(CodeBlock)
#endif

inline void debugOut(const std::string &msg) {
  #ifdef DEBUG_BUILD
  auto now = std::chrono::system_clock::now();
  auto nowAsTimeT = std::chrono::system_clock::to_time_t(now);
  auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
  std::stringstream timeStream;
  timeStream << std::put_time(std::localtime(&nowAsTimeT), "%Y-%m-%d %H:%M:%S");
  timeStream << '.' << std::setfill('0') << std::setw(3) << nowMs.count();
  std::cout << "[" << timeStream.str() << "] " << "DEBUG: " << msg << std::endl;
  #endif
}

#endif // STAIRCASE_HPP
