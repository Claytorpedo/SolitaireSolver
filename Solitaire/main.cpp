#include "CmdParser/CmdParser.hpp"
#include "batchrunner.hpp"

int main(int argc, const char* argv[]) {
	using namespace solitaire;
	BatchOptions options;
	bool showHelp;
	cmd::CmdParser parser;
	parser.pushFlag(showHelp, '?', "help", false, "Prints this help message.");
	parser.push(options.firstSeed, 'f', "first", u64{ 0 }, "The seed to start from.");
	parser.push(options.numBatches, 'n', "num-batches", u32{ 100 }, "How many batches to run. Output files are updated between batches. 0 for infinite.");
	parser.push(options.batchSize, 'b', "batch-size", u32{ 1000 }, "How many seeds to run per batch.");
	parser.push(options.maxStates, 's', "max-states", solitaire::u64{ 10'000'000 }, "Maximum number of states to try before giving up. 0 for infinite. Correlates to ram usage.");
	parser.push(options.numSolvers, 't', "num-solvers", u8{ 0 }, "How many solvers to run. Solvers run on separate threads. 0 to auto-deduce.");
	parser.pushFlag(options.writeGameSolutions, std::nullopt, "write-game-solutions", false, "Write out the winning game solutions to files.");
	parser.push(options.outputDirectory, 'o', "output-dir", "./results/", "Relative path to save output to.");
	parser.push(options.seedFilePath, 'F', "seed-file", "", "Relative path to seed file. If set, searches for first seed and starts from there.");

	bool writeDecks;
	parser.pushFlag(writeDecks, std::nullopt, "write-decks", false, "Generate decks for all seeds in a seed file, and write them out to a deck file.");

	constexpr std::string_view description = "Solitaire Solver:\nAttempts to determine if Klondike games are winnable or not.";
	if (!parser.parse(argc, argv) || showHelp) {
		parser.printHelp(description);
		return 1;
	} else if (writeDecks && options.seedFilePath.empty()) {
		std::cerr << "Seed file must be set to write decks.\n";
		parser.printHelp(description);
		return 1;
	}

	solitaire::BatchRunner batchRunner(options);
	if (writeDecks) {
		return batchRunner.writeDecks() ? 0 : 1;
	}

	return batchRunner.run() ? 0 : 1;
}
