#pragma once

#include <unordered_set>
#include <string>

#include "units.hpp"
#include "Card.hpp"
#include "Deck.hpp"
#include "KlondikeGame.hpp"
#include "Move.hpp"

namespace solitaire {

	struct GameResult {
		enum class Result {
			WIN,
			LOSE,
			UNKNOWN,
		};
		const u64 positionsTried;
		const u64 seed;
		const MoveList solution;
		const Result result;
	};

	class KlondikeSolver {
	public:
		const u64 maxStates = 0; // Max states == 0 -> search until solved.

		KlondikeSolver(u64 seed, u64 maxStates = 0) : game_(seed), maxStates(maxStates)  {};
		KlondikeSolver(KlondikeGame game, u64 maxStates = 0) : game_(game), maxStates(maxStates) {};

		GameResult Solve();

	public:
		static void doMove(KlondikeGame& game, const Move& move);

	private:
		void _init();
		// Is if a card is available for a move.
		bool _is_card_available(const Card& cardToFind) const;
		// Returns true if the game has been won.
		GameResult::Result _solve_recursive(u32 depth);

		void _do_move(const Move& m);
		void _undo_move(const Move& m);

		// Moves that move a full run (flip a tableau card or clear an empty tableau spot).
		void _find_full_run_moves(MoveList& availableMoves);
		// Moves to the foundation.
		void _find_foundation_moves(MoveList& availableMoves);
		// Moves from stock to tableau.
		void _find_stock_moves(MoveList& availableMoves);
		// Moves of partial runs from one tableau pile to another.
		void _find_partial_run_moves(MoveList& availableMoves);

		// Returns true if any auto-moves (guaranteed moves) were found.
		bool _find_auto_moves(MoveList& autoMoves);
		// Returns true if any available moves were found.
		bool _find_available_moves(MoveList& availableMoves);

		bool _is_seen_state();

		KlondikeGame game_;
		MoveList move_sequence_;

		Deck partial_run_move_cards_; // Keeps track of partial run moves, to stop cards from being moved back and forth.

		u64 states_tried_ = 0;
		std::unordered_set<std::string> seen_states_;
	};
}
