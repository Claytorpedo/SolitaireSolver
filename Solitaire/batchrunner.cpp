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

using namespace solitaire;

namespace {
	const unsigned int BATCH_SIZE = 10;

	const std::string RESULTS_DIR = "./results/";
	const std::string SOLUTIONS_DIR = RESULTS_DIR + "/solutions/";
	const std::string WIN_FILE = RESULTS_DIR + "winning_seeds.txt";
	const std::string LOSE_FILE = RESULTS_DIR + "losing_seeds.txt";
	const std::string UNKNOWN_FILE = RESULTS_DIR + "unknown_seeds.txt";
	const unsigned int SEED_PADDING = 16; // Zero padding for output to files.

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
		fileName << SOLUTIONS_DIR << std::setfill('0') << std::setw(SEED_PADDING) << result.seed << ".txt";
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
		winFile << std::setfill('0');
		loseFile << std::setfill('0');
		unknownFile << std::setfill('0');

		for (const GameResult& result : results) {
			switch (result.result) {
			case(GameResult::Result::WIN):
				winFile << std::setw(SEED_PADDING) << result.seed << " (positions tried: " << result.positionsTried << ", solution length: " << result.solution.size() << ")\n";
				_write_solution_file(result);
				break;
			case(GameResult::Result::LOSE):
				loseFile << std::setw(SEED_PADDING) << result.seed << " (positions tried: " << result.positionsTried << ")\n";
				break;
			case(GameResult::Result::UNKNOWN):
				unknownFile << std::setw(SEED_PADDING) << result.seed << " (positions tried: " << result.positionsTried << ")\n";
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
	for (u32 i = 0; i < BATCH_SIZE; ++i) {
		KlondikeSolver solver(i, 5000000);
		const GameResult result = solver.Solve();
		results.push_back(result);
		std::cout << "************************************************************************\n";
		std::cout << "Seed: " << result.seed << " is " << (result.result == GameResult::Result::WIN ? "win" : result.result == GameResult::Result::LOSE ? "loss" : "unknown") << "\n";
		if (result.result != GameResult::Result::UNKNOWN) {
			std::cout << "num moves: " << result.positionsTried << "\n";
		}
	}
	_write_results(results);

	return true;
}

/*	WIN SEEDS:
	1
	2
	3
	4
	5
	6
	8
	1336
	11      - 1963757
	21      - 117
	26      - 74


	LOSE SEEDS:
	0
	7       - 139437137
	42      - 68
*/
