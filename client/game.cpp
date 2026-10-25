#include <iostream>
#include <vector>
#include <string>
// #include "word_bank.h"
using namespace std;

struct Player {
    int position;   // index of the word being guessed
    int hintLevel; // amount of hints used on a word
    int score;      // correct guesses so far
    int players[2];     // current player
    vector<string> chain = {"apple", "banana", "carrot"};   // vector of words
};

class Game {
    private:
        const &string Game::expectedWord(int player) const {
            return chain[players[player].position];
        }
    public:
        Game() {
            // constructor takes the finished chain from word_bank
        }

        const string Game::view(int player) {
            return getGuessingChain(chain, players[player].position, players[player].hintLevel);
        }

        const bool Game::isPlayerFinished(vector<string> chain, int positon) {
            if (position >= chain.size() - 1) return true;
        }

        // returns the already-guessed words in full, plus the next word masked as its first letter and any hints.
        static string Game::getGuessingChain(vector<string>chain, int position, int hintLevel) {
            string guessingChain = "";
            
            for (int i = 0; i < chain.size(); i++) {
                const string &word = chain.at(i);
                
                for (int j = 0; j < word.size(); j++) {
                    if (i < position) guessingChain += word[j];

                    else if (i == position) {
                        if (j <= hintLevel) guessingChain += word[j]; // the char before the hint level are revealed
                        else guessingChain += "_";  // the char after is masked
                    }
                    
                    else {
                        guessingChain += "_";
                    }
                }
                
                if (i + 1 < chain.size()) guessingChain += " ";
            }
            return ;
        }

        bool checkGuess(vector<string> chain, string guess, int position) {
            for (int i = 0; i < )
        }
        
};

int main() {
    vector<string> chain = {"cat", "dog", "bird"};
    cout << Game::getGuessingChain(chain, 1, 1) << "\n";    // "cat do_ ____"
    cout << Game::getGuessingChain(chain, 0, 0) << "\n";    // "c__ ___ ____"
    cout << Game::getGuessingChain(chain, 2, 10) << "\n";   // "cat dog bird"
    cout << Game::getGuessingChain(chain, 3, 0) << "\n";    // "cat dog bird"
    cout << Game::getGuessingChain({}, 0, 0) << "\n";       // ""
}