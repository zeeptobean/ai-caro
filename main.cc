#include <chrono>
#include <iostream>
#include <sstream>
#include <string>

#include "alpha_beta.h"
#include "caro.h"

int main() {
  namespace chrono = std::chrono;

  Caro game(8, 8, 5);
  auto [m, n] = game.GetBoardSize();

  AlphaBetaAgent bot;

  std::stringstream ss;
  std::string input_str;
  unsigned x, y;

  while (true) {
    game.Display();
    while (true) {
      std::cout << "Enter your move (row and column, 0-indexed): ";
      getline(std::cin, input_str);
      ss.clear();
      ss.str(input_str);
      ss >> x >> y;
      if (!ss || x >= m || y >= n) {
        std::cout << "Invalid input. Please enter valid row and column numbers.\n";
        continue;
      }
      if (!game.PlaceMove(x, y, Caro::kMarkPlayer)) {
        std::cout << "Invalid move. Try again.\n";
        continue;
      }
      break;
    };

    // User's turn

    auto state = game.CheckGameState();
    if (state == Caro::GameState::kPlayerWin) {
      game.Display();
      std::cout << "Congratulations! You win!\n";
      break;
    } else if (state == Caro::GameState::kDraw) {
      game.Display();
      std::cout << "It's a draw!\n";
      break;
    }

    // Computer's turn

    auto start_time = chrono::steady_clock::now();
    auto [comp_x, comp_y] = bot.GetMove(game);
    bool computer_move_status = game.PlaceMove(comp_x, comp_y, bot.GetMark());
    auto end_time = chrono::steady_clock::now();
    chrono::duration<float, std::milli> duration = end_time - start_time;
    std::cout << std::format("Computer move: ({}, {}) calculated in {:.2f} ms\n", comp_x, comp_y,
                             duration.count());
    if (!computer_move_status) {
      std::cout << "Invalid move by computer. This should not happen\n";
      exit(1);
    }

    state = game.CheckGameState();
    if (state == Caro::GameState::kComputerWin) {
      game.Display();
      std::cout << "Computer wins! Better luck next time.\n";
      break;
    } else if (state == Caro::GameState::kDraw) {
      game.Display();
      std::cout << "It's a draw!\n";
      break;
    }
  }
}