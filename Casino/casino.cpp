#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <random>
#include <chrono>
#include <atomic>
#include <iomanip> // Add this for formatting

// Casino class to manage the bank and track statistics
class Casino {
private:
    double bankBalance;         //how much player has
    double playerWinnings;     // how much player has won.
    std::mutex bankMutex;     // protects bankBalance and playerWinnings from being accessed at the same time.
    
    // Only one thread can access these at a time because they're atomic.
    std::atomic<int> rouletteCount{0};   // Keeps track of how many time roulette has been played.
    std::atomic<int> blackjackCount{0};  // How many times blackJack has been played
    std::atomic<int> crapsCount{0};     // How many times Craps has been played.
    
public:
    Casino(double initialBalance) : bankBalance(initialBalance), playerWinnings(0) {}
    
    // Thread-safe bank access
    bool updateBalance(double amount) {
        std::lock_guard<std::mutex> lock(bankMutex);
        
        // Track player winnings (negative means casino wins)
        playerWinnings += amount;
        
        // Update casino balance (opposite of player winnings)
        bankBalance -= amount;
        
        return true;
    }
    
    void incrementRoulette() { rouletteCount++; }
    void incrementBlackjack() { blackjackCount++; }
    void incrementCraps() { crapsCount++; }
    
    // Print final statistics with fixed formatting
    void printStats() const {
        std::cout << "===== CASINO STATISTICS =====" << std::endl;
        std::cout << "Total plays:" << std::endl;
        std::cout << "  Roulette: " << rouletteCount << std::endl;
        std::cout << "  Blackjack: " << blackjackCount << std::endl;
        std::cout << "  Craps: " << crapsCount << std::endl;
        std::cout << std::endl;
        
        // Use fixed notation instead of scientific notation
        std::cout << std::fixed;
        std::cout << "Player winnings: " << static_cast<int>(playerWinnings) << " credits" << std::endl;
        std::cout << "Casino balance: " << static_cast<int>(bankBalance) << " credits" << std::endl;
    }
};

// Random number generator function
double getRandomAmount(int min, int max) {
    static thread_local std::mt19937 generator(std::chrono::system_clock::now().time_since_epoch().count());
    std::uniform_real_distribution<double> distribution(min, max);
    return distribution(generator);
}

// Game functions
void playRoulette(Casino& casino, int iterations) {
    for (int i = 0; i < iterations; i++) {
        double amount = getRandomAmount(-1000, 1000);
        casino.updateBalance(amount);
        casino.incrementRoulette();
    }
}

void playBlackjack(Casino& casino, int iterations) {
    for (int i = 0; i < iterations; i++) {
        double amount = getRandomAmount(-500, 500);
        casino.updateBalance(amount);
        casino.incrementBlackjack();
    }
}

void playCraps(Casino& casino, int iterations) {
    for (int i = 0; i < iterations; i++) {
        double amount = getRandomAmount(-1000, 500);
        casino.updateBalance(amount);
        casino.incrementCraps();
    }
}

void casinoThread(Casino& casino, int iterations){
    // Create threads for each game
    for (int i = 0; i < iterations; i++){
        std::thread rouletteThread(playRoulette, std::ref(casino), 1000);
        std::thread blackjackThread(playBlackjack, std::ref(casino), 100);
        std::thread crapsThread(playCraps, std::ref(casino), 500);
            // Wait for all games to complete
        rouletteThread.join();
        blackjackThread.join();
        crapsThread.join();
    }
}

int main() {
    // Initialize casino with 1 million credits
    Casino casino(1000000);

    casinoThread(casino, 1000);
    
    // Print final statistics
    casino.printStats();
    
    return 0;
}