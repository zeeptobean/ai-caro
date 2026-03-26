#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

#include "alpha_beta.h"
#include "caro.h"
#include "ybwc.h"

static void glfw_error_callback(int error, const char* description) {
  std::cerr << std::format("GLFW Error {}: {}\n", error, description);
}

static void headless_hint(const char* program_name) {
  std::cerr << "It looks like you are running this program in a headless environment\n";
  std::cerr << "Run \"" << program_name << " cli\" to use the command-line interface.\n";
}

class AppState {
  AppState() = default;

  ~AppState() {
    if (bot) delete bot;
    if (game) delete game;
  }

  struct AgentTask {
    std::pair<unsigned, unsigned> move;
    std::atomic<bool> is_complete{false};
    std::atomic<bool> is_thread_running{false};
    float duration_ms{0.0f};
  };

  AgentTask agent_task_;

 public:
  AppState(const AppState&) = delete;
  AppState& operator=(const AppState&) = delete;

  static AppState& Get() {
    static AppState instance;
    return instance;
  }

  Agent* bot = nullptr;
  Caro* game = nullptr;

  AgentTask& GetAgentTask() { return agent_task_; }
  void ResetAgentTask() {
    agent_task_.move = {0, 0};
    agent_task_.is_complete.store(false);
  }
};

struct MoveHistory {
  unsigned i, j;
  bool is_player;
};

