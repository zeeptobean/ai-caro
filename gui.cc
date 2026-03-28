#include "gui.h"

#include <chrono>
#include <exception>
#include <format>
#include <iostream>
#include <string>
#include <thread>

#include "alpha_beta.h"

static void glfw_error_callback(int error, const char* description) {
  std::cerr << std::format("GLFW Error {}: {}\n", error, description);
}

static std::string headless_hint(const char* program_name) {
  std::string str = "It looks like you are running this program in a headless environment\n";
  str += std::format("Run \"{} cli\" to use the command-line interface.\n", program_name);
  return str;
}

UiApplication::UiApplication(const char* program_name) {
  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit()) {
    throw std::runtime_error(headless_hint(program_name));
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

  window_ = glfwCreateWindow(1280, 720, "AI Caro", nullptr, nullptr);
  if (window_ == nullptr) {
    throw std::runtime_error(headless_hint(program_name));
  }

  glfwMakeContextCurrent(window_);
  glfwSwapInterval(1);  // Enable vsync

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.IniFilename = nullptr;
  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowPadding = ImVec2(12.0f, 12.0f);

  ImGui::StyleColorsDark();

  ImGui_ImplGlfw_InitForOpenGL(window_, true);
  ImGui_ImplOpenGL3_Init(glsl_version);
}

void UiApplication::Run() {
  while (!glfwWindowShouldClose(window_)) {
    glfwPollEvents();

    if (!glfwGetWindowAttrib(window_, GLFW_FOCUSED)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }

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
    glfwGetFramebufferSize(window_, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window_);
  }
}

UiApplication::~UiApplication() {
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  if (window_) {
    glfwDestroyWindow(window_);
    window_ = nullptr;
  }
  glfwTerminate();
}

