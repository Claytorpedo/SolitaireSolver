#pragma once

#include <vector>

#include "units.h"
#include "Card.h"
#include "Deck.h"

namespace Solitaire {

	struct PileID {
		PileType type = PileType::NONE;
		u8 index = 0;
	};

	enum class MoveType {
		TABLEAU,
		TABLEAU_PARTIAL,
		STOCK,
		REPILE_STOCK,
	};

	class Move {
	public:
		const Card movedCard;
		const PileID fromPile;
		const PileID toPile;
		union {
			const u8 cardsToMove;
			const u8 stockPosition; // If STOCK_MOVE or SHUFFLE_STOCK, indicates pre-move position.
		};
		const u8 stockMovePosition = 0; // If STOCK_MOVE, indicates position in stock to move from.
		const MoveType type;
		const bool flippedCard = false; // Whether the move caused a card to be flipped.

		static Move TableauPartial(const Card& movedCard, PileID fromPile, PileID toPile, u8 cardsToMove); // Move a partial run.
		static Move Tableau(const Card& movedCard, PileID fromPile, PileID toPile, u8 cardsToMove, bool flippedCard);  // Move one or more cards from a tableau pile to another pile.
		static Move Stock(const Card& movedCard, u8 stockPosition, u8 stockMovePosition, PileID toPile); // Move a card from the stock pile to another pile.
		static Move RepileStock(const Card& movedCard, u8 stockPosition); // Re-pile/reset the stock.

	private:
		explicit Move(const Card& movedCard, PileID fromPile, PileID toPile, u8 cardsToMove, MoveType type, bool flippedCard)
			: movedCard(movedCard),fromPile(fromPile), toPile(toPile), cardsToMove(cardsToMove), type(type), flippedCard(flippedCard) {}
		explicit Move(const Card& movedCard, PileID fromPile, PileID toPile, u8 stockPosition, u8 stockMovePosition, MoveType type)
			: movedCard(movedCard), fromPile(fromPile), toPile(toPile), stockPosition(stockPosition), stockMovePosition(stockMovePosition), type(type) {}
	};

	class Move;
	using MoveList = std::vector<Move>;

	struct KlondikeGame {
		static constexpr u8 NUM_TABLEAU_PILES = 7;
		static constexpr u8 NUM_FOUNDATION_PILES = static_cast<u8>(Suit::TOTAL_SUITS);
		static constexpr u8 NUM_STOCK_CARD_DRAW = 3; // Number of cards to deal from the stock at a time.

		const u64 seed = 0;

		std::vector<Pile> tableau{ NUM_TABLEAU_PILES, Pile(PileType::TABLEAU) };
		std::vector<Pile> foundation{ NUM_FOUNDATION_PILES, Pile(PileType::FOUNDATION) };
		Pile stock{PileType::STOCK};

		KlondikeGame(u64 seed) : seed(seed) {}

		void setUpGame();
		void printGame() const;
		Pile& getPile(const PileID& id);
		const Pile& getPile(const PileID& id) const;
		bool isGameWon() const;
	};

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

	private:
		void _init();
		// Equivalent to dealing all of stock to waste, and then back to stock.
		void _repile_stock();
		// Returns true if the stock position is not at the first available card in the stock pile.
		bool _is_stock_dirty() const;
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

		KlondikeGame game_;
		u8 stock_position_;
		MoveList move_sequence_;

		Deck partial_run_move_cards_; // Keeps track of partial run moves, to stop cards from being moved back and forth.

		u64 states_tried_ = 0;
	};
}
