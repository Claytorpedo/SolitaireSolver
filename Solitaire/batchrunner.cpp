#include "batchrunner.hpp"

#include <atomic>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <numeric>
#include <string>
#include <sstream>
#include <vector>
#include <chrono>

#include "KlondikeSolver.hpp"
#include "threadpool/threadpool/Threadpool.hpp"

#include <direct.h>
#include <errno.h>

using Clock = std::chrono::high_resolution_clock;

using namespace solitaire;

namespace {

	constexpr u64 FIRST_SEED = 300000;

	constexpr u32 NUM_BATCHES = 100;
	constexpr u32 BATCH_SIZE = 1000;
	constexpr u64 MAX_STATES = 10000000;
	constexpr bool WRITE_GAMES = false;

	const std::string RESULTS_DIR = "./results/";
	const std::string SOLUTIONS_DIR = RESULTS_DIR + "/solutions/";
	const std::string WIN_FILE = RESULTS_DIR + "winning_seeds.txt";
	const std::string LOSE_FILE = RESULTS_DIR + "losing_seeds.txt";
	const std::string UNKNOWN_FILE = RESULTS_DIR + "unknown_seeds.txt";
	const std::string STATS_FILE = RESULTS_DIR + "stats.txt";

	constexpr Threadpool::thread_num MAX_THREADS = 8;
	constexpr auto NUM_SOLVERS = MAX_THREADS;
	constexpr u64 unsigned_ceil(float f) {
		u64 n = static_cast<u64>(f);
		return f == static_cast<float>(n) ? n : n + 1;
	}
	constexpr u64 SEEDS_PER_TASK = unsigned_ceil(BATCH_SIZE / static_cast<float>(NUM_SOLVERS));

	std::atomic<u64> s_seedsRun = 0;
	Threadpool s_pool(MAX_THREADS);
	std::vector<KlondikeSolver> s_solvers(NUM_SOLVERS, MAX_STATES);

	struct Stats {
		u64 startSeed{ 0 };
		u64 endSeed{ 0 };
		u64 totalGames{ 0 };
		u64 wins{ 0 };
		u64 losses{ 0 };
		u64 unknown{ 0 };
		float completedGamesAveragePositionsTried{ 0 };
		float wonGamesAveragePositionsTried{ 0 };
		float lostGamesAveragePositionsTried{ 0 };
		float averageSolutionDepth{ 0 };
		u64 maxSolutionDepth{ 0 };
		std::chrono::seconds runTime{ 0 };
	};

	// Pad input to a given width.
	template<typename T>
	struct PadWrite {
		PadWrite(T n, char fill = ' ', u32 padding = 10, u32 precision = 2) : n(n), fill(fill), padding(padding), precision(precision) {}
		friend inline std::ostream& operator<<(std::ostream& stream, const PadWrite& writer) {
			stream << std::fixed << std::setfill(writer.fill) << std::setw(writer.padding) << std::setprecision(writer.precision) << writer.n;
			return stream;
		}
		T n;
		char fill;
		u32 padding;
		u32 precision;
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

	void _write_stats(const Stats& stats) {
		std::ofstream statsFile(STATS_FILE, std::ios::app);
		statsFile << "Ran from seed    " << PadWrite(stats.startSeed) << " to seed " << PadWrite(stats.endSeed) << "\n";
		statsFile << "Total games run: " << PadWrite(stats.totalGames) << "\n";
		statsFile << "Wins:            " << PadWrite(stats.wins) << " (" << PadWrite(stats.wins / static_cast<float>(stats.totalGames) * 100, ' ', 2) << "%)\n";
		statsFile << "Losses:          " << PadWrite(stats.losses) << " (" << PadWrite(stats.losses / static_cast<float>(stats.totalGames) * 100, ' ', 2) << "%)\n";
		statsFile << "Unsolved:        " << PadWrite(stats.unknown) << " (" << PadWrite(stats.unknown / static_cast<float>(stats.totalGames) * 100, ' ', 2) << "%)\n";
		statsFile << "Solved games:    " << PadWrite((stats.wins + stats.losses) / static_cast<float>(stats.totalGames) * 100, ' ', 2) << "%\n";
		statsFile << "Average positions tried for wins:            " << PadWrite(stats.wonGamesAveragePositionsTried) << "\n";
		statsFile << "Average positions tried for losses:          " << PadWrite(stats.lostGamesAveragePositionsTried) << "\n";
		statsFile << "Average positions tried for completed games: " << PadWrite(stats.completedGamesAveragePositionsTried) << "\n";
		statsFile << "Average solution depth: " << PadWrite(stats.averageSolutionDepth) << "\n";
		statsFile << "Total run time: " << PadWrite(stats.runTime.count()) << "s\n";

		statsFile << "********\n\n";
	}