void UiApplication::Draw() {
  static int time_limit = 2000, bot_index = 0;
  static bool has_error = false, player_start = true;
  static std::string game_msg = "";

  ImGui::Begin("m,n,k", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);

  DrawSettings();
  if (ImGui::Button("Settings")) {
    ImGui::OpenPopup("Settings");
  }
  ImGui::Dummy(ImVec2(0, 10));

  ImGui::BeginDisabled(in_game_);
  ImGui::SetNextItemWidth(100.0f);
  if (ImGui::InputInt("m", &m_)) {
    if (m_ < 3) m_ = 3;
  }
  ImGui::SetNextItemWidth(100.0f);
  if (ImGui::InputInt("n", &n_)) {
    if (n_ < 3) n_ = 3;
  }
  ImGui::SetNextItemWidth(100.0f);
  if (ImGui::InputInt("k", &k_)) {
    if (k_ > n_ || k_ > m_) k_ = std::min(m_, n_);
    if (k_ < 3) k_ = 3;
    if (k_ > 16) k_ = 16;
  }
  if (k_ > n_ || k_ > m_) {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error: k cannot be greater than m or n!");
    has_error = true;
  } else {
    has_error = false;
  }

  ImGui::SetNextItemWidth(150.0f);
  if (ImGui::BeginCombo("Bot", kBotOptions[bot_index])) {
    for (int i = 0; i < IM_ARRAYSIZE(kBotOptions); i++) {
      bool is_selected = (bot_index == i);
      int flags = 0;
      if (i != 0) flags |= ImGuiSelectableFlags_Disabled;
      if (ImGui::Selectable(kBotOptions[i], is_selected, flags)) {
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

  ImGui::BeginDisabled(has_error || in_game_);
  if (ImGui::Button("Game!")) {
    StartGame(bot_index, static_cast<unsigned>(time_limit), player_start);
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(!in_game_);
  if (ImGui::Button("Reset")) {
    ResetGame();
  }
  ImGui::EndDisabled();

  ImGui::Text("%s", game_msg_.c_str());
  DrawGameBoard();

  ImGui::End();
}

void UiApplication::DrawGameBoard() {
  if (show_game_) {
    ImGui::SeparatorText("Game Board");
    // ImGui::BeginDisabled(!player_turn_ || !in_game_ || move_history_.empty());
    // if (ImGui::Button("Undo")) {  //Only able to undo on player's turn
    //   const auto& last_move = move_history_.back();
    //   game_->RemoveMove(last_move.i, last_move.j);
    //   if (!last_move.is_player) {
    //     player_turn_ = !player_turn_;
    //   }
    //   move_history_.pop_back();
    //   if (game_->CheckGameState() == Caro::GameState::kNormal) {
    //     in_game_ = true;
    //     game_msg_ = player_turn_ ? "Your turn" : "Computer's turn";
    //   }
    // }
    // ImGui::EndDisabled();

    ImGui::BeginChild("Container A", ImVec2(-1, -1), true, ImGuiWindowFlags_HorizontalScrollbar);

    ImGui::BeginDisabled(!player_turn_ || !in_game_ || game_over_);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    for (int ti = 0; ti < n_; ti++) {
      for (int tj = 0; tj < m_; tj++) {
        unsigned i = static_cast<unsigned>(ti);
        unsigned j = static_cast<unsigned>(tj);
        ImGui::PushID(ti * m_ + tj);
        if (ImGui::InvisibleButton("##cell", ImVec2(32, 32)) &&
            game_->GetCell(i, j) == Caro::kEmptyCell) {
          (void)game_->PlaceMove(i, j, Caro::kMarkPlayer);
          player_turn_ = !player_turn_;
          move_history_.emplace_back(i, j, true);
          if (game_->CheckGameState() != Caro::GameState::kNormal) {
            in_game_ = false;
          }
        }

        // Draw cell
        ImVec2 pos_min = ImGui::GetItemRectMin();
        ImVec2 pos_max = ImGui::GetItemRectMax();

        ImVec2 sym_min = ImVec2(pos_min.x + kSymbolPadding, pos_min.y + kSymbolPadding);
        ImVec2 sym_max = ImVec2(pos_max.x - kSymbolPadding, pos_max.y - kSymbolPadding);

        // Hover Glow Effect + Tooltip
        if (ImGui::IsItemHovered() && game_->GetCell(i, j) == Caro::kEmptyCell) {
          ImGui::GetWindowDrawList()->AddRectFilled(pos_min, pos_max, IM_COL32(0, 255, 255, 40));
          ImGui::BeginTooltip();
          ImGui::Text("(row: %u, col: %u)", i + 1, j + 1);
          ImGui::EndTooltip();
        }

        // Draw Cell Border
        ImGui::GetWindowDrawList()->AddRect(pos_min, pos_max, IM_COL32(200, 200, 200, 100));

        // Draw X or O
        if (game_->GetCell(i, j) == Caro::kMarkPlayer) {
          ImGui::GetWindowDrawList()->AddLine(sym_min, sym_max, IM_COL32(255, 50, 50, 255),
                                              kSymbolThickness);
          ImGui::GetWindowDrawList()->AddLine(ImVec2(sym_max.x, sym_min.y),
                                              ImVec2(sym_min.x, sym_max.y),
                                              IM_COL32(255, 50, 50, 255), kSymbolThickness);
        } else if (game_->GetCell(i, j) == Caro::kMarkComputer) {
          float radius = (sym_max.x - sym_min.x) * 0.5f;
          ImVec2 center = ImVec2((sym_min.x + sym_max.x) * 0.5f, (sym_min.y + sym_max.y) * 0.5f);
          ImGui::GetWindowDrawList()->AddCircle(center, radius, IM_COL32(50, 50, 255, 255), 0,
                                                kSymbolThickness);
        }

        // Draw last move border
        if (!move_history_.empty() && i == move_history_.back().i && j == move_history_.back().j) {
          ImGui::GetWindowDrawList()->AddRect(pos_min, pos_max, IM_COL32(255, 255, 0, 255), 0.0f, 0,
                                              kLastMoveThickness);
        }

        if (tj < m_ - 1) ImGui::SameLine();
        ImGui::PopID();
      }
    }
    ImGui::PopStyleVar();
    ImGui::EndDisabled();

    ImGui::EndChild();

    if (in_game_ && !player_turn_ && !game_over_) {
      game_msg_ = "Computer is thinking...";
      if (agent_task_.status.load() == AgentTask::Status::kIdle) {
        agent_task_.status.store(AgentTask::Status::kRunning);
        std::thread([local_bot = bot_, local_game = game_,
                     &agent_task = this->agent_task_]() {  // local shared_ptr capture
          auto start_time = std::chrono::steady_clock::now();
          agent_task.move = local_bot->GetMove(*local_game);
          auto end_time = std::chrono::steady_clock::now();
          std::chrono::duration<float, std::milli> duration = end_time - start_time;
          if (!local_bot->CancelStatus()) {
            agent_task.duration_ms = duration.count();
            agent_task.status.store(AgentTask::Status::kCompleted);
          }
        })
            .detach();
      }

      if (agent_task_.status.load() == AgentTask::Status::kCompleted) {
        const auto& task = agent_task_;
        if (!game_->PlaceMove(task.move.first, task.move.second, Caro::kMarkComputer)) {
          game_msg_ = "Invalid bot move. The game terminated";
          game_over_ = true;
        }
        auto [movey, movex] = task.move;
        game_msg_ =
            std::format("Computer moved (row: {}, col: {}) calculated in {:.2f} ms. Your turn.\n",
                        movey + 1, movex + 1, task.duration_ms);
        move_history_.emplace_back(movey, movex, false);
        agent_task_.status.store(AgentTask::Status::kIdle);
        player_turn_ = true;
      }
    }

    switch (game_->CheckGameState()) {
      case Caro::GameState::kPlayerWin:
        game_over_ = true;
        game_msg_ = "Congratulations! You win!";
        break;
      case Caro::GameState::kComputerWin:
        game_over_ = true;
        game_msg_ = "Computer wins! Better luck next time.";
        break;
      case Caro::GameState::kDraw:
        game_over_ = true;
        game_msg_ = "It's a draw!";
        break;
      default:
        break;
    }
  }
}

void UiApplication::DrawSettings() {
  if (ImGui::BeginPopupModal("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::SeparatorText("Settings");

    ImGui::Checkbox("Dark Theme", &dark_theme_);
    if (dark_theme_) {
      ImGui::StyleColorsDark();
    } else {
      ImGui::StyleColorsLight();
    }

    ImGui::SliderFloat("Font Size", &font_size_, 10.0f, 24.0f);
    ImGui::GetIO().FontGlobalScale = font_size_ / 11.0f;

    if (ImGui::Button("Close")) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void UiApplication::StartGame(int bot_index, unsigned time_limit, bool player_start) {
  game_ = std::make_shared<Caro>(n_, m_, k_);
  switch (bot_index) {
    case 0:
      bot_ = std::make_shared<AlphaBetaAgent>(time_limit);
      break;
    // case 1:
    // bot = new YBWCAgent(k, time_limit);
    // break;
    default:
      throw std::runtime_error("Invalid bot index. This should never happen");
      break;
  }
  player_turn_ = player_start;
  if (player_turn_) {
    game_msg_ = "Your turn";
  }
  in_game_ = true;
  game_over_ = false;
  show_game_ = true;
  move_history_.clear();
  ResetAgentTask();
}

void UiApplication::ResetGame() {
  if (agent_task_.status.load() == AgentTask::Status::kRunning) {
    if (bot_) bot_->RequestCancel();
  }
  game_msg_ = "";
  game_over_ = false;
  in_game_ = false;
  show_game_ = false;
  move_history_.clear();
}

void UiApplication::ResetAgentTask() {
  agent_task_.move = {0, 0};
  agent_task_.status.store(AgentTask::Status::kIdle);
  agent_task_.duration_ms = 0.0f;
}