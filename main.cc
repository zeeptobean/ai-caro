#include <chrono>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "alpha_beta.h"
#include "caro.h"
#include "ybwc.h"

int main() {
  namespace chrono = std::chrono;

  Caro game(6, 6, 5);
  auto [m, n] = game.GetBoardSize();

  // std::unique_ptr<Agent> bot = std::make_unique<YBWCAgent>(4, 2, 4);
  std::unique_ptr<Agent> bot = std::make_unique<AlphaBetaAgent>();

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