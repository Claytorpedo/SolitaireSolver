#include "KlondikeGame.hpp"

#include <iomanip>
#include <sstream>
#include <string>

using namespace solitaire;

namespace {
	const char* const CARD_BACK[] = {
		".---.",
		"|///|",
		"|///|",
		"'---'",
	};

	const char* const CARD_FRONT[] = {
		".---.",
		"|   |",
		"|   |",
		"'---'",
	};

	const char* const CARD_BLANK = "     ";
	const char* const CARD_GAP = "  ";
	const char* const BORDER = "----------------------------------------------------------------\n";

	const u8 CARD_HEIGHT = 4;

	void _concat_pile_str(const Pile& pile, std::string& string) {
		const u8 size = pile.size();
		for (u8 i = 0; i < size; ++i)
			string.push_back(static_cast<char>((toUType(pile[i].getSuit()) * CARDS_PER_SUIT) + pile[i].getRank()));
	}
}

void KlondikeGame::setUpGame() {
	stock = Pile(PileType::STOCK, GenDeck(seed_));
	for (u8 i = 0; i < NUM_TABLEAU_PILES; ++i) {
		Pile::MoveCards(stock, tableau[i], i + 1);
		for (u8 k = 0; k < i; ++k)
			tableau[i][k].flipCard(); // Flip all but the topmost card.
	}
	repileStock();
}

const Pile& KlondikeGame::getPile(const PileID& id) const {
	switch (id.type) {
	case PileType::STOCK:
		return stock;
	case PileType::FOUNDATION:
		return foundation[id.index];
	case PileType::TABLEAU:
		return tableau[id.index];
	default:
		std::cerr << "Error: Invalid pile type for Klondike. (type: " << static_cast<u32>(id.type) << ")";
		return stock;
	}
}

Pile& KlondikeGame::getPile(const PileID& id) {
	return const_cast<Pile&>(std::as_const(*this).getPile(id));
}

bool KlondikeGame::isGameWon() const {
	if (stock.hasCards())
		return false;
	for (u8 i = 0; i < NUM_TABLEAU_PILES; ++i) {
		if (tableau[i].hasCards())
			return false;
	}
	for (u8 i = 0; i < NUM_FOUNDATION_PILES; ++i) {
		if (foundation[i].size() != CARDS_PER_SUIT)
			return false;
		for (Rank k = 0; k < CARDS_PER_SUIT; ++k) {
			const Card& c = foundation[i][k];
			if (c.getRank() != (k + 1) || c.getSuit() != Suit(i))
				return false;
		}
	}
	return true;
}

bool KlondikeGame::isStockDirty() const {
	if (!stock.hasCards())
		return false; // No cards left.
	if (stock_position_ == NUM_STOCK_CARD_DRAW - 1)
		return false; // At default position.
	if (stock_position_ < NUM_STOCK_CARD_DRAW && stock_position_ == stock.size() - 1)
		return false; // Not enough cards left for a single stock draw.
	return true;
}

void KlondikeGame::repileStock() {
	// This can cause overflow when we are out of cards, but is safe because we can never use stock_position_ without checking if the stock is empty anyway.
	stock_position_ = stock.size() < NUM_STOCK_CARD_DRAW ? stock.size() - 1 : NUM_STOCK_CARD_DRAW - 1;
}

u8 KlondikeGame::getNextInStock(u8 fromPosition) const {
	if (static_cast<int>(fromPosition) >= static_cast<int>(stock.size() - 1))
		return stock.size();
	fromPosition += NUM_STOCK_CARD_DRAW;
	return fromPosition < stock.size() ? fromPosition : stock.size() - 1;
}

std::string KlondikeGame::getUniqueStateID() const {
	// Make a unique string id to represent a board state (not human readable).
	std::string id;
	id.reserve(CARDS_PER_DECK + 1);
	id.push_back(isStockDirty() ? '1' : '0');
	for (const Pile& pile : tableau)
		_concat_pile_str(pile, id);
	for (const Pile& pile : foundation)
		_concat_pile_str(pile, id);
	_concat_pile_str(stock, id);
	return id;
}

void KlondikeGame::printGame(std::ostream& output) const {
	output << BORDER;

	// Print stock as a string of entries.
	std::ostringstream stockPositionStr;
	const std::string stockStr = "stock: ";
	output << stockStr;
	stockPositionStr << std::setw(stockStr.size()) << " ";
	for (u8 i = 0; i < stock.size(); ++i) {
		const std::string cardStr = CardToStr(stock[i]) + ", ";
		output << cardStr;
		if (i < stock_position_)
			stockPositionStr << std::setw(cardStr.size()) << " ";
	}
	output << "\n" << stockPositionStr.str() << "^\n";

	// Print foundation.
	for (u8 i = 0; i < CARD_HEIGHT; ++i) {
		for (u8 k = 0; k < NUM_FOUNDATION_PILES; ++k) {
			if (!foundation[k].hasCards()) {
				output << CARD_BLANK << CARD_GAP;
				continue;
			}
			const Card& c = foundation[k][foundation[k].size() - 1];
			if (i == 1 && c.isFaceUp()) {
				output << "|" << CardToStr(c) << "|";
			} else if (c.isFaceUp()) {
				output << CARD_FRONT[i];
			} else {
				output << CARD_BACK[i];
			}
			output << CARD_GAP;
		}
		output << "\n";
	}

	output << "\n\n";

	// Print tableau.
	bool printedSomething = true;
	const u8 halfHeight = static_cast<u8>(CARD_HEIGHT * 0.5f);
	for (u16 i = 0; printedSomething; ++i) {
		const u8 cardIndex = static_cast<u8>(i / halfHeight);
		const u8 cardDrawIndex = static_cast<u8>(i % halfHeight);
		printedSomething = false;
		for (u8 k = 0; k < NUM_TABLEAU_PILES; ++k) {
			if (!tableau[k].hasCards() || tableau[k].size() < cardIndex) {
				output << CARD_BLANK << CARD_GAP;
				continue;
			}
			printedSomething = true;
			if (cardIndex == tableau[k].size()) { // Printing the bottom half of the last card in the pile.
				const Card& c = tableau[k][cardIndex - 1];
				output << (c.isFaceUp() ? CARD_FRONT[cardDrawIndex + halfHeight] : CARD_BACK[cardDrawIndex + halfHeight]);
			} else { // Printing the top half of the current card in the pile.
				const Card& c = tableau[k][cardIndex];
				if (c.isFaceUp()) {
					if (cardDrawIndex == 1)
						output << "|" << CardToStr(c) << "|";
					else
						output << CARD_FRONT[cardDrawIndex];
				} else {
					output << CARD_BACK[cardDrawIndex];
				}
			}
			output << CARD_GAP;
		}
		output << "\n";
	}

	output << BORDER;
}
