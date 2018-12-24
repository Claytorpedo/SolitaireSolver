#include "batchrunner.hpp"

#include <atomic>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <numeric>
#include <mutex>
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
		u64 minSolutionDepth{ std::numeric_limits<u64>::max() };
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
		if (::_mkdir(resultsDir.c_str()) != 0 && errno != EEXIST) {
			std::cerr << "Failed to create results directory.\n";
			return false;
		}
		if (::_mkdir((resultsDir + std::string(SOLUTIONS_SUBFOLDER)).c_str()) != 0 && errno != EEXIST) {
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
		statsFile << "Average solution depth: " << PadWrite(stats.averageSolutionDepth)
			<< " (min: " << PadWrite(stats.minSolutionDepth, ' ', 3) << ", max: " << PadWrite(stats.maxSolutionDepth, ' ', 3) << ")\n";
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
				if (r.solution.size() < stats.minSolutionDepth)
					stats.minSolutionDepth = r.solution.size();
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
	}

	void _print_options(const BatchOptions& options, u32 numSolvers) {
		std::cout << "Running batches with options:\n";
		std::cout << "First seed: " << PadWrite(options.firstSeed);
		if (options.numBatches > 0 && options.seedFilePath.empty())
			std::cout << " (last seed: " << (options.firstSeed + static_cast<u64>(options.batchSize) * options.numBatches - 1) << ")";
		std::cout << "\n";
		std::cout << "Batches:    " << PadWrite(options.numBatches);
		if (options.numBatches == 0)
			std::cout << " (infinite)";
		std::cout << "\n";
		std::cout << "Batch Size: " << PadWrite(options.batchSize) << "\n";
		std::cout << "Max States: " << PadWrite(options.maxStates);
		if (options.maxStates == 0)
			std::cout << "(infinite)";
		std::cout << "\n";
		std::cout << "Solvers:    " << PadWrite(static_cast<u32>(options.numSolvers));
		if (options.numSolvers == 0)
			std::cout << " (deduced to " << numSolvers << ")";
		std::cout << "\n";
		std::cout << "Results directory: " << options.outputDirectory << "\n";
		std::cout << (options.writeGameSolutions ? "Writing out game solutions.\n" : "Not writing out game solutions.\n");

		if (!options.seedFilePath.empty()) {
			std::cout << "Running from seed file: " << options.seedFilePath << "\n";
		}

		std::cout << std::endl;
	}

	void _batch_task(KlondikeSolver& solver, std::mutex& writeMutex, size_t& seedIndex, const std::vector<u64>& seeds, GameResults& workingResults, std::atomic<u32>& seedsRun) {
		size_t seedToRunIndex = 0;
		{
			std::lock_guard<std::mutex> lock(writeMutex);
			seedToRunIndex = seedIndex++;
		}
		while (seedToRunIndex < seeds.size()) {
			solver.setSeed(seeds[seedToRunIndex]);
			auto result = solver.solve();
			++seedsRun;
			{
				std::lock_guard<std::mutex> lock(writeMutex);
				workingResults.emplace_back(std::move(result));
				seedToRunIndex = seedIndex++;
			}
		}
	}
}

