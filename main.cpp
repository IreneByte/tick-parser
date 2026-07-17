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
#include <algorithm> // Min and max
#include <regex> // For pattern matching

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
    try {
        Trade trade;
        stringstream ss(inputString);
        string field1, field2, field3, field4;
        regex pattern(R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}$)");

        if( !getline(ss, field1, ',') || 
            !getline(ss, field2, ',') ||
            !getline(ss, field3, ',') || 
            !getline(ss, field4, ',')) 
        {
            throw runtime_error("Missing fields");
        }

        trade.ticker = field1;
        trade.price = stod(field2); // stod will throw an error if data is corrupted
        trade.volume = stol(field3); // stol will throw an error if data is corrupted
        trade.timestamp = field4;

        // If price is less than 0, or volume is less than 0, throw an error
        if (trade.price < 0 || trade.volume < 0) {
            throw invalid_argument("Negative price or volume");
        }

        if (trade.ticker.empty()) {
            throw invalid_argument("Ticker is empty");
        }

        if (trade.timestamp.length() != 19) {
            throw invalid_argument("Incorrect timestamp format");
        }

        if (!std::regex_match(trade.timestamp, pattern)) {
            throw std::invalid_argument("Malformed timestamp format");
        }

        return trade;
    } catch (const exception& e) {
        // Catch errors with e.what(), automatically outputting the reason for the error message
        cout << "Skipping bad row. Reason: " << e.what() << endl;
        
        return Trade{"", 0.0, 0, ""};
    }
}

vector<Trade> loadTrades(const string& filename) {
    vector<Trade> trades;
    ifstream file(filename);

    if (!file.is_open()) {
        // Exit early if the file is missing or corrupted
        throw runtime_error("Error: Could not open file!");
    }

    string line;
    // Read the file line-by-line until the end
    while(getline(file, line)) {
        Trade tempTrade = parseLine(line);
        if (tempTrade.ticker != "") {
            trades.push_back(tempTrade);
        }
    }

    return trades;
}

map<string, CompanyMetrics> computeMetrics(const vector<Trade>& trades) {
    map<string, CompanyMetrics> statsMap;

    // Populating Company Metrics for each company
    // const auto& avoids copying the structs in memory on every loop iteration
    for (const auto& t : trades) { 
        cout << "Ticker: " << t.ticker << ", Price: " << t.price << ", Volume: " << t.volume << ", Timestamp: " << t.timestamp << endl;

        // Use & to get the address of the actual metrics in the map so they can be changed directly
        CompanyMetrics& stats = statsMap[t.ticker];

        // Add the current trade's volume and total weighted value to the running sums
        stats.totalVolume += t.volume;
        stats.weightedPriceSum += (t.volume * t.price);

        stats.minPrice = (stats.minPrice == -1.0) ? t.price : min(stats.minPrice, t.price);
        stats.maxPrice = (stats.maxPrice == -1.0) ? t.price : max(stats.maxPrice, t.price);
    }

    return statsMap;
}

void printReport(const map<string, CompanyMetrics>& statsMap) {
    // Print final analytics for each unique company in the map
    for (const auto& pair : statsMap) {
        double vwap = 0.0;
        
        // VWAP = (Total Money Traded) / (Total Shares Traded)
        if (pair.second.totalVolume > 0) {
            vwap = pair.second.weightedPriceSum / pair.second.totalVolume;
        }
        
        cout << "Ticker: " << pair.first 
                << ", VWAP: " << vwap 
                << ", Minimum Price: " << pair.second.minPrice 
                << ", Maximum Price: " << pair.second.maxPrice 
                << ", Total Volume: " << pair.second.totalVolume << endl;
    }
}

int main()
{
    vector<Trade> trades = loadTrades("trades.csv");
    map<string, CompanyMetrics> statsMap = computeMetrics(trades);
    printReport(statsMap);
}