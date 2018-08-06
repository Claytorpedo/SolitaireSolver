#include "KlondikeSolver.h"

#include <algorithm>
#include <climits>
#include <iostream>
#include <string>

#include "units.h"

using namespace Solitaire;

// ---------------------------------- Move ----------------------------------
Move Move::TableauPartial(const Card& movedCard, PileID fromPile, PileID toPile, u8 cardsToMove) {
	return Move(movedCard, fromPile, toPile, cardsToMove, MoveType::TABLEAU_PARTIAL, false);
}
Move Move::Tableau(const Card& movedCard, PileID fromPile, PileID toPile, u8 cardsToMove, bool flippedCard) {
	return Move(movedCard, fromPile, toPile, cardsToMove, MoveType::TABLEAU, flippedCard);
}
Move Move::Stock(const Card& movedCard, u8 stockPosition, u8 stockMovePosition, PileID toPile) {
	return Move(movedCard, PileID{ PileType::STOCK }, toPile, stockPosition, stockMovePosition, MoveType::STOCK);
}
Move Move::RepileStock(u8 stockPosition) {
	return Move(Card{}, PileID{}, PileID{}, stockPosition, 0, MoveType::REPILE_STOCK);
}

// ------------------------------ Klondike Game ------------------------------
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
	const char* const CARD_GAP   = "  ";
	const char* const BORDER     = "----------------------------------------------------------------\n";

	const Solitaire::u8 CARD_HEIGHT = 4;

	void _concat_pile_str(const Pile& pile, std::string& string ) {
		const u8 size = pile.size();
		for (u8 i = 0; i < size; ++i)
			string.push_back(static_cast<char>((toUType(pile[i].getSuit()) * CARDS_PER_SUIT) + pile[i].getRank()));
	}
}

void KlondikeGame::setUpGame() {
	stock = Pile(PileType::STOCK, GenDeck(seed));
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
	if (stock_position_ == KlondikeGame::NUM_STOCK_CARD_DRAW - 1)
		return false; // At default position.
	if (stock_position_ < KlondikeGame::NUM_STOCK_CARD_DRAW && stock_position_ == stock.size() - 1)
		return false; // Not enough cards left for a single stock draw.
	return true;
}

