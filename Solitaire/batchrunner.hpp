#pragma once

#include "units.hpp"

#include <optional>
#include <string_view>

// Batch runner for Solitaire Klondike. Runs batches of games and writes out results to disk.

namespace solitaire {
	struct BatchOptions {
		u64 firstSeed{ 0 };
		u32 numBatches{ 10 };
		u32 batchSize{ 100 };
		u64 maxStates{ 1000000 };
		u8 numSolvers{ 4 };

		bool writeGameSolutions{ false };
		std::string outputDirectory{ "./results/" };
	};

	class BatchRunner {
	public:
		BatchRunner() = default;
		BatchRunner(BatchOptions options) : options_(std::move(options)) {}

		BatchOptions getOptions() const { return options_; }
		void         setOptions(BatchOptions options) { options_ = std::move(options); }
		// Returns false if there is an error.
		bool         run(std::optional<std::string> seedFilePath = std::nullopt, bool printOptions = true);
		bool         writeDecks(const std::string& seedFilePath) const;

	private:
		BatchOptions options_;
	};
}
