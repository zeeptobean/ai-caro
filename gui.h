#ifndef AI_CARO_GUI_H
#define AI_CARO_GUI_H

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "agent.h"
#include "caro.h"
#include "util.h"

struct AgentTask {
  enum Status { kIdle, kRunning, kCompleted };
  std::pair<unsigned, unsigned> move = {0, 0};
  std::atomic<Status> status{Status::kIdle};
  float duration_ms{0.0f};
};

class UiApplication {
  GLFWwindow* window_ = nullptr;
  bool dark_theme_ = true;
  float font_size_ = 11.0f;
  const char* kBotOptions[3] = {"AB", "ANTS", "MCTS"};
  const float kSymbolThickness = 2.5f, kSymbolPadding = 6.0f, kLastMoveThickness = 2.0f;

  int n_ = 10, m_ = 10, k_ = 5;
  bool show_game_ = false, in_game_ = false, player_turn_ = true, game_over_ = false;
  std::vector<MoveHistory> move_history_;
  std::string game_msg_ = "";

  std::shared_ptr<Agent> bot_ = nullptr;
  std::shared_ptr<Caro> game_ = nullptr;
  AgentTask agent_task_;

  void Draw();
  void DrawSettings();
  void DrawGameBoard();

  static void GlfwFramebufferSizeCallback(GLFWwindow* window, int width, int height);
  void RenderFrame();

  void StartGame(int, unsigned, bool);
  void ResetGame();
  void ResetAgentTask();

 public:
  UiApplication(const char* program_name);
  ~UiApplication();
  void Run();
};

#endif  // AI_CARO_GUI_H