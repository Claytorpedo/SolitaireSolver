#pragma once

#include "units.h"

#include <vector>
#include <iostream>
#include <string>

namespace Solitaire {
	constexpr Rank CARDS_PER_SUIT = 13;
	constexpr Rank RANK_KING = CARDS_PER_SUIT;
	constexpr u8 CARDS_PER_DECK = static_cast<u8>(CARDS_PER_SUIT) * 4;

	enum class Suit : u8 {
		HEARTS,
		DIAMONDS,
		CLUBS,
		SPADES,
		TOTAL_SUITS
	};

	inline bool IsRed(Suit s) {
		return s == Suit::HEARTS || s == Suit::DIAMONDS;
	}

	inline bool IsOppositeColour(Suit s, Suit o) {
		return IsRed(s) != IsRed(o);
	}

	inline Suit GetSameColourOtherSuit(Suit s) {
		switch (s) {
		case Suit::HEARTS:
			return Suit::DIAMONDS;
		case Suit::DIAMONDS:
			return Suit::HEARTS;
		case Suit::CLUBS:
			return Suit::SPADES;
		case Suit::SPADES:
			return Suit::CLUBS;
		default:
			std::cerr << "Error (GetSameColourOtherSuit): Invalid card suit for.\n";
			return Suit::TOTAL_SUITS;
		}
	}

	inline char SuitToChar(Suit s) {
		switch (s) {
		case Suit::HEARTS:
			return 'H';
		case Suit::CLUBS:
			return 'C';
		case Suit::DIAMONDS:
			return 'D';
		case Suit::SPADES:
			return 'S';
		default:
			return '?';
		}
	}

	inline std::string RankToStr(Rank r) {
		if (r == 1)
			return "A ";
		if (r < 10)
			return std::to_string(r) + " ";
		if (r == 10)
			return "10";
		if (r == 11)
			return "J ";
		if (r == 12)
			return "Q ";
		return "K ";
	}

	class Card {
	public:
		Card(Suit suit, Rank rank, bool isFaceUp=true) : suit_(suit), rank_(rank), is_face_up_(isFaceUp) {}

		~Card() = default;

		void flipCard() { is_face_up_ = !is_face_up_; }

		inline Rank getRank()  const { return rank_; }
		inline Suit getSuit()  const { return suit_; }
		inline bool isFaceUp() const { return is_face_up_; }

		inline bool operator==(const Card& o) const { return rank_ == o.rank_ && suit_ == o.suit_; }

		std::string getSuitName() { return suit_ == Suit::HEARTS ? "Hearts" : suit_ == Suit::CLUBS ? "Clubs" : suit_ == Suit::DIAMONDS ? "Diamonds" : "Spades"; }

	private:
		Suit suit_ = Suit::HEARTS;
		Rank rank_ = 1;
		bool is_face_up_ = true;
	};
}