bool BatchRunner::run(bool printOptions) {
	if (!_startup(options_.outputDirectory))
		return false;

	const unsigned int numSolvers = options_.numSolvers > 0 ? options_.numSolvers : std::thread::hardware_concurrency();
	const u32 numBatches = options_.numBatches > 0 ? options_.numBatches : std::numeric_limits<u32>::max();

	if (printOptions)
		_print_options(options_, numSolvers);

	std::atomic<u32> seedsRun = 0;
	Threadpool pool(numSolvers);
	std::vector<KlondikeSolver> solvers(numSolvers, options_.maxStates);

	std::vector<std::future<void>> threads;
	threads.reserve(numSolvers);

	std::mutex updateResultsMutex;
	std::vector<GameResult> workingResults, writingResults;

	std::ifstream seedFile;
	if (!options_.seedFilePath.empty()) {
		seedFile.open(options_.seedFilePath);
		if (!seedFile.is_open()) {
			std::cerr << "BatchRunner::run: Failed to open seed file.\n";
			return false;
		}
	}

	u64 batchStartSeed = options_.firstSeed;
	auto populateSeeds = [options = options_, &batchStartSeed, &seedFile] (std::vector<u64>& seeds, bool firstTime = false) {
		seeds.reserve(options.batchSize);
		if (!options.seedFilePath.empty()) {
			size_t i = 0;
			u64 seed;
			if (firstTime) {
				// Find the first seed.
				while (seedFile >> seed) {
					if (seed == options.firstSeed) {
						seeds.push_back(seed);
						++i;
						break;
					}
				}
			}
			while (i++ < options.batchSize && seedFile >> seed)
				seeds.push_back(seed);
		} else {
			const u64 batchEnd = batchStartSeed + options.batchSize;
			while (batchStartSeed < batchEnd)
				seeds.push_back(batchStartSeed++);
		}
	};

	Stats stats;
	stats.startSeed = options_.firstSeed;

	const auto timeStart = Clock::now();

	std::vector<u64> batchSeeds, tempBatchSeeds;

	auto writeResults = [options = options_, timeStart, &stats, &writingResults, &batchSeeds] {
		if (!writingResults.empty()) {
			std::sort(writingResults.begin(), writingResults.end(), [](const auto& lhs, const auto& rhs) { return lhs.seed < rhs.seed; });

			_write_results(writingResults, options.outputDirectory, options.writeGameSolutions);

			_update_stats(writingResults, stats);
			stats.runTime = std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - timeStart);
			_write_stats(options.outputDirectory, stats);

			writingResults.clear();
		}
	};

	populateSeeds(tempBatchSeeds, true);
	for (u32 i = 1; i <= numBatches && !tempBatchSeeds.empty(); ++i) {
		// Initialize data and spawn tasks for solvers.
		size_t seedIndex = 0;
		batchSeeds = std::move(tempBatchSeeds);
		tempBatchSeeds.clear();
		workingResults.reserve(options_.batchSize);
		stats.endSeed = batchSeeds.back();
		for (auto& solver : solvers)
			threads.push_back(pool.add(_batch_task, std::ref(solver), std::ref(updateResultsMutex), std::ref(seedIndex), std::ref(batchSeeds), std::ref(workingResults), std::ref(seedsRun)));

		// Output results, get seeds for next batch.
		writeResults();
		populateSeeds(tempBatchSeeds);

		// Wait for solvers to finish batch.
		while (!pool.isIdle()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			std::cout << "\rSeeds Run: " << PadWrite<u32>(seedsRun);
		}

		for (auto& thread : threads)
			thread.get();
		threads.clear();

		std::cout << "\nBatch " << i << " done. Writing results.\n";
		// Move results so we can spawn new tasks before writing.
		writingResults = std::move(workingResults);
		workingResults.clear();
	}
	writeResults();
	std::cout << "All batches completed.\n";
	std::cout << "Time: " << stats.runTime.count() << " seconds\n";

	return true;
}

bool BatchRunner::writeDecks(bool useNumericCards) const {
	if (!_startup(options_.outputDirectory))
		return false;

	std::ifstream seedFile(options_.seedFilePath);
	if (!seedFile.is_open()) {
		std::cerr << "BatchRunner::writeDecks: Failed to open seed file.\n";
		return false;
	}
	std::ofstream decksFile(options_.outputDirectory + "decks.txt", std::ios::app);
	if (!decksFile.is_open()) {
		std::cerr << "BatchRunner::writeDecks: Failed to open decks output file.\n";
		return false;
	}

	u64 seed;
	while (seedFile >> seed) {
		const Deck deck = GenDeck(seed);
		for (const Card& c : deck) {
			if (useNumericCards)
				decksFile << static_cast<u32>(toUType(c.getSuit()) * CARDS_PER_SUIT + c.getRank());
			else
				decksFile << CardToStr(c) << ",";
			decksFile << " ";
		}
		decksFile << "\n";
	}

	return true;
}
