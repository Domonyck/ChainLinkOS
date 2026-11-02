// Chain Blitz rules (30s timer per player, most words wins)
// No game logic first
// Only establishes the rules

#include <iostream>
#include <chrono>
#include "../client/game.h"
#include "../client/session.h"
using namespace std;

class BlitzMode : public Mode { // Inherits all public methods of Mode
    private:
        int timeLeft[2];            // stopwatch of each player
        int deadline = 0;           // stopwatch object
        int active = -1;            // checks player turn
        bool isRunning = false;     // checks if the stopwatch is running
    
    public:
        // A reference to the game board to read and update the chain, players, hints
        explicit Game (Game &game) {
            // game methods
        }            
        
        // Signals the start of the game, called by session after the coin flip. starts the clock, sets the deadline
        void start(int firstPlayer) {
            // sets the active player
            active = firstPlayer;
            // sets the clock as running
            isRunning = true;

            /*
                – uses steady_clock to acts a stopwatch of 30 seconds
                – now = sets the current clock
                – deadline = notes the deadline of each player, which is +30 seconds to the current clock
                    – if now reaches deadline, then timer is finished
            */
            Clock = chrono::steady_clock;
            Clock::time_point now = Clock::now();
            Clock::time_point deadline = now + chrono::milliseconds(timeLeft[active]);
        }
        
        // determines if the player guess is correct        
        Result on_guess(int player, string word) {
            if (active != firstPlayer) || (isRunning == false) {
                return -1;
            }
            
            

        }       
        
        // handles when stopwatch hits zero
        Result onTimeout();                            
        
        // reads the stopwatch
        const int msUntilDeadline();                  
        
        // checks if the match is finished
        const bool isOver();                           
        
        // determines the winner
        const int winner();                             
}  
