#pragma once

#include <string>
#include <vector>

#include "units.hpp"
#include "Card.hpp"
#include "KlondikeGame.hpp"

namespace solitaire {
	enum class MoveType {
		TABLEAU,
		TABLEAU_PARTIAL,
		STOCK,
		REPILE_STOCK,
	};

	inline std::string MoveTypeToStr(const MoveType& t) {
		switch (t) {
		case(MoveType::TABLEAU):         return "TABLEAU";
		case(MoveType::TABLEAU_PARTIAL): return "TABLEAU_PARTIAL";
		case(MoveType::STOCK):           return "STOCK";
		case(MoveType::REPILE_STOCK):    return "REPILE_STOCK";
		}
	}

	class Move { // Holds information for a move, as well as what's needed to undo that move.
	public:
		const Card movedCard;
		const PileID fromPile;
		const PileID toPile;
		union {
			const u8 cardsToMove;
			const u8 stockPosition; // If type is STOCK or REPILE_STOCK, indicates pre-move position.
		};
		const u8 stockMovePosition = 0; // If type is STOCK, indicates position in stock to move from.
		const MoveType type;
		const bool flippedCard = false; // Whether the move caused a card to be flipped.

		static Move TableauPartial(const Card& movedCard, PileID fromPile, PileID toPile, u8 cardsToMove); // Move a partial run.
		static Move Tableau(const Card& movedCard, PileID fromPile, PileID toPile, u8 cardsToMove, bool flippedCard);  // Move one or more cards from a tableau pile to another pile.
		static Move Stock(const Card& movedCard, u8 stockPosition, u8 stockMovePosition, PileID toPile); // Move a card from the stock pile to another pile.
		static Move RepileStock(u8 stockPosition); // Repile/reset the stock.

	private:
		explicit Move(const Card& movedCard, PileID fromPile, PileID toPile, u8 cardsToMove, MoveType type, bool flippedCard)
			: movedCard(movedCard), fromPile(fromPile), toPile(toPile), cardsToMove(cardsToMove), type(type), flippedCard(flippedCard) {}
		explicit Move(const Card& movedCard, PileID fromPile, PileID toPile, u8 stockPosition, u8 stockMovePosition, MoveType type)
			: movedCard(movedCard), fromPile(fromPile), toPile(toPile), stockPosition(stockPosition), stockMovePosition(stockMovePosition), type(type) {}
	};

	using MoveList = std::vector<Move>;

	inline std::string MoveToStr(const Move& m) {
		std::string moveStr = MoveTypeToStr(m.type);
		if (m.type != MoveType::REPILE_STOCK) {
			moveStr += " " + CardToStr(m.movedCard);
		}
		return moveStr;
	}
}