void Draw() {
  static int m = 10, n = 10, k = 5, time_limit = 2000, bot_index = 0;
  static bool has_error = false, in_game = false, player_start = true, player_turn = true;
  static bool ai_thinking = false, show_game = false;
  static std::vector<MoveHistory> move_history;
  static std::string game_msg = "";
  const char* bot_options[] = {"AB", "YBWC", "ANTS", "MCTS"};

  Agent*& bot = AppState::Get().bot;
  Caro*& game = AppState::Get().game;

  ImGui::Begin("m,n,k", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);

  ImGui::BeginDisabled(in_game);
  ImGui::SetNextItemWidth(100.0f);
  if (ImGui::InputInt("m", &m)) {
    if (m < 3) m = 3;
  }
  ImGui::SetNextItemWidth(100.0f);
  if (ImGui::InputInt("n", &n)) {
    if (n < 3) n = 3;
  }
  ImGui::SetNextItemWidth(100.0f);
  if (ImGui::InputInt("k", &k)) {
    if (k > n || k > m) k = std::min(m, n);
    if (k < 3) k = 3;
    if (k > 16) k = 16;
  }
  if (k > n || k > m) {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error: k cannot be greater than m or n!");
    has_error = true;
  } else {
    has_error = false;
  }

  ImGui::SetNextItemWidth(150.0f);
  if (ImGui::BeginCombo("Bot", bot_options[bot_index])) {
    for (int i = 0; i < IM_ARRAYSIZE(bot_options); i++) {
      bool is_selected = (bot_index == i);
      int flags = 0;
      if (i == 2 || i == 3) flags |= ImGuiSelectableFlags_Disabled;
      if (ImGui::Selectable(bot_options[i], is_selected, flags)) {
        bot_index = i;
      }
      if (is_selected) ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }

  ImGui::SetNextItemWidth(100.0f);
  if (ImGui::InputInt("Time constraint", &time_limit)) {
    if (time_limit < 50) time_limit = 50;
  }

  ImGui::Checkbox("Player goes first", &player_start);
  ImGui::EndDisabled();

  ImGui::Dummy(ImVec2(0, 10));

  ImGui::BeginDisabled(has_error || in_game);
  if (ImGui::Button("Game!")) {
    // Reset prev game
    if (!in_game || !AppState::Get().GetAgentTask().is_thread_running.load()) {
      if (game) delete game;
      if (bot) delete bot;
    }
    game = new Caro(m, n, k);
    switch (bot_index) {
      case 0:
        bot = new AlphaBetaAgent(time_limit);
        break;
      case 1:
        bot = new YBWCAgent(k, time_limit);
        break;
      default:
        throw std::runtime_error("Invalid bot index. This should never happen");
        break;
    }
    player_turn = player_start;
    if (player_turn) {
      game_msg = "Your turn";
    }
    in_game = true;
    show_game = true;
    ai_thinking = false;
    move_history.clear();
    AppState::Get().ResetAgentTask();
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(!in_game);
  if (ImGui::Button("Reset")) {
    if (ai_thinking) {
      if (bot) bot->RequestCancel();
    }
    game_msg = "";
    in_game = false;
    show_game = false;
    ai_thinking = false;
    move_history.clear();
  }
  ImGui::EndDisabled();

  ImGui::Text(game_msg.c_str());
  if (show_game) {
    ImGui::SeparatorText("Game Board");

    ImGui::BeginChild("Container A", ImVec2(-1, -1), true, ImGuiWindowFlags_HorizontalScrollbar);

    ImGui::BeginDisabled(!player_turn || !in_game);
    for (int i = 0; i < n; i++) {
      for (int j = 0; j < m; j++) {
        ImGui::PushID(i * m + j);
        if (ImGui::InvisibleButton("##cell", ImVec2(32, 32)) &&
            game->GetCell(i, j) == Caro::kEmptyCell) {
          game->PlaceMove(i, j, Caro::kMarkPlayer);
          player_turn = !player_turn;
          move_history.emplace_back(i, j, true);
          if (game->CheckGameState() != Caro::GameState::kNormal) {
            in_game = false;
          }
        }

        // Draw cell
        ImVec2 pos_min = ImGui::GetItemRectMin();
        ImVec2 pos_max = ImGui::GetItemRectMax();
        float padding = 2.0f;
        pos_min.x += padding;
        pos_min.y += padding;
        pos_max.x -= padding;
        pos_max.y -= padding;

        // Hover Glow Effect
        if (ImGui::IsItemHovered() && game->GetCell(i, j) == Caro::kEmptyCell) {
          ImGui::GetWindowDrawList()->AddRectFilled(pos_min, pos_max, IM_COL32(0, 255, 255, 40));
        }

        // Draw Cell Border
        ImGui::GetWindowDrawList()->AddRect(pos_min, pos_max, IM_COL32(200, 200, 200, 100));

        // Draw X or O
        if (game->GetCell(i, j) == Caro::kMarkPlayer) {
          ImGui::GetWindowDrawList()->AddLine(ImVec2(pos_min.x, pos_min.y),
                                              ImVec2(pos_max.x, pos_max.y),
                                              IM_COL32(255, 50, 50, 255), 4.0f);
          ImGui::GetWindowDrawList()->AddLine(ImVec2(pos_max.x, pos_min.y),
                                              ImVec2(pos_min.x, pos_max.y),
                                              IM_COL32(255, 50, 50, 255), 4.0f);
        } else if (game->GetCell(i, j) == Caro::kMarkComputer) {
          ImGui::GetWindowDrawList()->AddCircle(
              ImVec2((pos_min.x + pos_max.x) * 0.5f, (pos_min.y + pos_max.y) * 0.5f),
              (pos_max.x - pos_min.x) * 0.5f - padding, IM_COL32(50, 50, 255, 255), 0, 2.0f);
        }

        // Draw last move border
        if (!move_history.empty() && i == move_history.back().i && j == move_history.back().j) {
          ImGui::GetWindowDrawList()->AddRect(pos_min, pos_max, IM_COL32(255, 255, 0, 255), 0.0f, 0,
                                              3.0f);
        }

        if (j < m - 1) ImGui::SameLine();
        ImGui::PopID();
      }
    }
    ImGui::EndDisabled();

    ImGui::EndChild();

    if (!player_turn) {
      if (!ai_thinking) {
        ai_thinking = true;
        game_msg = "Computer is thinking...";
        std::thread([]() {
          auto& task = AppState::Get().GetAgentTask();
          task.is_thread_running.store(true);
          auto bot = AppState::Get().bot;
          auto game = AppState::Get().game;
          auto start_time = std::chrono::steady_clock::now();
          task.move = bot->GetMove(*game);
          auto end_time = std::chrono::steady_clock::now();
          std::chrono::duration<float, std::milli> duration = end_time - start_time;
          if (!bot->CancelStatus()) {
            task.duration_ms = duration.count();
            task.is_complete.store(true);
          }
          task.is_thread_running.store(false);
        }).detach();
      }

      if (AppState::Get().GetAgentTask().is_complete.load()) {
        const auto& task = AppState::Get().GetAgentTask();
        if (!game->PlaceMove(task.move.first, task.move.second, Caro::kMarkComputer)) {
          game_msg = "Invalid bot move. The game terminated";
          in_game = false;
        }
        game_msg = std::format("Computer move: ({}, {}) calculated in {:.2f} ms. Your turn.\n",
                               task.move.first, task.move.second, task.duration_ms);
        move_history.emplace_back(task.move.first, task.move.second, false);
        AppState::Get().ResetAgentTask();
        ai_thinking = false;
        player_turn = true;
        if (game->CheckGameState() != Caro::GameState::kNormal) {
          in_game = false;
        }
      }
    }

    if (!in_game) {
      switch (AppState::Get().game->CheckGameState()) {
        case Caro::GameState::kPlayerWin:
          game_msg = "Congratulations! You win!";
          break;
        case Caro::GameState::kComputerWin:
          game_msg = "Computer wins! Better luck next time.";
          break;
        case Caro::GameState::kDraw:
          game_msg = "It's a draw!";
          break;
        default:
          break;
      }
      ai_thinking = false;
    }
  }

  ImGui::End();
}

int main(int argc, char** argv) {
  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit()) {
    headless_hint(argv[0]);
    return 1;
  }

#if defined(__APPLE__)
  // GL 3.2 + GLSL 150 for macOS
  // Never have an Apple device to test this out smh...
  const char* glsl_version = "#version 150";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
  // GL 3.0 + GLSL 130 for Windows/Linux
  const char* glsl_version = "#version 130";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

  GLFWwindow* window = glfwCreateWindow(1280, 720, "AI Caro", nullptr, nullptr);
  if (window == nullptr) {
    headless_hint(argv[0]);
    return 1;
  }

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);  // Enable vsync

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.IniFilename = nullptr;
  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowPadding = ImVec2(8.0f, 8.0f);

  ImGui::StyleColorsDark();

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Set internal window size to match the viewport size
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    Draw();

    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w,
                 clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}

