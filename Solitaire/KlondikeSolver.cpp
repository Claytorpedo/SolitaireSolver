#include "KlondikeSolver.hpp"

#include <algorithm>
#include <climits>
#include <iostream>
#include <string>

#include "units.hpp"

using namespace solitaire;

namespace {

	// -------------------------------- Move Strategy -----------------------------------------------
	// Numbers are padded such that if their base priority is EG 100, then they can be subtracted from to make them higher priority.
	enum class Priority : u32 {
		REVEAL = 100,                // Moves that reveal a card. Number indicates how many cards are flipped in the stack (Klondike has max 6).
		CLEAR_WITH_KING = 200,       // Clearning an empty board spot when there is a king available to occupy it.
		STOCK = 300,                 // Moves from stock pile (to tableau or foundation). Higher priority towards the end of the stock pile.
		TABLEAU_TO_FOUNDATION = 400,
		REPILE_STOCK = 400,
		PARTIAL = 600,               // Intra-tableau moves that don't reveal a card or clear a space.
	};

	// ----------------------------------------------------------------------------------------------

	bool _can_place_card(const Card& lower, const Card& higher) {
		return IsRed(lower.getSuit()) != IsRed(higher.getSuit()) && lower.getRank() == higher.getRank() - 1;
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
	bool _find_top_of_run(const Pile& pile, u8& out_run_length, Card* optional_out_card = nullptr) {
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
		bool foundOneSpot = false;
		for (u8 i = 0; i < KlondikeGame::NUM_TABLEAU_PILES; ++i) {
			if (tableau[i].hasCards()) {
				const Card& c = tableau[i].getFromTop();
				if (wantedSuitIsBlack && !IsRed(c.getSuit()) && wantedRank == c.getRank()) {
					if (foundOneSpot)
						return true;
					firstSpot = i;
					foundOneSpot = true;;
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

	std::unique_ptr<Move> _find_guaranteed_stock_move(u8 testStockPosition, const KlondikeGame& game) {
		const Card& c = game.stock[testStockPosition];
		// Check for a guaranteed moves to the foundation.
		if (_guaranteed_move_to_foundation(c, game.foundation))
			return std::make_unique<Move>(Move::Stock(c, game.getStockPosition(), testStockPosition, PileID{ PileType::FOUNDATION, toUType(c.getSuit()) }));
		// Check if it's a king, and see if there are enough tableau spaces to guarantee it has room.
		if (u8 emptySpot; c.getRank() == RANK_KING && _has_space_for_all_kings(game.tableau, emptySpot))
			return std::make_unique<Move>(Move::Stock(c, game.getStockPosition(), testStockPosition, PileID{ PileType::TABLEAU, emptySpot }));
		return nullptr;
	}
}

bool KlondikeSolver::_is_king_available() const {
	Card card;
	u8 unused;
	for (const auto& pile : game_.tableau) {
		if (_find_top_of_run(pile, unused, &card) && card.getRank() == RANK_KING)
			return true;
	}
	for (u8 i = game_.getStockPosition(); i < game_.stock.size(); i = game_.getNextInStock(i)) {
		if (game_.stock[i].getRank() == RANK_KING)
			return true;
	}
	return false;
}

bool KlondikeSolver::_is_card_available(const Card& cardToFind) const {
	for (const auto& pile : game_.tableau) {
		if (pile.hasCards() && cardToFind == pile.getFromTop())
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
	// Find auto-moves in the tableau.
	for (u8 i = 0; i < KlondikeGame::NUM_TABLEAU_PILES; ++i) {
		if (!game_.tableau[i].hasCards())
			continue;
		// Check for a guaranteed move to the foundation.
		if (const Card& c = game_.tableau[i].getFromTop(); _guaranteed_move_to_foundation(c, game_.foundation)) {
			const bool flippedCard = game_.tableau[i].size() > 1 && !game_.tableau[i].getFromTop(1).isFaceUp(); // Check if move will reveal a tableau card.
			return std::make_unique<Move>(Move::Tableau(c, PileID{ PileType::TABLEAU, i }, PileID{ PileType::FOUNDATION, toUType(c.getSuit()) }, 1, flippedCard));
		}

		// Look for a run with a king, and see if there are enough tableau spaces to guarantee it has room.
		u8 runLength;
		Card topOfRun;
		_find_top_of_run(game_.tableau[i], runLength, &topOfRun);
		if (!game_.tableau[i][0].isFaceUp() && topOfRun.getRank() == RANK_KING) { // Don't move a king that is already on an empty spot.
			if (u8 emptySpot; _has_space_for_all_kings(game_.tableau, emptySpot))
				return std::make_unique<Move>(Move::Tableau(topOfRun, PileID{ PileType::TABLEAU, i }, PileID{ PileType::TABLEAU, emptySpot }, runLength, true));
		}
	}
	// Find auto-moves in the stock pile. There are some special cases where taking a card won't affect what stock cards are available.
	if (!game_.stock.hasCards())
		return nullptr;

	const u8 stockPos = game_.getStockPosition();
	const u8 stockSize = game_.stock.size();

	if (stockPos == stockSize - 1) {
		// The last card is always a candidate as it cannot change the stock deal order.
		return _find_guaranteed_stock_move(stockPos, game_);
	}

	if ((stockPos + 1) % KlondikeGame::NUM_STOCK_CARD_DRAW == 0 ) { // We are in-run with our deal amount.
		// We have two possible moves that cannot change the stock deal order: second last and last, as we know by this point we are not the last card.
		u8 secondLastStockPos = stockPos;
		for (u8 i = game_.getNextInStock(stockPos); i < stockSize - 1; i = game_.getNextInStock(i))
			secondLastStockPos = i;

		auto move = _find_guaranteed_stock_move(secondLastStockPos, game_);
		if (move)
			return move;
		return _find_guaranteed_stock_move(stockSize - 1, game_);
	}

	// Check special case if we are in the last section, but not the last card.
	u8 cardsAtEnd = stockSize % KlondikeGame::NUM_STOCK_CARD_DRAW;
	if (cardsAtEnd == 0)
		cardsAtEnd = KlondikeGame::NUM_STOCK_CARD_DRAW;
	if (stockSize - stockPos <= cardsAtEnd) {
		// Can move the current card, but not the last card (because then the current card would no longer be available).
		return _find_guaranteed_stock_move(stockPos, game_);
	}

	return nullptr;
}

void KlondikeSolver::_find_moves_to_foundation(PriorityMoveList& availableMoves) {
	for (u8 i = 0; i < KlondikeGame::NUM_TABLEAU_PILES; ++i) {
		if (!game_.tableau[i].hasCards())
			continue;
		const Card& c = game_.tableau[i].getFromTop();
		if (_can_move_to_foundation(c, game_.foundation)) {
			const bool flippedCard = game_.tableau[i].size() > 1 && !game_.tableau[i].getFromTop(1).isFaceUp(); // Check if move will reveal a tableau card.
			const u32 priority = flippedCard ? toUType(Priority::REVEAL) - (game_.tableau[i].size() - 1) : toUType(Priority::TABLEAU_TO_FOUNDATION);
			availableMoves.emplace_back(PriorityMove{ Move::Tableau(c, PileID{ PileType::TABLEAU, i }, PileID{ PileType::FOUNDATION, toUType(c.getSuit()) }, 1, flippedCard), priority });
		}
	}
	for (u8 i = game_.getStockPosition(); i < game_.stock.size(); i = game_.getNextInStock(i)) {
		const Card& c = game_.stock[i];
		if (_can_move_to_foundation(c, game_.foundation))
			availableMoves.emplace_back(PriorityMove{ Move::Stock(c, game_.getStockPosition(), i, PileID{ PileType::FOUNDATION, toUType(c.getSuit()) }), toUType(Priority::STOCK) - i });
	}
}

void KlondikeSolver::_find_full_run_moves(PriorityMoveList& availableMoves) {
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
		const u32 remainingCards = game_.tableau[i].size() - runLength;
		if (remainingCards > 0) {
			availableMoves.emplace_back(PriorityMove{ Move::Tableau(card, PileID{ PileType::TABLEAU, i }, PileID{ PileType::TABLEAU, toPile }, runLength, true), toUType(Priority::REVEAL) - remainingCards });
		} else if (_is_king_available()) {
			availableMoves.emplace_back(PriorityMove{ Move::Tableau(card, PileID{ PileType::TABLEAU, i }, PileID{ PileType::TABLEAU, toPile }, runLength, false), toUType(Priority::CLEAR_WITH_KING) });
		}
	}
}

void KlondikeSolver::_find_partial_run_moves(PriorityMoveList& availableMoves) {
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
				availableMoves.emplace_back(PriorityMove{ Move::TableauPartial(c, PileID{ PileType::TABLEAU, i }, PileID{ PileType::TABLEAU, toPile }, k), toUType(Priority::PARTIAL) });
			}
		}
	}
}

void KlondikeSolver::_find_stock_to_tableau_moves(PriorityMoveList& availableMoves) {
	for (u8 i = game_.getStockPosition(); i < game_.stock.size(); i = game_.getNextInStock(i)) {
		const Card& c = game_.stock[i];
		for (u8 k = 0; k < KlondikeGame::NUM_TABLEAU_PILES; ++k) {
			if (!game_.tableau[k].hasCards()) {
				if (c.getRank() == RANK_KING) // Move king down to empty spot.
					availableMoves.emplace_back(PriorityMove{ Move::Stock(c, game_.getStockPosition(), i, PileID{ PileType::TABLEAU, k }), toUType(Priority::STOCK) - i });
			} else if (_can_place_card(c, game_.tableau[k].getFromTop())) { // Place card on a tableau pile.
				availableMoves.emplace_back(PriorityMove{ Move::Stock(c, game_.getStockPosition(), i, PileID{ PileType::TABLEAU, k }), toUType(Priority::STOCK) - i });
			}
		}
	}
}

KlondikeSolver::PriorityMoveList KlondikeSolver::_find_available_moves() {
	PriorityMoveList moves;
	_find_full_run_moves(moves);
	_find_partial_run_moves(moves);
	_find_stock_to_tableau_moves(moves);
	_find_moves_to_foundation(moves);

	if (game_.isStockDirty()) // If we can shuffle the stock, do so last.
		moves.emplace_back(PriorityMove{ Move::RepileStock(game_.getStockPosition()), toUType(Priority::REPILE_STOCK) });

	std::sort(moves.begin(), moves.end(), [](const auto& lhs, const auto& rhs) { return lhs.priority < rhs.priority; });
	return moves;
}

bool KlondikeSolver::_is_seen_state() {
	if (!move_sequence_.empty() && move_sequence_.back().type == MoveType::REPILE_STOCK)
		return false; // Don't bother storing new state on repile stock moves.

	// Build a unique ID for the deck, using the series of all its cards.
	// Each card takes a value of [0,51], meaning they fit in the space of 6 bits.
	// This means the unique ID for a full deck can be packed into a 39 char string,
	// plus 12 chars for pile separators and the stock position, for a total of 48.
	std::string uniqueId;
	uniqueId.resize(UNIQUE_STATE_SIZE, 0);

	u8 offset = 0;
	u8 index = 0;
	auto pack_bits = [idPtr = uniqueId.data(), &index, &offset](uint8_t bits) {
		uint16_t& edit = reinterpret_cast<uint16_t&>(idPtr[index]);
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

	auto pack_pile_bits = [this, &pack_bits](const Pile& pile) {
		const u8 size = pile.size();
		for (u8 i = 0; i < size; ++i)
			pack_bits(static_cast<uint8_t>((toUType(pile[i].getSuit()) * CARDS_PER_SUIT) + pile[i].getRank()));
	};

	constexpr u8 pileSeparator = 63;
	for (const Pile& pile : game_.tableau) {
		pack_pile_bits(pile);
		pack_bits(pileSeparator);
	}
	for (const Pile& pile : game_.foundation) {
		pack_pile_bits(pile);
		pack_bits(pileSeparator);
	}
	pack_pile_bits(game_.stock);
	pack_bits(game_.getStockPosition());

	return !seen_states_.insert(uniqueId).second;
}

void KlondikeSolver::_init() {
	states_tried_ = 0;
	seen_states_.clear();
	move_sequence_.clear();
	partial_run_move_cards_.clear();
	seen_states_.reserve( static_cast<unsigned int>(maxStates == 0 ? 10'000'000 : std::min(maxStates, static_cast<u64>(seen_states_.max_size()))));
}

GameResult::Result KlondikeSolver::_solve_recursive(u32 depth) {
	if (_is_seen_state())
		return GameResult::Result::LOSE;

	MoveList autoMoves;
	while (std::unique_ptr<Move> m = _find_auto_move()) {
		autoMoves.push_back(*m.get());
		_do_move(*m.get());
	}

	if (game_.isGameWon())
		return GameResult::Result::WIN;

	if (states_tried_ != 0 && maxStates != 0 && states_tried_ >= maxStates)
		return GameResult::Result::UNKNOWN; // Ran out of allowed states to try.

	if (PriorityMoveList moves = _find_available_moves(); !moves.empty()) {
		for (const auto& priMove : moves) {
			_do_move(priMove.move);
			++states_tried_;

			if (false)
				game_.printGame();

			GameResult::Result r = _solve_recursive(depth + 1);
			if (r != GameResult::Result::LOSE)
				return r;

			_undo_move(priMove.move);
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
