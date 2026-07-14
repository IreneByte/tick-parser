// Real-Time Market Data Ingestion Engine
// Multithreaded C++ parser for stock trade data (CSV).
// Computes per-ticker VWAP, volume, and price range analytics.
// Author: Irene R.

#include <iostream> // Printing to console
#include <string> // String text handling
#include <sstream> // String splitting
using namespace std;

struct Trade {
    std::string ticker;
    double price;
    long volume;
    std::string timestamp;
};

// Parses a single CSV trade line into a Trade struct
Trade parseLine(string inputString) {
    Trade trade;
    stringstream ss(inputString);
    string field1, field2, field3, field4;

    getline(ss, field1, ',');
    trade.ticker = field1;

    getline(ss, field2, ',');
    trade.price = stod(field2);

    getline(ss, field3, ',');
    trade.volume = stol(field3);

    getline(ss, field4, ',');
    trade.timestamp = field4;

    return trade;
}

int main()
{
    string testLine = "AAPL,187.32,500,2025-01-15T09:30:01";
    Trade t = parseLine(testLine);
    cout << "Ticker: " << t.ticker << ", Price: " << t.price << ", Volume: " << t.volume << ", Timestamp: " << t.timestamp << endl;
}