	void _write_results(const std::vector<GameResult>& results) {
		std::ofstream winFile(WIN_FILE, std::ios::app);
		std::ofstream loseFile(LOSE_FILE, std::ios::app);
		std::ofstream unknownFile(UNKNOWN_FILE, std::ios::app);

		for (const GameResult& result : results) {
			switch (result.result) {
			case(GameResult::Result::WIN):
				winFile << PadWrite(result.seed, '0') << " (positions tried: " << PadWrite(result.positionsTried) << ", solution length: " << PadWrite(result.solution.size()) << ")\n";
				if (WRITE_GAMES)
					_write_solution_file(result);
				break;
			case(GameResult::Result::LOSE):
				loseFile << PadWrite(result.seed, '0') << " (positions tried: " << PadWrite(result.positionsTried) << ")\n";
				break;
			case(GameResult::Result::UNKNOWN):
				unknownFile << PadWrite(result.seed, '0') << " (positions tried: " << PadWrite(result.positionsTried) << ")\n";
				break;
			}
		}
	}

	void _update_stats(const std::vector<GameResult>& results, Stats& stats) {
		u64 wins{ 0 }, losses{ 0 }, unknown{ 0 };
		u64 winPositions{ 0 }, lossPositions{ 0 };
		u64 solutionLengths{ 0 };
		for (const auto& r : results) {
			switch (r.result) {
			case(GameResult::Result::WIN):
				++wins;
				winPositions += r.positionsTried;
				solutionLengths += r.solution.size();
				if (r.solution.size() > stats.maxSolutionDepth)
					stats.maxSolutionDepth = r.solution.size();
				break;
			case(GameResult::Result::LOSE):
				++losses;
				lossPositions += r.positionsTried;
				break;
			case(GameResult::Result::UNKNOWN):
				++unknown;
				break;
			}
		}
		const auto allWins = stats.wins + wins;
		stats.wonGamesAveragePositionsTried = allWins == 0 ? 0 : (stats.wonGamesAveragePositionsTried * stats.wins + winPositions) / allWins;
		const auto allLosses = stats.losses + losses;
		stats.lostGamesAveragePositionsTried = allLosses == 0 ? 0 : (stats.lostGamesAveragePositionsTried * stats.losses + lossPositions) / allLosses;
		const auto totalCompletedGames = allWins + allLosses;
		stats.completedGamesAveragePositionsTried = totalCompletedGames == 0 ? 0 : (stats.completedGamesAveragePositionsTried * (stats.wins + stats.losses) + winPositions + lossPositions) / totalCompletedGames;

		stats.averageSolutionDepth = allWins == 0 ? 0 : (stats.averageSolutionDepth * stats.wins + solutionLengths) / allWins;

		stats.totalGames += results.size();
		stats.wins += wins;
		stats.losses += losses;
		stats.unknown += unknown;
		stats.endSeed += results.size();
	}

	std::vector<GameResult> _batch_task(KlondikeSolver& solver, u64 startSeed, u64 numSeeds) {
		std::vector<GameResult> results;
		results.reserve(static_cast<int>(numSeeds));

		const u64 seedEnd = startSeed + numSeeds;
		for (u64 i = startSeed; i < seedEnd; ++i) {
			solver.setSeed(i);
			results.push_back(solver.solve());
			++s_seedsRun;
		}
		return results;
	}
}

bool batchrunner::run() {
	if (!_startup())
		return false;

	std::vector<std::future<std::vector<GameResult>>> allResults;
	allResults.reserve(NUM_SOLVERS);

	Stats stats;
	stats.startSeed = FIRST_SEED;
	stats.endSeed = FIRST_SEED;
	const auto timeStart = Clock::now();
	u64 startSeed = FIRST_SEED;
	for (u32 i = 0; i < NUM_BATCHES; ++i) {
		for (auto& solver : s_solvers) {
			allResults.push_back(s_pool.add(_batch_task, solver, startSeed, SEEDS_PER_TASK));
			startSeed += SEEDS_PER_TASK;
		}

		while (!s_pool.isIdle()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			std::cout << "\rSeeds Run: " << PadWrite<u64>(s_seedsRun);
		}

		std::cout << "\nBatch done. Writing results.\n";

		for (auto& results : allResults) {
			auto gameResults = results.get();
			_write_results(gameResults);
			_update_stats(gameResults, stats);
		}
		stats.runTime = std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - timeStart);
		_write_stats(stats);

		allResults.clear();
	}
	std::cout << "All batches completed.\n";
	std::cout << "Time: " << stats.runTime.count() << " seconds\n";

	return true;
}
