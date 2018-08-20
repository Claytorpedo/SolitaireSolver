#include "KlondikeSolver.hpp"

#include <algorithm>
#include <climits>
#include <iostream>
#include <string>

#include "units.hpp"

using namespace solitaire;

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
	bool _find_top_of_run(const Pile& pile, u8& out_run_length, Card* optional_out_card=nullptr) {
		for (u8 i = 0; i < pile.size(); ++i) {
			if (pile[i].isFaceUp()) {
				if (optional_out_card)
					*optional_out_card = pile[i];
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
	for (u8 i = game_.getStockPosition(); i < game_.stock.size(); i = game_.getNextInStock(i)) {
		if (cardToFind == game_.stock[i])
			return true;
	}
	return false;
}

std::unique_ptr<Move> KlondikeSolver::_find_auto_move() {
	// Auto moves can change the state of the board and interfere with each other, so only do one at a time.
	for (u8 i = 0; i < KlondikeGame::NUM_TABLEAU_PILES; ++i) {
		if (!game_.tableau[i].hasCards())
			continue;
		// First, check for a guaranteed moves to the foundation.
		if (const Card& c = game_.tableau[i].getFromTop(); _guaranteed_move_to_foundation(c, game_.foundation)) {
			const bool flippedCard = game_.tableau[i].size() > 1 && !game_.tableau[i].getFromTop(1).isFaceUp(); // Check if move will reveal a tableau card.
			return std::make_unique<Move>(Move::Tableau(c, PileID{ PileType::TABLEAU, i }, PileID{ PileType::FOUNDATION, toUType(c.getSuit()) }, 1, flippedCard));
		} else {
			// Second, look for tableau runs that have two open options in the tableau.
			u8 runLength;
			Card topOfRun;
			_find_top_of_run(game_.tableau[i], runLength, &topOfRun);
			if (u8 firstSpot; _has_two_available_spots(topOfRun, game_.tableau, firstSpot)) {
				const bool flippedCard = !game_.tableau[i][0].isFaceUp(); // Moving full run, so will flip if there are still face-down cards.
				return std::make_unique<Move>(Move::Tableau(topOfRun, PileID{ PileType::TABLEAU, i }, PileID{ PileType::TABLEAU, firstSpot }, runLength, flippedCard));

			// Last, look for a run with a king, and see if there are enough tableau spaces to guarantee it has room.
			// Note that while this case can run while the first two also run, it would get an invalid run length.
			// It can instead be picked up be subsequent calls.
			} else if (!game_.tableau[i][0].isFaceUp()) { // Don't move a king that is already on an empty spot.
				if (u8 emptySpot;  topOfRun.getRank() == RANK_KING && _has_space_for_all_kings(game_.tableau, emptySpot))
					return std::make_unique<Move>(Move::Tableau(topOfRun, PileID{ PileType::TABLEAU, i }, PileID{ PileType::TABLEAU, emptySpot }, runLength, true));
			}
		}
	}
	return nullptr;
}

void KlondikeSolver::_find_moves_to_foundation(MoveList& availableMoves) {
	for (u8 i = 0; i < KlondikeGame::NUM_TABLEAU_PILES; ++i) {
		if (!game_.tableau[i].hasCards())
			continue;
		const Card& c = game_.tableau[i].getFromTop();
		if (_can_move_to_foundation(c, game_.foundation)) {
			const bool flippedCard = game_.tableau[i].size() > 1 && !game_.tableau[i].getFromTop(1).isFaceUp(); // Check if move will reveal a tableau card.
			availableMoves.push_back(Move::Tableau(c, PileID{ PileType::TABLEAU, i }, PileID{ PileType::FOUNDATION, toUType(c.getSuit()) }, 1, flippedCard));
		}
	}
	for (u8 i = game_.getStockPosition(); i < game_.stock.size(); i = game_.getNextInStock(i)) {
		const Card& c = game_.stock[i];
		if (_can_move_to_foundation(c, game_.foundation))
			availableMoves.push_back(Move::Stock(c, game_.getStockPosition(), i, PileID{ PileType::FOUNDATION, toUType(c.getSuit()) }));
	}
}

void KlondikeSolver::_find_full_run_moves(MoveList& availableMoves) {
	for (u8 i = 0; i < KlondikeGame::NUM_TABLEAU_PILES; ++i) {
		const Pile& fromPile = game_.tableau[i];
		Card card;
		u8 runLength;
		if (!_find_top_of_run(fromPile, runLength, &card) || (runLength == fromPile.size() && card.getRank() == RANK_KING))
			continue; // Empty pile or King in empty space.
		// Find a place to move the card to. Only take first place if there are multiple.
		u8 toPile;
		if (!_find_tableau_to_tableau_move(card, game_.tableau, i, toPile))
			continue;
		const bool flippedCard = game_.tableau[i].size() > runLength;
		availableMoves.push_back(Move::Tableau(card, PileID{ PileType::TABLEAU, i }, PileID{ PileType::TABLEAU, toPile }, runLength, flippedCard));
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
			if (std::find(partial_run_move_cards_.cbegin(), partial_run_move_cards_.cend(), c) != partial_run_move_cards_.cend())
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

void KlondikeSolver::_find_stock_to_tableau_moves(MoveList& availableMoves) {
	for (u8 i = game_.getStockPosition(); i < game_.stock.size(); i = game_.getNextInStock(i)) {
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
	_find_stock_to_tableau_moves(availableMoves);
	_find_moves_to_foundation(availableMoves);

	if (game_.isStockDirty()) // If we can shuffle the stock, do so last.
		availableMoves.push_back(Move::RepileStock(game_.getStockPosition()));

	return !availableMoves.empty();
}

bool KlondikeSolver::_is_seen_state() {
	if (!move_sequence_.empty() && move_sequence_.back().type == MoveType::REPILE_STOCK)
		return false; // Don't store new state on repile stock moves.

	// Build a unique ID for the deck, using the series of all its cards.
	// Each card takes a value of [0,51], meaning they fit in the space of 6 bits.
	// This means the unique ID for a full deck can be packed into a 39 char string.
	std::memset(id_scratch_space_.data(), 0, SCRATCH_BUFF_SIZE);

	u8 offset = 0;
	u8 index = 0;
	auto pack_bits = [&](uint8_t bits) {
		uint16_t& edit = reinterpret_cast<uint16_t&>(id_scratch_space_[index]);
		edit |= bits << offset;
		// We cut off two bits every time.
		if (offset == 0) {
			// When offset wraps to 6, the next write will start on the last two bits of the current byte.
			offset = 6;
		} else {
			offset -= 2;
			++index;
		}
	};

	auto pack_pile_bits = [&](const Pile& pile) {
		const u8 size = pile.size();
		for (u8 i = 0; i < size; ++i)
			pack_bits(static_cast<uint8_t>((toUType(pile[i].getSuit()) * CARDS_PER_SUIT) + pile[i].getRank()));
	};

	for (const Pile& pile : game_.tableau)
		pack_pile_bits(pile);
	for (const Pile& pile : game_.foundation)
		pack_pile_bits(pile);
	pack_pile_bits(game_.stock);

	// Get the 39 char string, ignoring the extra buffer byte on the end of the scratch space.
	const std::string uniqueId(id_scratch_space_.cbegin(), id_scratch_space_.cend() - 1);

	return !seen_states_.insert(uniqueId).second;
}

void KlondikeSolver::_init() {
	states_tried_ = 0;
	seen_states_.clear();
	move_sequence_.clear();
	partial_run_move_cards_.clear();
	seen_states_.reserve( static_cast<unsigned int>(std::min(maxStates, static_cast<u64>(seen_states_.max_size()))));
}

GameResult::Result KlondikeSolver::_solve_recursive(u32 depth) {
	if (_is_seen_state())
		return GameResult::Result::LOSE;

	MoveList autoMoves, availableMoves;

	while (std::unique_ptr<Move> m = _find_auto_move()) {
		autoMoves.push_back(*m.get());
		_do_move(*m.get());
	}

	if (game_.isGameWon())
		return GameResult::Result::WIN;

	if (states_tried_ != 0 && maxStates != 0 && states_tried_ >= maxStates)
		return GameResult::Result::UNKNOWN; // Ran out of allowed states to try.

	if (_find_available_moves(availableMoves)) {
		for (const Move& m : availableMoves) {
			_do_move(m);
			++states_tried_;

			if (false)
				game_.printGame();

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
	case MoveType::REPILE_STOCK: // Undo stock repile by moving the stock position back to its previous position.
		game_.setStockPosition(m.currentStockPosition);
		break;
	}
}

GameResult KlondikeSolver::solve() {
	u32 depth = 0;
	GameResult::Result r = _solve_recursive(depth);

	if (r == GameResult::Result::UNKNOWN || r == GameResult::Result::LOSE)
		move_sequence_.clear();

	return GameResult{ states_tried_, game_.getSeed(), move_sequence_, r };
}

void KlondikeSolver::setSeed(u64 seed) {
	game_ = KlondikeGame(seed);
	game_.setUpGame();
	_init();
}

void KlondikeSolver::setGame(const KlondikeGame& game) {
	game_ = game;
	_init();
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
		Pile::MoveCard(game.getPile(m.fromPile), m.stockMovePosition, game.getPile(m.toPile));
		if (m.stockMovePosition != 0)
			game.setStockPosition(m.stockMovePosition - 1); // Move to previous card (now made visible).
		else
			game.repileStock(); // We've used up all the "waste" cards. Need to re-pile or we wouldn't be looking at a card anymore.
		break;
	case MoveType::REPILE_STOCK: // Shuffle the stock, resetting the stock position.
		game.repileStock();
		break;
	}
}
