// Real-Time Market Data Ingestion Engine
// Multithreaded C++ parser for stock trade data (CSV).
// Computes per-ticker VWAP, volume, and price range analytics.
// Author: Irene R.

#include <iostream> // Printing to console
#include <string> // String text handling
#include <sstream> // String splitting
#include <fstream> // Reading .csv file(s)
#include <vector> // List to hold parsed trades
#include <unordered_map> // Key-value lookups
#include <algorithm> // Min and max
#include <regex> // For pattern matching
#include <thread> // Multiple threads
#include <chrono> // Single vs multi-threaded benchmark
#include <iomanip> // Formatting decimals

using namespace std;

// Holds data for a single trade record
struct Trade {
    std::string ticker;
    double price;
    long volume; // long covers massive volume sizes without overflow issues
    std::string timestamp; // Expected format: YYYY-MM-DDTHH:MM:SS
};

// Holds calculated metrics for a stock ticker
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
        
        return Trade{"", 0.0, 0, ""};
    }
}

// Reads the CSV file and loads valid records into memory
vector<Trade> loadTrades(const string& filename) {
    vector<Trade> trades;
    int skippedCount = 0;
    ifstream file(filename);
    ofstream errorLog("errors.csv");

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
        } else {
            skippedCount++;
            errorLog << line << endl;
        }
    }

    cout << "Skipped " << skippedCount << " malformed rows." << endl;
    this_thread::sleep_for(chrono::seconds(5));

    return trades;
}

// Calculates stock analytics from a collection of trades
unordered_map<string, CompanyMetrics> computeMetrics(const vector<Trade>& trades) {
    unordered_map<string, CompanyMetrics> statsMap;

    // Populating Company Metrics for each company
    // const auto& avoids copying the structs in memory on every loop iteration
    for (const Trade& t : trades) { 
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

// Displays final calculated metrics to the screen
void printReport(const unordered_map<string, CompanyMetrics>& statsMap) {
    cout << left << setw(8) << "TICKER"
            << right << setw(12) << "VWAP"
            << setw(12) << "MIN"
            << setw(12) << "MAX"
            << setw(14) << "VOLUME" << endl;

    // Print final analytics for each unique company in the map
    for (const auto& pair : statsMap) {
        double vwap = 0.0;
        
        // VWAP = (Total Money Traded) / (Total Shares Traded)
        if (pair.second.totalVolume > 0) {
            vwap = pair.second.weightedPriceSum / pair.second.totalVolume;
        }

        cout << left  << setw(8)  << pair.first
             << right << setw(12) << vwap
             << setw(12) << pair.second.minPrice
             << setw(12) << pair.second.maxPrice
             << setw(14) << pair.second.totalVolume << endl;
    }
}

// Splits data into equal parts for the threads
vector<vector<Trade>> partitionTrades(const vector<Trade>& trades, int numThreads) {
    vector<vector<Trade>> threads;
    int threadSize = trades.size() / numThreads;

    for (int i = 0; i < numThreads; i++) {
        int start = i * threadSize;
        int end = (i * threadSize) + threadSize;

        if (trades.size() - end <= threadSize) {
            end = trades.size();
        }

        vector<Trade> partition(trades.begin() + start, trades.begin() + end);
        threads.push_back(partition);
    }

    return threads;
}

// Runs calculations on one assigned partition
void processPartition(vector<Trade>& trades, unordered_map<string, CompanyMetrics>& result) {
    result = computeMetrics(trades);
}

// Merges all thread maps into one final map
unordered_map<string, CompanyMetrics> mergeResults(vector<unordered_map<string, CompanyMetrics>> partitions, int numWorkers) {
    unordered_map<string, CompanyMetrics> result;

    // Iterate through each partition in the split vector
    for (unordered_map<string, CompanyMetrics>& p : partitions) {
        // Iterate through each pair in each map
        for (const pair<string, CompanyMetrics> t : p) {
            string ticker = t.first;
            if (result.count(ticker) == 0) { // Add new aggregated stock if not in map
                result.insert(t);
            } else { // Combine totals if stock exists
                result[ticker].totalVolume += t.second.totalVolume;
                result[ticker].weightedPriceSum += t.second.weightedPriceSum;
                result[ticker].minPrice = min(t.second.minPrice, result[ticker].minPrice);
                result[ticker].maxPrice = max(t.second.maxPrice, result[ticker].maxPrice);
            }
        }
    }

    return result;
}

unordered_map<string, CompanyMetrics> runMultithreaded(const vector<Trade>& trades, int numWorkers) {
    vector<vector<Trade>> partitions = partitionTrades(trades, numWorkers);
    vector<unordered_map<string, CompanyMetrics>> partition_results(numWorkers);

    vector<thread> workers;
    for (int i = 0; i < numWorkers; i++) {
        // Pass references directly to prevent copying data
        workers.push_back(thread(processPartition, ref(partitions[i]), ref(partition_results[i])));
    }

    for (thread& w : workers) {
        // Joins background thread execution back into main
        w.join();
    }

    // Merge the results of the workers
    return mergeResults(partition_results, numWorkers);
}

void runBenchmark(const vector<Trade>& trades) {
    double baselineTime, speedup;

    for (int n : {1, 2, 4, 8}) {
        auto start = chrono::high_resolution_clock::now();
        runMultithreaded(trades, n);
        auto end = chrono::high_resolution_clock::now();
        chrono::duration<double> elapsed = end - start;
        baselineTime = (n == 1) ? elapsed.count() : baselineTime;
        speedup = (n != 1) ? baselineTime / elapsed.count() : 1.00;
        cout << "Threads: " << n << ", Time: " << fixed << setprecision(3) << elapsed.count() << " seconds, Speedup: " << speedup << "x" << endl;
    }
}

void runDashboard(const vector<Trade>& trades, int numWorkers) {
    int i = 1;

    while (true) {
        int end = trades.size() * i / 10;

        if (end > trades.size()) {
            end = trades.size();
        }

        vector<Trade> partialData(trades.begin(), trades.begin() + end);
        unordered_map<string, CompanyMetrics> merged = runMultithreaded(partialData, numWorkers);

        system("cls");
        printReport(merged);
        this_thread::sleep_for(chrono::milliseconds(500));
   
        i++;
    }
}

// Runs the program from start to finish
int main()
{
    vector<Trade> trades = loadTrades("trades.csv");
    runDashboard(trades, 4);
    // runBenchmark(trades);
}