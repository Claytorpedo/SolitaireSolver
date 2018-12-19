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
	constexpr std::string_view SOLUTIONS_SUBFOLDER = "/solutions/";

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

	constexpr u64 _unsigned_ceil(float f) noexcept {
		u64 n = static_cast<u64>(f);
		return f == static_cast<float>(n) ? n : n + 1;
	}

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

	bool _startup(const std::string& resultsDir) {
		if (_mkdir(resultsDir.c_str()) != 0 && errno != EEXIST) {
			std::cerr << "Failed to create results directory.\n";
			return false;
		}
		if (_mkdir((resultsDir + std::string(SOLUTIONS_SUBFOLDER)).c_str()) != 0 && errno != EEXIST) {
			std::cerr << "Failed to create solutions directory.\n";
			return false;
		}
		return true;
	}

	void _write_solution_file(std::string_view resultsDir, const GameResult& result) {
		std::stringstream fileName;
		fileName << resultsDir << SOLUTIONS_SUBFOLDER << PadWrite(result.seed) << ".txt";
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

	void _write_stats(const std::string& resultsDir, const Stats& stats) {
		std::ofstream statsFile(resultsDir + "stats.txt", std::ios::app);
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

	void _write_results(const std::vector<GameResult>& results, const std::string& resultsDir, bool writeSolutions) {
		std::ofstream winFile(resultsDir + "winning_seeds.txt", std::ios::app);
		std::ofstream loseFile(resultsDir + "losing_seeds.txt", std::ios::app);
		std::ofstream unknownFile(resultsDir + "unknown_seeds.txt", std::ios::app);

		for (const GameResult& result : results) {
			switch (result.result) {
			case(GameResult::Result::WIN):
				winFile << PadWrite(result.seed, '0') << " (positions tried: " << PadWrite(result.positionsTried) << ", solution length: " << PadWrite(result.solution.size()) << ")\n";
				if (writeSolutions)
					_write_solution_file(resultsDir, result);
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

	std::vector<GameResult> _batch_task(KlondikeSolver& solver, u64 startSeed, u64 numSeeds, std::atomic<u32>& seedsRun) {
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

bool BatchRunner::run() {
	if (!_startup(options_.outputDirectory))
		return false;

	const u64 seedsPerTask = _unsigned_ceil(options_.batchSize / static_cast<float>(options_.numSolvers));

	std::atomic<u32> seedsRun = 0;
	Threadpool pool(options_.numSolvers);
	std::vector<KlondikeSolver> solvers(options_.numSolvers, options_.maxStates);

	std::vector<std::future<std::vector<GameResult>>> allResults;
	allResults.reserve(options_.numSolvers);

	Stats stats;
	stats.endSeed = stats.startSeed = options_.firstSeed;

	u64 startSeed = options_.firstSeed;
	const auto timeStart = Clock::now();
	for (u32 i = 0; i < options_.numBatches; ++i) {
		for (auto& solver : solvers) {
			allResults.push_back(pool.add(_batch_task, std::ref(solver), startSeed, seedsPerTask, std::ref(seedsRun)));
			startSeed += seedsPerTask;
		}

		while (!pool.isIdle()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			std::cout << "\rSeeds Run: " << PadWrite<u32>(seedsRun);
		}

		std::cout << "\nBatch done. Writing results.\n";

		for (auto& results : allResults) {
			auto gameResults = results.get();
			_write_results(gameResults, options_.outputDirectory, options_.writeGameSolutions);
			_update_stats(gameResults, stats);
		}
		stats.runTime = std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - timeStart);
		_write_stats(options_.outputDirectory, stats);

		allResults.clear();
	}
	std::cout << "All batches completed.\n";
	std::cout << "Time: " << stats.runTime.count() << " seconds\n";

	return true;
}
