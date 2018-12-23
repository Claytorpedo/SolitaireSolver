#pragma once

#include <unordered_set>
#include <memory>
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
		u64 positionsTried;
		u64 seed;
		MoveList solution;
		Result result;
	};
	using GameResults = std::vector<GameResult>;

	class KlondikeSolver {
	public:
		const u64 maxStates = 0; // Max states == 0 -> search until solved.

		KlondikeSolver(u64 maxStates = 0) noexcept : maxStates(maxStates) {};

		GameResult solve();

		// (Re)set the solver with a new seed.
		void setSeed(u64 seed);
		// Set the solver with a game (if in progress, will determine if it is solvable from that point).
		void setGame(const KlondikeGame& game);

	public:
		static void doMove(KlondikeGame& game, const Move& move);

	private:
		struct PriorityMove {
			Move move;
			u32 priority;
		};
		using PriorityMoveList = std::vector<PriorityMove>;

		void _init();
		bool _is_king_available() const;
		bool _is_card_available(const Card& cardToFind) const;
		// Returns true if the game has been won.
		GameResult::Result _solve_recursive(u32 depth);

		void _do_move(const Move& m);
		void _undo_move(const Move& m);

		void _find_full_run_moves(PriorityMoveList& availableMoves);
		void _find_moves_to_foundation(PriorityMoveList& availableMoves);
		void _find_stock_to_tableau_moves(PriorityMoveList& availableMoves);
		void _find_partial_run_moves(PriorityMoveList& availableMoves);

		std::unique_ptr<Move> _find_auto_move();
		// Returns true if any available moves were found.
		PriorityMoveList _find_available_moves();

		bool _is_seen_state();

		KlondikeGame game_;
		MoveList move_sequence_;

		Deck partial_run_move_cards_; // Keeps track of partial run moves, to stop cards from being moved back and forth.

		u64 states_tried_ = 0;
		std::unordered_set<std::string> seen_states_;

		static constexpr u8 UNIQUE_STATE_SIZE = 48;
	};
}
