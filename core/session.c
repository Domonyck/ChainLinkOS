#include <iostream>
#include <vector>
#include "../client/word_bank.h"

using namespace std;

class Session {
    private:
        int fds[2];
        GameMode mode = GameMode::Classic;              // from word_bank
        ListSource source = ListSource::GameMaster;     // from word_bank
        vector<string> chain;                           

        GameMode askMode();                             // from word_bank
        ListSource = askListSource();                   // from word_bank
        vector<string> askPlayerChain();                // from word_bank

        void setup() {
            mode = askMode();
            
            if (mode == GameMode::Blitz) {
                chain = randomChain(GameMode::Blitz);   
            }
            // Mode == classic
            else { 
                // Ask player if game master or player will provide list
                source = askListSource();
                if (source == ListSource::GameMaster) {     
                    chain = randomChain(GameMode::Classic); 
                } else {
                    chain = askPlayerChain();
                }

            }
        }
    
        public:
            Session(int fd0, int fd1) : fds{fd0, fd1} {}
            void run { setup(); }
};

int main () {
    Session session(-1, -1);
    
    session.run();
    return 0;
}