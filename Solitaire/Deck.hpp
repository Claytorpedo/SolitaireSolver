#pragma once

#include "units.hpp"
#include "Card.hpp"

#include <vector>
#include <string>

namespace solitaire {
	class Card;

	using Deck = std::vector<Card>;

	Deck GenDeck(u64 deckSeed, u8 numDecks = 1);
	
	enum class PileType {
		NONE,
		FOUNDATION,
		TABLEAU,
		STOCK,
		TOTAL_TYPES
	};

	class Pile {
	public:
		Pile() = default;
		Pile(PileType t) : type_(t) {}
		Pile(PileType t, const Deck& d) : type_(t), deck_(d) {}
		Pile(PileType t, Deck&& d) : type_(t), deck_(std::move(d)) {}

		inline Card& operator[](u8 ind) { return deck_[ind]; }
		inline const Card& operator[](u8 ind) const { return deck_[ind]; }
		inline bool  operator==(const Pile& o) const { return type_ == o.type_ && deck_ == o.deck_; }
		inline bool  hasCards() const { return !deck_.empty(); }
		inline u8    size() const { return static_cast<u8>(deck_.size()); }
		// Get a card starting from the "top" of the pile (topmost card is not overlapped by any other card).
		inline const Card& getFromTop(u8 pos = 0) const {
			return deck_[deck_.size() - (1 + pos)]; 
		}
		// Get a card starting from the "top" of the pile (topmost card is not overlapped by any other card).
		inline Card& getFromTop(u8 pos = 0) {
			return const_cast<Card&>(std::as_const(*this).getFromTop(pos));
		}

		// Move cards from the end of pile "from" to the end of pile "to".
		static void MoveCards(Pile& from, Pile& to, u8 numCards);
		// Move a card from the given position in pile "from" to the given position in pile "to".
		// If the given position is < 0, moves from/to the end of that pile.
		static void MoveCard(Pile& from, s32 fromPosition, Pile& to, s32 toPosition=-1);

	private:
		Deck deck_;
		PileType type_;
	};
}
