#include "Deck.h"

#include <random>

#include "Card.h"

namespace Solitaire {
	void Pile::MoveCards(Pile& from, Pile& to, u32 numCards) {
		const auto start(from.deck_.end() - numCards);
		to.deck_.insert(to.deck_.end(), std::make_move_iterator(start), std::make_move_iterator(from.deck_.end()));
		from.deck_.erase(start, from.deck_.end());
	}

	void Pile::MoveCard(Pile& from, s32 fromPosition, Pile& to, s32 toPosition) {
		if (fromPosition < 0)
			fromPosition = from.deck_.size() - 1;
		if (toPosition < 0)
			to.deck_.push_back(from[static_cast<u32>(fromPosition)]);
		else
			to.deck_.insert(to.deck_.begin() + toPosition, from[static_cast<u32>(fromPosition)]);
		from.deck_.erase(from.deck_.begin() + fromPosition);
	}

	Deck GenDeck(u32 deckSeed, u8 numDecks) {
		Suit suit;
		const u32 numCards(numDecks * CARDS_PER_DECK);
		Deck deck;
		deck.reserve(numCards);
		for (u8 i = 0; i < numDecks; ++i) {
			for (u8 s = 0; s < toUType(Suit::TOTAL_SUITS); ++s) {
				suit = static_cast<Suit>(s);
				for (Rank k = 1; k <= CARDS_PER_SUIT; ++k) {
					deck.push_back(Card(suit, k));
				}
			}
		}

		std::mt19937 rng;
		rng.seed(deckSeed);

		for (u32 i = numCards - 1; i > 0; --i) {
			std::uniform_int_distribution<u32> dist(0, i);
			std::swap(deck[i], deck[dist(rng)]);
		}
		return deck;
	}
}
