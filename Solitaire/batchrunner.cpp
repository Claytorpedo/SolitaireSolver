#include "batchrunner.hpp"

#include <atomic>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <thread>
#include <chrono>

#include "KlondikeSolver.hpp"
#include "threadpool/threadpool/Threadpool.hpp"

#include <direct.h>
#include <errno.h>

using Clock = std::chrono::high_resolution_clock;

using namespace solitaire;

namespace {
	std::atomic<u64> seedsRun = 0;

	const unsigned int BATCH_SIZE = 10000;
	const u64 MAX_STATES = 5000000;
	const bool WRITE_GAMES = false;

	const std::string RESULTS_DIR = "./results/";
	const std::string SOLUTIONS_DIR = RESULTS_DIR + "/solutions/";
	const std::string WIN_FILE = RESULTS_DIR + "winning_seeds.txt";
	const std::string LOSE_FILE = RESULTS_DIR + "losing_seeds.txt";
	const std::string UNKNOWN_FILE = RESULTS_DIR + "unknown_seeds.txt";

	constexpr Threadpool::thread_num MAX_THREADS = 8;

	Threadpool pool(MAX_THREADS);
	std::vector<KlondikeSolver> solvers(MAX_THREADS, MAX_STATES);

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
				if (WRITE_GAMES)
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

	std::vector<GameResult> _batch_task(KlondikeSolver& solver, u64 startSeed, u64 numSeeds) {
		std::vector<GameResult> results;
		results.reserve(static_cast<int>(numSeeds));

		const u64 seedEnd = startSeed + numSeeds;
		for (u64 i = startSeed; i < seedEnd; ++i) {
			solver.setSeed(i);
			results.push_back(solver.solve());
			++seedsRun;
		}
		return results;
	}
}

bool batchrunner::run() {
	if (!_startup())
		return false;

	std::vector<std::future<std::vector<GameResult>>> allResults;

	const auto timeStart = Clock::now();
	u64 startSeed = 0;
	u64 seedsPerTask = BATCH_SIZE / solvers.size() + (BATCH_SIZE % solvers.size() != 0); // Round up.
	for (auto& solver : solvers) {
		allResults.push_back(pool.add(_batch_task, solver, startSeed, seedsPerTask));
		startSeed += seedsPerTask;
	}

	while (!pool.isIdle()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		std::cout << "\rSeeds Run: " << PadWrite<u64>(seedsRun, ' ');
	}
	std::cout << "\nDone. Writing results.\n";

	for (auto& resultList : allResults)
		_write_results(resultList.get());

	const auto timeEnd = Clock::now();
	std::cout << "Time: " << std::chrono::duration_cast<std::chrono::seconds>(timeEnd - timeStart).count() << " seconds\n";

	return true;
}
