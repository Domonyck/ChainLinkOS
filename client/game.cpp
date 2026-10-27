#include <iostream>
#include <vector>
#include <cassert>
#include <algorithm>
#include <string>
#include <stdexcept>
#include <utility>
#include <cctype>

using namespace std;

// ======== FUNCTIONS ========
string toLower(string str) {
    for (char &ch : str) {
        ch = tolower(static_cast<unsigned char>(ch));
    }
    return str;
}

string ltrim(const string &str) {
    size_t start = str.find_first_not_of(" \n\r\t\f\v");
    return (start == string::npos) ? "" : str.substr(start);
}

string rtrim(const string& str) {
    size_t end = str.find_last_not_of(" \n\r\t\f\v");
    return (end == string::npos) ? "" : str.substr(0, end + 1);
}

string trim(const string &str) {
    return rtrim(ltrim(str));
}

// ======== STRUCTS ========
struct Player {
    size_t position = 0;            // index of the word being guessed
    size_t hintLevel = 0;           // amount of hints used on a word
    int score = 0;                  // correct guesses so far
};

// ======== CLASSES AND MEMBERS ========
class Game {
    private:
        Player players[2];
        vector<string> chain;
    
        const string &expectedWord(int player) const {
            const Player &p = players[player];
            return chain.at(p.position);
        }

        void validatePlayer(int player) const {
            if (player != 0 && player != 1) throw invalid_argument("There should only be two players (0 and 1).");
        }
    
        public:
            // ===== CONSTRUCTOR =====
            Game(vector<string> words) : chain(std::move(words)){

                if (chain.empty()) {
                    throw invalid_argument("Chain is empty.");
                }

                for (size_t i = 0; i < chain.size(); i++) {
                    const string &word = chain.at(i);

                    if (word.empty()) {
                        throw invalid_argument("Chain is incomplete.");
                    }
                }
            }

            // ===== READ-ONLY =====
            string view(int player) const {
                validatePlayer(player);

                string guessingChain = "";
                
                const Player &p = players[player];

                for (size_t i = 0; i < chain.size(); i++) {
                    const string &word = chain.at(i);

                    for (size_t j = 0; j < word.size(); j++) {
                        if (i < p.position) guessingChain += word[j];

                        else if (i == p.position) {
                            if (j <= p.hintLevel) guessingChain += word[j];
                            else guessingChain += '_';
                        }

                        else {
                            guessingChain += '_';
                        }
                    }

                    if (i + 1 < chain.size()) guessingChain += ' ';
                }
                return guessingChain;
            }

            bool isFinished(int player) const {
                validatePlayer(player);
                return players[player].position>= chain.size();
            }

            // ===== READ & WRITE =====
            bool checkGuess(int player, string guess) const {
                validatePlayer(player);
                if (isFinished(player)) return false;
                
                return trim(toLower(guess)) == trim(toLower(expectedWord(player)));
            }

            void advance(int player) {
                validatePlayer(player);
                Player &p = players[player];

                p.score++;
                p.position++;
                p.hintLevel = 0;
            }

            void addHint(int player) {
                validatePlayer(player);
                Player &p = players[player];

                p.hintLevel++;
            }

            // ===== RESULT =====
            int getScore(int player) const {
                validatePlayer(player);
                const Player &p = players[player];

                return p.score;
            }
};

int main () {
    Game g({"beach", "front", "yard"});

    // starting view (both players at word 0, hint 0)
    assert(g.view(0) == "b____ _____ ____");
    assert(g.view(1) == "b____ _____ ____");

    // checkGuess: normalization and wrong answers
    assert(g.checkGuess(0, "beach"));
    assert(g.checkGuess(0, "  BEACH "));
    assert(!g.checkGuess(0, "front"));
    assert(!g.checkGuess(0, ""));

    // addHint reveals one more letter, only for player 0
    g.addHint(0);
    assert(g.view(0) == "be___ _____ ____");
    assert(g.view(1) == "b____ _____ ____");   // per-player proof

    // advance: guessed word shows in full, hint resets
    g.advance(0);
    assert(g.view(0) == "beach f____ ____");
    assert(g.view(1) == "b____ _____ ____");
    assert(g.getScore(0) == 1);
    assert(g.getScore(1) == 0);
    assert(g.checkGuess(0, "front"));          // now expects the 2nd word
    assert(!g.checkGuess(0, "beach"));

    // finishing the chain
    g.advance(0);
    g.advance(0);
    assert(g.view(0) == "beach front yard");
    assert(g.isFinished(0));
    assert(!g.checkGuess(0, "anything"));      // no crash for a finished player
    assert(!g.isFinished(1));

    // bad inputs
    try { g.view(2); assert(false); }
    catch (const invalid_argument&) {}

    try { Game bad(vector<string>{}); assert(false); }
    catch (const invalid_argument&) {}

    cout << "all tests passed\n";
    return 0;
}