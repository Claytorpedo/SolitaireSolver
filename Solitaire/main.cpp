#include "Deck.h"
#include "KlondikeSolver.h"

#include <iostream>

int main() {
	Solitaire::KlondikeSolver solver(1340);
	solver.Solve();

	return 0;
}

/*

Independent vs dependent moves.

Independent -- order between these moves doesn't matter/affect win chances.
	Don't need to check all possible combinations. Just do the move or don't do the move.
	The goal is to avoid switching around orders of moves that are independent to each other.

	All tableau moves are independent -- both intra-tableau and tableau-to-foundation

	Send down an independent moves list to the recursive function == ignored_moves
		If a newly found move is already in ignored_moves, discard it.
		Make list of own dependent and independent moves.
			Try all dependent and newly found independent moves, sending down a list of (ignored_moves + current layer's independent) moves to ignore.

Dependent   -- order between these moves might matter/affect win chances.

	All (or almost all) stock moves are dependent. Changing the order can change what cards are available.



PrevMove
	Send down the previous move. Depending on the previous move type, we may not need to remake some moves lists for certain types.
	EG If the previous move was stock to foundation, we know we have no new tableau moves and can just use our old ones.

	This would involve sending a lot of separate lists up/down though, and some fancy checking of move types.

	cases:
		Stock to foundation: Reuse full/partial run moves.
		Tableau to foundation or full/partial run moves: Reuse stock to foundation moves.
		Repile stock: Reuse Tableau to foundation and full/partial run moves.

*/