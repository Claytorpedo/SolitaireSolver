#pragma once

#include "units.hpp"
#include "Card.hpp"
#include "Deck.hpp"

#include <iostream>
#include <ostream>
#include <vector>

namespace solitaire {
	struct PileID {
		PileType type = PileType::NONE;
		u8 index = 0;
	};

	class KlondikeGame {
	public:
		static constexpr u8 NUM_TABLEAU_PILES = 7;
		static constexpr u8 NUM_FOUNDATION_PILES = static_cast<u8>(Suit::TOTAL_SUITS);
		static constexpr u8 NUM_STOCK_CARD_DRAW = 3; // Number of cards to deal from the stock at a time.

		const u64 seed = 0;

		KlondikeGame(u64 seed) : seed(seed) {}

		void setUpGame();

		Pile&       getPile(const PileID& id);
		const Pile& getPile(const PileID& id) const;

		u8   getStockPosition() const { return stock_position_; }
		void setStockPosition(u8 position) { stock_position_ = position; }

		bool isGameWon() const;
		bool isStockDirty() const; // Whether the stock can be repiled (stock position is not pointing to the first available card).
		void repileStock(); // Equivalent to dealing all of stock to waste, and then back to stock.

		void printGame(std::ostream& output = std::cout) const;

		std::string getUniqueStateID() const;

	public:
		std::vector<Pile> tableau{ NUM_TABLEAU_PILES, Pile(PileType::TABLEAU) };
		std::vector<Pile> foundation{ NUM_FOUNDATION_PILES, Pile(PileType::FOUNDATION) };
		Pile stock{ PileType::STOCK };

	private:
		u8 stock_position_;
	};
}
