#include "Move.hpp"

using namespace solitaire;

Move Move::TableauPartial(const Card& movedCard, PileID fromPile, PileID toPile, u8 cardsToMove) {
	return Move(movedCard, fromPile, toPile, cardsToMove, MoveType::TABLEAU_PARTIAL, false);
}
Move Move::Tableau(const Card& movedCard, PileID fromPile, PileID toPile, u8 cardsToMove, bool flippedCard) {
	return Move(movedCard, fromPile, toPile, cardsToMove, MoveType::TABLEAU, flippedCard);
}
Move Move::Stock(const Card& movedCard, u8 currentStockPosition, u8 stockMovePosition, PileID toPile) {
	return Move(movedCard, PileID{ PileType::STOCK }, toPile, currentStockPosition, stockMovePosition, MoveType::STOCK);
}
Move Move::RepileStock(u8 stockPosition) {
	return Move(Card{}, PileID{}, PileID{}, stockPosition, 0, MoveType::REPILE_STOCK);
}
