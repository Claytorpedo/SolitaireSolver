#include "batchrunner.hpp"

#include <iomanip>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>

#include "KlondikeSolver.hpp"

#include <direct.h>
#include <errno.h>
#include <chrono>
using Clock = std::chrono::high_resolution_clock;

using namespace solitaire;

namespace {
	const unsigned int BATCH_SIZE = 100;

	const std::string RESULTS_DIR = "./results/";
	const std::string SOLUTIONS_DIR = RESULTS_DIR + "/solutions/";
	const std::string WIN_FILE = RESULTS_DIR + "winning_seeds.txt";
	const std::string LOSE_FILE = RESULTS_DIR + "losing_seeds.txt";
	const std::string UNKNOWN_FILE = RESULTS_DIR + "unknown_seeds.txt";

	// Pad input to a given width.
	template<typename T>
	struct PadWrite {
		PadWrite(T n, char fill = '0', unsigned int padding = 16) : n(n), fill(fill), padding(padding) {}
		friend inline std::ostream& operator<<(std::ostream& stream, const PadWrite& writer) {
			stream << std::setfill(writer.fill) << std::setw(writer.padding) << writer.n;
			return stream;
		}
		T n;
		char fill;
		unsigned int padding;
	};

	bool _startup() {
		if (_mkdir(RESULTS_DIR.c_str()) != 0 && errno != EEXIST) {
			std::cerr << "Failed to create results directory.\n";
			return false;
		}
		if (_mkdir(SOLUTIONS_DIR.c_str()) != 0 && errno != EEXIST) {
			std::cerr << "Failed to create solutions directory.\n";
			return false;
		}
		return true;
	}

	void _write_solution_file(const GameResult& result) {
		std::stringstream fileName;
		fileName << SOLUTIONS_DIR << PadWrite(result.seed) << ".txt";
		std::ofstream solutionFile(fileName.str(), std::ios::trunc);

		// Print off moves list.
		for (const Move& move : result.solution) {
			solutionFile << MoveToStr(move) << " ";
		}
		solutionFile << "\n\n";

		KlondikeGame game(result.seed);
		game.setUpGame();

		game.printGame(solutionFile);

		for (const Move& move : result.solution) {
			KlondikeSolver::doMove(game, move);
			game.printGame(solutionFile);
			solutionFile << MoveToStr(move) << "\n";
		}
	}

	void _write_results(const std::vector<GameResult>& results) {
		std::ofstream winFile(WIN_FILE, std::ios::app);
		std::ofstream loseFile(LOSE_FILE, std::ios::app);
		std::ofstream unknownFile(UNKNOWN_FILE, std::ios::app);

		for (const GameResult& result : results) {
			switch (result.result) {
			case(GameResult::Result::WIN):
				winFile << PadWrite(result.seed) << " (positions tried: " << PadWrite(result.positionsTried, ' ') << ", solution length: " << PadWrite(result.solution.size(), ' ') << ")\n";
				_write_solution_file(result);
				break;
			case(GameResult::Result::LOSE):
				loseFile << PadWrite(result.seed) << " (positions tried: " << PadWrite(result.positionsTried, ' ') << ")\n";
				break;
			case(GameResult::Result::UNKNOWN):
				unknownFile << PadWrite(result.seed) << " (positions tried: " << PadWrite(result.positionsTried, ' ') << ")\n";
				break;
			}
		}
	}
}

bool batchrunner::run() {
	if (!_startup())
		return false;

	std::vector<GameResult> results;
	results.reserve(BATCH_SIZE);
	KlondikeSolver solver(5000000);

	const auto timeStart = Clock::now();
	for (u32 i = 0; i < BATCH_SIZE; ++i) {
		solver.setSeed(i);
		const GameResult result = solver.solve();
		results.push_back(result);
		std::cout << "************************************************************************\n";
		std::cout << "Seed: " << result.seed << " is " << (result.result == GameResult::Result::WIN ? "win" : result.result == GameResult::Result::LOSE ? "loss" : "unknown") << "\n";
		if (result.result != GameResult::Result::UNKNOWN) {
			std::cout << "num moves: " << result.positionsTried << "\n";
		}
	}
	_write_results(results);
	const auto timeEnd = Clock::now();
	std::cout << "Time: " << std::chrono::duration_cast<std::chrono::nanoseconds>(timeEnd - timeStart).count() << " nanos\n";
	return true;
}
