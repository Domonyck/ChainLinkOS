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

/*
    lowers a string using cctype tolower for input validation
*/
string toLower(string str) {
    for (char &ch : str) {
        ch = tolower(static_cast<unsigned char>(ch));
    }
    return str;
}

/*
    trims all the whitespaces inside a string for input validation
*/
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

/*
    Player struct that stores all needed variables a player needs in the game
*/
struct Player {
    vector<string> chain;           // this player's own chain of words
    size_t position = 0;            // index of the word being guessed in the list
    size_t hintLevel = 0;           // amount of hints used on a word
    int score = 0;                  // correct guesses so far
};

// ======== CLASSES AND MEMBERS ========
class Game {
    private:
        Player players[2];

        /*
            reads the unmasked word at position i
            in private because it is not supposed to be accessed by any other methods
        */
        const string &expectedWord(int player) const {
            const Player &p = players[player];
            return p.chain.at(p.position);
        }

        /*
            validates if the player is within bounds or 0 and 1.
        */
        void validatePlayer(int player) const {
            if (player != 0 && player != 1) throw invalid_argument("There should only be two players (0 and 1).");
        }
    
        public:
            // ===== CONSTRUCTOR =====

            /*
                Constructor that takes one chain per player from word_bank.cpp, validates them and immediately stores after running
            */
            Game(vector<string> words0, vector<string> words1) {

                players[0].chain = std::move(words0);
                players[1].chain = std::move(words1);

                for (int k = 0; k < 2; k++) {
                    const vector<string> &chain = players[k].chain;

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
            }

            // ===== READ-ONLY =====

            /*
                Views the player view of the chain
                What it does:
                    Masks the words that is not yet reached by the hintLevel and position
                    Reveals all the words already guessed and is before the hintLevel
            */
            string view(int player) const {
                // Validates the player using the validatePlayer function
                validatePlayer(player);

                // Initializes the chain the player is given
                string guessingChain = "";
                
                // Initializes the player
                const Player &p = players[player];

                // loops through the chain (list of words) and masks or unmasks the words and letters depending on the player's current gameplay
                for (size_t i = 0; i < p.chain.size(); i++) {
                    // Initializes the current word being guessed by the player
                    const string &word = p.chain.at(i);

                    // Unmasks or masks each letter of the word based on hintLevel and position
                    for (size_t j = 0; j < word.size(); j++) {
                        // Unmask all the words (through letters) already guessed by the player i.e. those before the current index of the player along the chain
                        if (i < p.position) guessingChain += word[j];

                        // Unmask a letter in the word if it lower than the current hintLevel, otherwise mask
                        else if (i == p.position) {
                            if (j <= p.hintLevel) guessingChain += word[j];
                            else guessingChain += '_';
                        }

                        // Otherwise, mask the words (through letters)
                        else {
                            guessingChain += '_';
                        }
                    }

                    // For every word in the chain put it in its own line
                    if (i + 1 < p.chain.size()) guessingChain += '\n';
                }
                return guessingChain;
            }

            /*
                checks if the player has guessed all the words in the chain
            */
            bool isFinished(int player) const {
                validatePlayer(player);
                return players[player].position >= players[player].chain.size();
            }

            // ===== READ & WRITE =====
            
            /*
                checks if player guess matches the expected word
            */
            bool checkGuess(int player, string guess) const {
                validatePlayer(player);
                if (isFinished(player)) return false;
                
                return trim(toLower(guess)) == trim(toLower(expectedWord(player)));
            }

            /*
                advances the player to the next word
            */
            void advance(int player) {
                validatePlayer(player);
                Player &p = players[player];

                p.score++;
                p.position++;
                p.hintLevel = 0;
            }

            /*
                adds hint if the player answers incorrectly (not handled by game.cpp)
            */
            void addHint(int player) {
                validatePlayer(player);
                Player &p = players[player];

                p.hintLevel++;
            }

            // ===== RESULT =====

            /*
                gets the number of correct words by the player
            */
            int getScore(int player) const {
                validatePlayer(player);
                const Player &p = players[player];

                return p.score;
            }
};