void KlondikeGame::repileStock() {
	// This can cause overflow when we are out of cards, but is safe because we can never use stock_position_ without checking if the stock is empty anyway.
	stock_position_ = stock.size() < KlondikeGame::NUM_STOCK_CARD_DRAW ? stock.size() - 1 : KlondikeGame::NUM_STOCK_CARD_DRAW - 1;
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

void KlondikeGame::printGame() const {
	std::cout << BORDER;

	// Print stock as a string of entries.
	std::cout << "stock: ";
	for (u8 i = 0; i < stock.size(); ++i)
		std::cout << RankToStr(stock[i].getRank()) << SuitToChar(stock[i].getSuit()) << ", ";
	std::cout << "\n";

	// Print foundation.
	for (u8 i = 0; i < CARD_HEIGHT; ++i) {
		for (u8 k = 0; k < NUM_FOUNDATION_PILES; ++k) {
			if (!foundation[k].hasCards()) {
				std::cout << CARD_BLANK << CARD_GAP;
				continue;
			}
			const Card& c = foundation[k][foundation[k].size() - 1];
			if (i == 1 && c.isFaceUp()) {
				std::cout << "|" << RankToStr(c.getRank()) << SuitToChar(c.getSuit()) << "|";
			} else if (c.isFaceUp()) {
				std::cout << CARD_FRONT[i];
			} else {
				std::cout << CARD_BACK[i];
			}
			std::cout << CARD_GAP;
		}
		std::cout << "\n";
	}

	std::cout << "\n\n";

	// Print tableau.
	bool printedSomething = true;
	const u8 halfHeight = static_cast<u8>(CARD_HEIGHT * 0.5f);
	for (u16 i = 0; printedSomething; ++i) {
		const u8 cardIndex = static_cast<u8>(i / halfHeight);
		const u8 cardDrawIndex = static_cast<u8>(i % halfHeight);
		printedSomething = false;
		for (u8 k = 0; k < NUM_TABLEAU_PILES; ++k) {
			if (!tableau[k].hasCards() || tableau[k].size() < cardIndex) {
				std::cout << CARD_BLANK << CARD_GAP;
				continue;
			}
			printedSomething = true;
			if (cardIndex == tableau[k].size()) { // Printing the bottom half of the last card in the pile.
				const Card& c = tableau[k][cardIndex - 1];
				std::cout << (c.isFaceUp() ? CARD_FRONT[cardDrawIndex + halfHeight] : CARD_BACK[cardDrawIndex + halfHeight]);
			} else { // Printing the top half of the current card in the pile.
				const Card& c = tableau[k][cardIndex];
				if (c.isFaceUp()) {
					if (cardDrawIndex == 1)
						std::cout << "|" << RankToStr(c.getRank()) << SuitToChar(c.getSuit()) << "|";
					else
						std::cout << CARD_FRONT[cardDrawIndex];
				} else {
					std::cout << CARD_BACK[cardDrawIndex];
				}
			}
			std::cout << CARD_GAP;
		}
		std::cout << "\n";
	}

	std::cout << BORDER;
}

// ----------------------------- Klondike Solver -----------------------------
namespace {
	bool _can_place_card(const Card& place, const Card& other) {
		return IsRed(place.getSuit()) != IsRed(other.getSuit()) && place.getRank() == other.getRank() - 1;
	}
	bool _can_move_to_foundation(const Card& card, const std::vector<Pile>& foundation) {
		const Pile& pile = foundation[toUType(card.getSuit())];
		if (!pile.hasCards())
			return card.getRank() == 1;
		return pile.getFromTop().getRank() == card.getRank() - 1;
	}

	// If the card can be moved to the foundation immediately without impacting chances of game success.
	bool _guaranteed_move_to_foundation(const Card& card, const std::vector<Pile>& foundation) {
		Rank minRank;
		if (IsRed(card.getSuit())) // Check black foundations.
			minRank = static_cast<Rank>(std::min(foundation[toUType(Suit::CLUBS)].size(), foundation[toUType(Suit::SPADES)].size()));
		else // Check red foundations.
			minRank = static_cast<Rank>(std::min(foundation[toUType(Suit::HEARTS)].size(), foundation[toUType(Suit::DIAMONDS)].size()));
		return _can_move_to_foundation(card, foundation) && card.getRank() <= minRank + 2;
	}
	// Find first face-up card for the pile. Returns whether a run was found (false if pile has no cards).
	bool _find_top_of_run(const Pile& pile, u8& out_run_length, const Card** optional_out_card=nullptr) {
		for (u8 i = 0; i < pile.size(); ++i) {
			if (pile[i].isFaceUp()) {
				if (optional_out_card)
					*optional_out_card = &pile[i];
				out_run_length = pile.size() - i;
				return true;
			}
		}
		return false; // Pile is empty.
	}
	// Find the first available spot to move a card to, if it exists.
	bool _find_tableau_to_tableau_move(const Card& card, const std::vector<Pile>& tableau, u8 fromTableau, u8& out_to_tableau) {
		for (u8 i = 0; i < KlondikeGame::NUM_TABLEAU_PILES; ++i) {
			if (i == fromTableau)
				continue; // Can't move to itself.
			if (!tableau[i].hasCards()) {
				if (card.getRank() == RANK_KING) { // Move king over to empty spot.
					out_to_tableau = i;
					return true;
				}
			} else if (_can_place_card(card, tableau[i].getFromTop())) { // Move card over to first available spot.
				out_to_tableau = i;
				return true;
			}
		}
		return false;
	}
	// See if a card (lower rank than King) has two spots on the tableau it can move onto.
	bool _has_two_available_spots(const Card& card, const std::vector<Pile>& tableau, u8& firstSpot) {
		if (card.getRank() == RANK_KING)
			return false;
		const bool wantedSuitIsBlack = IsRed(card.getSuit());
		const Rank wantedRank = card.getRank() + 1;
		u8 numAvailableSpots = 0;
		for (u8 i = 0; i < KlondikeGame::NUM_TABLEAU_PILES; ++i) {
			if (tableau[i].hasCards()) {
				const Card& c = tableau[i].getFromTop();
				if (wantedSuitIsBlack && !IsRed(c.getSuit()) && wantedRank == c.getRank()) {
					if (numAvailableSpots >= 1)
						return true;
					firstSpot = i;
					++numAvailableSpots;
				}
			}
		}
		return false;
	}
	// See if there is room in the tableau for all the kings. If there is, return an empty spot to place a king in.
	// This function "cheats", by peeking under flipped cards at the base of tableau piles.
	bool _has_space_for_all_kings(const std::vector<Pile>& tableau, u8& emptySpot) {
		u8 numKingSpaces = 0;
		for (u8 i = 0; i < KlondikeGame::NUM_TABLEAU_PILES; ++i) {
			if (!tableau[i].hasCards()) {
				emptySpot = i;
				++numKingSpaces;
			} else if (tableau[i][0].getRank() == RANK_KING)
				++numKingSpaces;
		}
		return numKingSpaces >= toUType(Suit::TOTAL_SUITS);
	}
}

bool KlondikeSolver::_is_card_available(const Card& cardToFind) const {
	for (u8 i = 0; i < KlondikeGame::NUM_TABLEAU_PILES; ++i) {
		if (!game_.tableau[i].hasCards())
			continue;
		if (cardToFind == game_.tableau[i].getFromTop())
			return true;
	}
	for (u8 i = game_.getStockPosition(); i < game_.stock.size(); i += KlondikeGame::NUM_STOCK_CARD_DRAW) {
		if (cardToFind == game_.stock[i])
			return true;
	}
	return false;
}

bool KlondikeSolver::_find_auto_moves(MoveList& autoMoves) {
	for (u8 i = 0; i < KlondikeGame::NUM_TABLEAU_PILES; ++i) {
		if (!game_.tableau[i].hasCards())
			continue;
		// First, check for a guaranteed moves to the foundation.
		if (const Card& c = game_.tableau[i].getFromTop(); _guaranteed_move_to_foundation(c, game_.foundation)) {
			const bool flippedCard = game_.tableau[i].size() > 1 && !game_.tableau[i].getFromTop(1).isFaceUp(); // Check if move will reveal a tableau card.
			autoMoves.push_back(Move::Tableau(c, PileID{ PileType::TABLEAU, i }, PileID{ PileType::FOUNDATION, toUType(c.getSuit()) }, 1, flippedCard));
		} else {
			// Second, look for tableau runs that have two open options in the tableau.
			u8 runLength;
			const Card* topOfRun;
			_find_top_of_run(game_.tableau[i], runLength, &topOfRun);
			if (u8 firstSpot; _has_two_available_spots(*topOfRun, game_.tableau, firstSpot)) {
				const bool flippedCard = !game_.tableau[i][0].isFaceUp(); // Moving full run, so will flip if there are still face-down cards.
				autoMoves.push_back(Move::Tableau(*topOfRun, PileID{ PileType::TABLEAU, i }, PileID{ PileType::TABLEAU, firstSpot }, runLength, flippedCard));
			// Last, look for a run with a king, and see if there are enough tableau spaces to guarantee it has room.
			// Note that while this case can run while the first two also run, it would get an invalid run length.
			// It can instead be picked up be subsequent calls.
			} else if (!game_.tableau[i][0].isFaceUp()) { // Don't move a king that is already on an empty spot.
				if (u8 emptySpot;  topOfRun->getRank() == RANK_KING && _has_space_for_all_kings(game_.tableau, emptySpot))
					autoMoves.push_back(Move::Tableau(*topOfRun, PileID{ PileType::TABLEAU, i }, PileID{ PileType::TABLEAU, emptySpot }, runLength, true));
			}
		}
	}
	return autoMoves.size();
}

void KlondikeSolver::_find_foundation_moves(MoveList& availableMoves) {
	for (u8 i = 0; i < KlondikeGame::NUM_TABLEAU_PILES; ++i) {
		if (!game_.tableau[i].hasCards())
			continue;
		const Card& c = game_.tableau[i].getFromTop();
		if (_can_move_to_foundation(c, game_.foundation)) {
			const bool flippedCard = game_.tableau[i].size() > 1 && !game_.tableau[i].getFromTop(1).isFaceUp(); // Check if move will reveal a tableau card.
			availableMoves.push_back(Move::Tableau(c, PileID{ PileType::TABLEAU, i }, PileID{ PileType::FOUNDATION, toUType(c.getSuit()) }, 1, flippedCard));
		}
	}
	for (u8 i = game_.getStockPosition(); i < game_.stock.size(); i += KlondikeGame::NUM_STOCK_CARD_DRAW) {
		const Card& c = game_.stock[i];
		if (_can_move_to_foundation(c, game_.foundation))
			availableMoves.push_back(Move::Stock(c, game_.getStockPosition(), i, PileID{ PileType::FOUNDATION, toUType(c.getSuit()) }));
	}
}

void KlondikeSolver::_find_full_run_moves(MoveList& availableMoves) {
	for (u8 i = 0; i < KlondikeGame::NUM_TABLEAU_PILES; ++i) {
		const Pile& fromPile = game_.tableau[i];
		const Card* card;
		u8 runLength;
		if (!_find_top_of_run(fromPile, runLength, &card) || (runLength == fromPile.size() && card->getRank() == RANK_KING))
			continue; // Empty pile or King in empty space.
		// Find a place to move the card to. Only take first place if there are multiple.
		u8 toPile;
		if (!_find_tableau_to_tableau_move(*card, game_.tableau, i, toPile))
			continue;
		const bool flippedCard = game_.tableau[i].size() > runLength;
		availableMoves.push_back(Move::Tableau(*card, PileID{ PileType::TABLEAU, i }, PileID{ PileType::TABLEAU, toPile }, runLength, flippedCard));
	}
}

void KlondikeSolver::_find_partial_run_moves(MoveList& availableMoves) {
	for (u8 i = 0; i < KlondikeGame::NUM_TABLEAU_PILES; ++i) {
		const Pile& fromPile = game_.tableau[i];
		u8 runLength;
		if (!_find_top_of_run(fromPile, runLength))
			continue;
		for (u8 k = runLength - 1; k != 0; --k) {
			const Card& c = fromPile.getFromTop(k - 1); // Next card in run.

			// Check if this move is in our move history. If we've already moved it, ignore this card.
			if (std::find(partial_run_move_cards_.begin(), partial_run_move_cards_.end(), c) != partial_run_move_cards_.end())
				continue;

			// See if there is a spot to move this partial run to.
			u8 toPile;
			if (!_find_tableau_to_tableau_move(c, game_.tableau, i, toPile))
				continue;

			// It is a possible valid move to split up a run if:
			// 1. The card being uncovered can be moved to the foundation.
			// 2. There is another card that can be moved onto the uncovered card.
			if (_can_move_to_foundation(fromPile.getFromTop(k), game_.foundation) || _is_card_available(Card(GetSameColourOtherSuit(c.getSuit()), c.getRank()))) {
				availableMoves.push_back(Move::TableauPartial(c, PileID{ PileType::TABLEAU, i }, PileID{ PileType::TABLEAU, toPile }, k));
			}
		}
	}
}

void KlondikeSolver::_find_stock_moves(MoveList& availableMoves) {
	for (u8 i = game_.getStockPosition(); i < game_.stock.size(); i += KlondikeGame::NUM_STOCK_CARD_DRAW) {
		const Card& c = game_.stock[i];
		for (u8 k = 0; k < KlondikeGame::NUM_TABLEAU_PILES; ++k) {
			if (!game_.tableau[k].hasCards()) {
				if (c.getRank() == RANK_KING) // Move king down to empty spot.
					availableMoves.push_back(Move::Stock(c, game_.getStockPosition(), i, PileID{ PileType::TABLEAU, k }));
			} else if (_can_place_card(c, game_.tableau[k].getFromTop())) { // Place card on a tableau pile.
				availableMoves.push_back(Move::Stock(c, game_.getStockPosition(), i, PileID{ PileType::TABLEAU, k }));
			}
		}
	}
}

bool KlondikeSolver::_find_available_moves(MoveList& availableMoves) {
	// Order here sets priority.
	_find_full_run_moves(availableMoves);
	_find_partial_run_moves(availableMoves);
	_find_stock_moves(availableMoves);
	_find_foundation_moves(availableMoves);

	if (game_.isStockDirty()) // If we can shuffle the stock, do so last.
		availableMoves.push_back(Move::RepileStock(game_.getStockPosition()));

	return !availableMoves.empty();
}

bool KlondikeSolver::_is_seen_state() {
	const std::string uniqueId(game_.getUniqueStateID());
	if (seen_states_.count(uniqueId))
		return true;
	seen_states_.insert(uniqueId);
	return false;
}

void KlondikeSolver::_init() {
	game_.setUpGame();
	seen_states_.reserve( static_cast<unsigned int>(std::min(maxStates, static_cast<Solitaire::u64>(seen_states_.max_size()))));
}

GameResult::Result KlondikeSolver::_solve_recursive(u32 depth) {
	if (_is_seen_state())
		return GameResult::Result::LOSE;

	MoveList autoMoves, tempAutoMoves, availableMoves;

	while (_find_auto_moves(tempAutoMoves)) {
		for (auto m : tempAutoMoves) {
			autoMoves.push_back(m);
			_do_move(m);
		}
		tempAutoMoves.clear();
	}

	if (game_.isGameWon())
		return GameResult::Result::WIN;

	if (states_tried_ != 0 && states_tried_ >= maxStates)
		return GameResult::Result::UNKNOWN; // Ran out of allowed states to try.

	if (_find_available_moves(availableMoves)) {
		for (auto m : availableMoves) {
			_do_move(m);
			++states_tried_;

			GameResult::Result r = _solve_recursive(depth + 1);
			if (r != GameResult::Result::LOSE)
				return r;

			_undo_move(m);
		}
	}

	for (std::size_t i = autoMoves.size(); i != 0; )
		_undo_move(autoMoves[--i]);

	return GameResult::Result::LOSE;
}

void KlondikeSolver::_do_move(const Move& m) {
	move_sequence_.push_back(m);
	if (m.type == MoveType::TABLEAU_PARTIAL)
		partial_run_move_cards_.push_back(m.movedCard);
	KlondikeSolver::doMove(game_, m);
}

void KlondikeSolver::_undo_move(const Move& m) {
	move_sequence_.pop_back();
	switch (m.type) {
	case MoveType::TABLEAU_PARTIAL:
	{
		auto position = std::find(partial_run_move_cards_.begin(), partial_run_move_cards_.end(), m.movedCard);
		if (position != partial_run_move_cards_.end())
			partial_run_move_cards_.erase(position);
		else
			std::cerr << "Error (_undo_move): Failed to find partial run move to erase!\n";
	}
		[[fallthrough]];
	case MoveType::TABLEAU: // Move one or several cards back from one pile to another.
		if (m.flippedCard) // If we flipped a card, turn it back over first.
			game_.getPile(m.fromPile).getFromTop().flipCard();
		Pile::MoveCards(game_.getPile(m.toPile), game_.getPile(m.fromPile), m.cardsToMove);
		break;
	case MoveType::STOCK: // Move one card from the end of a tableau or foundation pile back to the stock pile.
		Pile::MoveCard(game_.getPile(m.toPile), -1, game_.getPile(m.fromPile), m.stockMovePosition);
		[[fallthrough]];
	case MoveType::REPILE_STOCK: // Undo stock re-pile by moving the stock position back to its previous position.
		game_.setStockPosition(m.stockPosition);
		break;
	}
}

GameResult KlondikeSolver::Solve() {
	_init();

	u32 depth = 0;
	GameResult::Result r = _solve_recursive(depth);

	if (r == GameResult::Result::UNKNOWN || r == GameResult::Result::LOSE)
		move_sequence_.clear();

	return GameResult{ states_tried_, game_.seed, move_sequence_, r };
}

void KlondikeSolver::doMove(KlondikeGame& game, const Move& m) {
	switch (m.type) {
	case MoveType::TABLEAU_PARTIAL:
		[[fallthrough]];
	case MoveType::TABLEAU: // Move one or several cards from one pile to another.
		Pile::MoveCards(game.getPile(m.fromPile), game.getPile(m.toPile), m.cardsToMove);
		if (m.flippedCard) // Reveal an uncovered card.
			game.getPile(m.fromPile).getFromTop().flipCard();
		break;
	case MoveType::STOCK: // Move one card from stock to a tableau or foundation pile.
		if (m.stockMovePosition != 0)
			game.setStockPosition(m.stockMovePosition - 1); // Move to previous card (now made visible).
		else
			game.repileStock(); // We've used up all the "waste" cards. Need to re-pile or we wouldn't be looking at a card anymore.
		Pile::MoveCard(game.getPile(m.fromPile), m.stockMovePosition, game.getPile(m.toPile));
		break;
	case MoveType::REPILE_STOCK: // Shuffle the stock, resetting the stock position.
		game.repileStock();
		break;
	}
}
