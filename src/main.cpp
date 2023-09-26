#include "EmbeddedStepFile.h"
#include <chrono>
#include <emscripten.h>
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

  void pushMessage(const Message &msg) {
    std::lock_guard<std::mutex> lock(queueMutex);
    messageQueue.push(msg);
  }

  std::queue<Message> drainMessageQueue() {
    std::lock_guard<std::mutex> lock(queueMutex);
    std::queue<Message> localQueue;
    std::swap(localQueue, messageQueue);
    return localQueue;
  }

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
void drawCheckerBoard();
void readDemoSTEPFile(AppContext &context, CallbackType callback);
void readStepFileDone();

void main_loop(void *arg) {
  AppContext *context = static_cast<AppContext*>(arg);
  auto localQueue = context->drainMessageQueue();

  while (!localQueue.empty()) {
    auto message = localQueue.front();
    localQueue.pop();

    switch (message.type) {
    case MessageType::DrawCheckerboard:
      std::cout << "[REC MSG] MessageType::DrawCheckerboard" << std::endl;
      drawCheckerBoard();
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
}

int main() {

  if (!arePthreadsEnabled()) {
    std::cerr << "Pthreads are required." << std::endl;
    return 1;
  }
  std::cout << "Pthreads enabled" << std::endl;

  std::string occt_version_str =
      "OCCT Version: " + std::string(OCC_VERSION_COMPLETE);

  std::cout << occt_version_str << std::endl;

  EM_ASM({ document.getElementById('version').innerHTML = UTF8ToString($0); },
         occt_version_str.c_str());
  EM_ASM({ document.getElementById('stepText').innerHTML = UTF8ToString($0); },
         embeddedStepFile.c_str());

  AppContext context;
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

void drawCheckerBoard() {
  std::cout << "TODO: DrawCheckerboard" << std::endl;
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
