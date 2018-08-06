#pragma once

// Batch runner for Solitaire Klondike. Runs batches of games and writes out results to disk.

namespace solitaire::batchrunner {
	bool run(); // Run Solitaire solver batch runner. Returns false if there is an error.
}