/*
int main2() {
  namespace chrono = std::chrono;

  Caro game(16, 16, 5);
  auto [m, n] = game.GetBoardSize();

  // std::unique_ptr<Agent> bot = std::make_unique<YBWCAgent>(4, 987);
  std::unique_ptr<Agent> bot = std::make_unique<AlphaBetaAgent>(1000);

  std::stringstream ss;
  std::string input_str;
  unsigned ux = 0, uy = 0;
  int x, y;

  std::vector<std::pair<unsigned, unsigned>> human_move_history;
  std::vector<std::pair<unsigned, unsigned>> computer_move_history;

  while (true) {
    game.Display();
    while (true) {
      std::cout << "Enter your move (row and column, 0-indexed). -1 for undo: ";
      getline(std::cin, input_str);
      ss.clear();
      ss.str(input_str);
      ss >> x >> y;
      if (x == -1) {
        if (human_move_history.empty() || computer_move_history.empty()) {
          std::cout << "No moves to undo.\n";
          continue;
        }
        auto [last_computer_x, last_computer_y] = computer_move_history.back();
        computer_move_history.pop_back();
        game.UndoMove(last_computer_x, last_computer_y);
        auto [last_human_x, last_human_y] = human_move_history.back();
        human_move_history.pop_back();
        game.UndoMove(last_human_x, last_human_y);

        std::cout << "Last move undone.\n";
        game.Display();
        continue;
      }
      if (!ss || x >= static_cast<int>(m) || y >= static_cast<int>(n) || x < 0 || y < 0) {
        std::cout << "Invalid input. Please enter valid row and column numbers.\n";
        continue;
      }

      ux = static_cast<unsigned>(x);
      uy = static_cast<unsigned>(y);
      if (!game.PlaceMove(ux, uy, Caro::kMarkPlayer)) {
        std::cout << "Invalid move. Try again.\n";
        continue;
      }
      break;
    };

    human_move_history.push_back({ux, uy});

    game.Display();

    // User's turn

    auto state = game.CheckGameState();
    if (state == Caro::GameState::kPlayerWin) {
      std::cout << "Congratulations! You win!\n";
      break;
    } else if (state == Caro::GameState::kDraw) {
      std::cout << "It's a draw!\n";
      break;
    }

    // Computer's turn

    std::cout << "Computer is thinking...\n";
    auto start_time = chrono::steady_clock::now();
    auto [comp_x, comp_y] = bot->GetMove(game);
    bool computer_move_status = game.PlaceMove(comp_x, comp_y, bot->GetMark());
    auto end_time = chrono::steady_clock::now();
    chrono::duration<float, std::milli> duration = end_time - start_time;
    std::cout << std::format("Computer move: ({}, {}) calculated in {:.2f} ms\n", comp_x, comp_y,
                             duration.count());
    if (!computer_move_status) {
      std::cout << "Invalid move by computer. This should not happen\n";
      exit(1);
    }
    computer_move_history.push_back({comp_x, comp_y});

    state = game.CheckGameState();
    if (state == Caro::GameState::kComputerWin) {
      std::cout << "Computer wins! Better luck next time.\n";
      break;
    } else if (state == Caro::GameState::kDraw) {
      std::cout << "It's a draw!\n";
      break;
    }
  }
}
  */