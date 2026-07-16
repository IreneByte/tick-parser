// Real-Time Market Data Ingestion Engine
// Multithreaded C++ parser for stock trade data (CSV).
// Computes per-ticker VWAP, volume, and price range analytics.
// Author: Irene R.

#include <iostream> // Printing to console
#include <string> // String text handling
#include <sstream> // String splitting
#include <fstream> // Reading .csv file(s)
#include <vector> // List to hold parsed trades
#include <map> // Key-value lookups

using namespace std;

struct Trade {
    std::string ticker;
    double price;
    long volume; // long covers massive volume sizes without overflow issues
    std::string timestamp; // Expected format: YYYY-MM-DDTHH:MM:SS
};

struct CompanyMetrics {
    double minPrice = -1.0; 
    double maxPrice = -1.0; // -1.0 is a flag meaning "no trades seen yet"
    long totalVolume = 0; 
    double weightedPriceSum = 0.0; // Cumulative (Price * Volume)
};

// Parses a single CSV line into a Trade struct
Trade parseLine(string inputString) {
    Trade trade;
    stringstream ss(inputString);
    string field1, field2, field3, field4;

    getline(ss, field1, ',');
    trade.ticker = field1;

    getline(ss, field2, ',');
    trade.price = stod(field2); // stod will throw an error if data is corrupted

    getline(ss, field3, ',');
    trade.volume = stol(field3); // stol will throw an error if data is corrupted

    getline(ss, field4, ',');
    trade.timestamp = field4;

    return trade;
}

int main()
{
    string filename = "trades.csv";
    ifstream file(filename);

    if (!file.is_open()) {
        // Exit early if the file is missing or corrupted
        cout << "Error: Could not open file!" << endl;
        return 1;

    } else {
        vector<Trade> trades;
        string line;
        map<string, CompanyMetrics> statsMap;
        
        // Read the file line-by-line until the end
        while(getline(file, line)) {
            Trade tempTrade = parseLine(line);
            trades.push_back(tempTrade);
        }

        // Populating Company Metrics for each company
        // const auto& avoids copying the structs in memory on every loop iteration
        for (const auto& t : trades) { 
            cout << "Ticker: " << t.ticker << ", Price: " << t.price << ", Volume: " << t.volume << ", Timestamp: " << t.timestamp << endl;

            // Use & to get the address of the actual metrics in the map so they can be changed directly
            CompanyMetrics& stats = statsMap[t.ticker];

            // Add the current trade's volume and total weighted value to the running sums
            stats.totalVolume += t.volume;
            stats.weightedPriceSum += (t.volume * t.price);

            // If this is the first trade we've seen for this company, set it to our starting min and max
            if (stats.minPrice == -1.0) {
                stats.minPrice = t.price;
                stats.maxPrice = t.price;
            } else {
                // Otherwise, check if this trade's price is a new minimum or maximum in the company's dataset
                if (t.price <= stats.minPrice) {
                    stats.minPrice = t.price;
                } 
                if (t.price >= stats.maxPrice) {
                    stats.maxPrice = t.price;
                }
            }
        }

        // Print final analytics for each unique company in the map
        for (const auto& pair : statsMap) {
            double vwap;
            
            // VWAP = (Total Money Traded) / (Total Shares Traded)
            vwap = pair.second.weightedPriceSum / pair.second.totalVolume;
            cout << "Ticker: " << pair.first 
                 << ", VWAP: " << vwap 
                 << ", Minimum Price: " << pair.second.minPrice 
                 << ", Maximum Price: " << pair.second.maxPrice 
                 << ", Total Volume: " << pair.second.totalVolume << endl;
        }
    }
}