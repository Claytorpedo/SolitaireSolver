# SolitaireSolver
Determines whether solitaire games are solvable, and can give solutions.

Games are made from seeds wich will generate a shuffled deck of cards. Running the solver with the `--write-decks` option will instead create a game file containing these generated decks. (At current there is no option to run a game on a pre-defined deck or board state, though it would not be terribly difficult to implement.)

The solver will run through a list or range of seeds and output which are solvable or not, as well as statistics for the current run. A "batch" denotes how many seeds to try solving before writing out stats -- essentially saving your progress.

Run with `-?` for a list of options.

### Building
`git clone --recursive git@github.com:Claytorpedo/SolitaireSolver.git`

Build with Solitaire.sln on windows, or make on Linux with `make debug` or `make release`. In this case, the release build optimizations make a huge difference in solving speed.

### Ruleset
The solver is currently set up to solve Klondike games with the following rules:
- 3 card draw (easy to change)
- Cards moved to the foundation cannot be moved back to the tableau (more challenging to change)
