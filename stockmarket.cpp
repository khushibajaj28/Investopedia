// Stock Market Trading Platform
// Console-based simulator for buying and selling stocks

#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <ctime>
#include <cstdlib>
#include <map>
#include <algorithm>
#include <limits>
#include <thread>
#include <chrono>
#include <cmath>
#include <sstream>
#include <cctype>
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
using namespace std;
string trim(string text) {
    while (!text.empty() && (text.back() == '\r' || text.back() == '\n' || text.back() == ' ' || text.back() == '\t')) {
        text.pop_back();
    }

    size_t start = 0;
    while (start < text.size() && (text[start] == ' ' || text[start] == '\t')) {
        start++;
    }

    return text.substr(start);
}
// ================= FILE NAMES =================
const string STOCKS_FILE = "stock_data.csv";
const string TRADES_FILE = "trades.csv";
const string PORTFOLIO_FILE = "portfolio.txt";

// ================= COLOR SETUP =================
#ifdef _WIN32
class ConsoleColor {
private:
    HANDLE hConsole;

public:
    ConsoleColor() {
        hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    }

    void setColor(int color) {
        SetConsoleTextAttribute(hConsole, color);
    }

    void reset() {
        SetConsoleTextAttribute(hConsole, 7);
    }
};

ConsoleColor consoleColor;

#define RED consoleColor.setColor(12)
#define GREEN consoleColor.setColor(10)
#define YELLOW consoleColor.setColor(14)
#define CYAN consoleColor.setColor(11)
#define MAGENTA consoleColor.setColor(13)
#define WHITE consoleColor.setColor(15)
#define BLUE consoleColor.setColor(9)
#define RESET consoleColor.reset()
#else
#define RED cout << "\033[31m"
#define GREEN cout << "\033[32m"
#define YELLOW cout << "\033[33m"
#define CYAN cout << "\033[36m"
#define MAGENTA cout << "\033[35m"
#define WHITE cout << "\033[37m"
#define BLUE cout << "\033[34m"
#define RESET cout << "\033[0m"
#endif

// ================= HELPERS =================
void sleepMilliseconds(int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}
void showLoading(const string& message) {
    cout << "\n" << message;
    for (int i = 0; i < 3; i++) {
        cout << ".";
        cout.flush();
        sleepMilliseconds(250);
    }
    cout << " done\n";
}
string formatMoney(double amount) {
    ostringstream out;
    out << "Rs. " << fixed << setprecision(2) << amount;
    return out.str();
}
string formatSignedMoney(double amount) {
    ostringstream out;
    if (amount >= 0) {
        out << "+Rs. " << fixed << setprecision(2) << amount;
    } else {
        out << "-Rs. " << fixed << setprecision(2) << fabs(amount);
    }
    return out.str();
}
string formatSignedNumber(double value) {
    ostringstream out;
    if (value > 0) {
        out << "+";
    }
    out << fixed << setprecision(2) << value;
    return out.str();
}
string getCurrentTimeString() {
    time_t now = time(nullptr);
    char buf[64];

#if defined(_MSC_VER)
    tm localTm;
    localtime_s(&localTm, &now);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &localTm);
#else
    tm* localTm = localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localTm);
#endif
    return string(buf);
}
void toUpperCase(string& text) {
    transform(text.begin(), text.end(), text.begin(),
              [](unsigned char c) { return static_cast<char>(toupper(c)); });
}
void printDivider(int width = 78, char ch = '=') {
    cout << string(width, ch) << "\n";
}
string makeGraphBar(double changePercent) {
    int barLength = static_cast<int>(round(fabs(changePercent) * 2.5));
    if (barLength < 2) {
        barLength = 2;
    }
    string bar;
    for (int i = 0; i < barLength - 1; i++) {
        bar += "~";
    }
    bar += ">";
    return bar;
}
// ================= STOCK =================
struct Stock {
    string symbol;
    string name;
    double price;
    double change;
    double changePercent;
    int volume;

    Stock(string s, string n, double p)
        : symbol(s), name(n), price(p), change(0.0), changePercent(0.0), volume(1000000) {}

    void updatePrice() {
        double oldPrice = price;
        double percentMove = ((rand() % 200) - 100) / 1000.0;
        price += price * percentMove;
        if (price < 1.0) {
            price = 1.0;
        }
        change = price - oldPrice;
        changePercent = (change / oldPrice) * 100.0;

        volume += (rand() % 50000) - 25000;
        if (volume < 100000) {
            volume = 100000;
        }
    }
};

// ================= TRADE =================
struct Trade {
    string symbol;
    string type;
    int quantity;
    double price;
    double total;
    string time;
};

// ================= PORTFOLIO =================
class Portfolio {
private:
    double balance;
    double initialBalance;
    map<string, int> holdings;
    vector<Trade> tradeHistory;

    void appendTradeToFile(const Trade& trade) {
        bool fileExists = false;
        ifstream checkFile(TRADES_FILE);
        fileExists = checkFile.good();
        checkFile.close();

        ofstream file(TRADES_FILE, ios::app);
        if (!file.is_open()) {
            cout << "Could not open " << TRADES_FILE << " to save trade.\n";
            return;
        }

        if (!fileExists) {
            file << "type,symbol,quantity,price,total,time\n";
        }

        file << trade.type << ","
             << trade.symbol << ","
             << trade.quantity << ","
             << fixed << setprecision(2) << trade.price << ","
             << fixed << setprecision(2) << trade.total << ","
             << trade.time << "\n";

        file.close();
    }

public:
    Portfolio(double startBalance = 100000.0)
        : balance(startBalance), initialBalance(startBalance) {}

    double getBalance() const {
        return balance;
    }
    double getInitialBalance() const {
        return initialBalance;
    }

    const map<string, int>& getHoldings() const {
        return holdings;
    }

    const vector<Trade>& getHistory() const {
        return tradeHistory;
    }

    void loadPortfolioFromFile() {
        ifstream file(PORTFOLIO_FILE);
        if (!file.is_open()) {
            return;
        }

        holdings.clear();
        string line;

        while (getline(file, line)) {
            if (line.empty()) {
                continue;
            }

            size_t pos = line.find('=');
            if (pos == string::npos) {
                continue;
            }

            string key = line.substr(0, pos);
            string value = line.substr(pos + 1);

            if (key == "balance") {
                balance = stod(value);
            } else {
                holdings[key] = stoi(value);
            }
        }
        file.close();
    }

    void savePortfolioToFile() const {
        ofstream file(PORTFOLIO_FILE);
        if (!file.is_open()) {
            cout << "Could not open " << PORTFOLIO_FILE << " to save portfolio.\n";
            return;
        }

        file << "balance=" << fixed << setprecision(2) << balance << "\n";

        for (const auto& holding : holdings) {
            file << holding.first << "=" << holding.second << "\n";
        }

        file.close();
    }

    void loadTradeHistoryFromFile() {
        ifstream file(TRADES_FILE);
        if (!file.is_open()) {
            return;
        }

        tradeHistory.clear();

        string line;
        getline(file, line); // skip header

        while (getline(file, line)) {
            if (line.empty()) {
                continue;
            }

            stringstream ss(line);
            Trade trade;
            string quantityStr, priceStr, totalStr;

            getline(ss, trade.type, ',');
            getline(ss, trade.symbol, ',');
            getline(ss, quantityStr, ',');
            getline(ss, priceStr, ',');
            getline(ss, totalStr, ',');
            getline(ss, trade.time);
            if (trade.type.empty() || trade.symbol.empty() ||
                quantityStr.empty() || priceStr.empty() || totalStr.empty()) {
                continue;
            }
            trade.quantity = stoi(quantityStr);
            trade.price = stod(priceStr);
            trade.total = stod(totalStr);
            tradeHistory.push_back(trade);
        }
        file.close();
    }
    bool buyStock(const Stock& stock, int quantity) {
        double cost = stock.price * quantity;
        if (quantity <= 0 || cost > balance) {
            return false;
        }
        balance -= cost;
        holdings[stock.symbol] += quantity;

        Trade trade;
        trade.symbol = stock.symbol;
        trade.type = "BUY";
        trade.quantity = quantity;
        trade.price = stock.price;
        trade.total = cost;
        trade.time = getCurrentTimeString();

        tradeHistory.push_back(trade);
        appendTradeToFile(trade);
        savePortfolioToFile();
        return true;
    }

    bool sellStock(const Stock& stock, int quantity) {
        auto it = holdings.find(stock.symbol);

        if (quantity <= 0 || it == holdings.end() || it->second < quantity) {
            return false;
        }

        double revenue = stock.price * quantity;
        balance += revenue;
        it->second -= quantity;

        if (it->second == 0) {
            holdings.erase(it);
        }

        Trade trade;
        trade.symbol = stock.symbol;
        trade.type = "SELL";
        trade.quantity = quantity;
        trade.price = stock.price;
        trade.total = revenue;
        trade.time = getCurrentTimeString();

        tradeHistory.push_back(trade);
        appendTradeToFile(trade);
        savePortfolioToFile();
        return true;
    }

    double getPortfolioValue(const vector<Stock>& stocks) const {
        double total = balance;

        for (const auto& holding : holdings) {
            for (const auto& stock : stocks) {
                if (stock.symbol == holding.first) {
                    total += holding.second * stock.price;
                    break;
                }
            }
        }
        return total;
    }
};
// ================= MARKET =================
class Market {
private:
    vector<Stock> stocks;

    void loadDefaultStocks() {
    cout << "stock_data.csv not found or is empty.\n";
}
public:
    Market() {
        loadStocksFromFile();
    }

    void loadStocksFromFile() {
    stocks.clear();

    ifstream file(STOCKS_FILE);
    if (!file.is_open()) {
        cout << "Could not open file: " << STOCKS_FILE << endl;
        loadDefaultStocks();
        return;
    }

    string line;

    // header read
    if (!getline(file, line)) {
        cout << "File is empty.\n";
        file.close();
        loadDefaultStocks();
        return;
    }

    cout << "Header: " << line << endl;

    while (getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        cout << "Raw row: " << line << endl;

        stringstream ss(line);
        string symbol, name, priceStr;

        getline(ss, symbol, ',');
        getline(ss, name, ',');
        getline(ss, priceStr, ',');

        cout << "Parsed -> "
             << "symbol = [" << symbol << "], "
             << "name = [" << name << "], "
             << "price = [" << priceStr << "]" << endl;

        if (symbol.empty() || name.empty() || priceStr.empty()) {
            cout << "Skipped: incomplete row\n";
            continue;
        }

        try {
            double price = stod(priceStr);
            stocks.push_back(Stock(symbol, name, price));
            cout << "Loaded stock: " << symbol << endl;
        } catch (...) {
            cout << "Skipped: invalid price in row\n";
            continue;
        }
    }

    file.close();

    cout << "Total stocks loaded: " << stocks.size() << endl;

    if (stocks.empty()) {
        cout << "No valid stock data found in " << STOCKS_FILE << ". Loading default stocks.\n";
        loadDefaultStocks();
    }
}

    void updatePrices() {
        for (auto& stock : stocks) {
            stock.updatePrice();
        }
    }

    void displayMarket() const {
        CYAN;
        cout << "\n";
        printDivider(114);
        cout << setw(66) << right << "LIVE MARKET DATA" << "\n";
        printDivider(114);
        RESET;

        cout << left
             << setw(14) << "SYMBOL"
             << setw(30) << "COMPANY"
             << setw(16) << "PRICE"
             << setw(14) << "CHANGE"
             << setw(12) << "%CHANGE"
             << "TREND" << "\n";

        cout << string(114, '-') << "\n";

        for (const auto& stock : stocks) {
            WHITE;
            cout << left
                 << setw(14) << stock.symbol
                 << setw(30) << stock.name.substr(0, 29);

            CYAN;
            cout << setw(16) << formatMoney(stock.price);

            ostringstream changeStream, percentStream;
            changeStream << fixed << setprecision(2) << stock.change;
            percentStream << fixed << setprecision(2) << stock.changePercent;

            if (stock.change > 0) {
                GREEN;
            } else if (stock.change < 0) {
                RED;
            } else {
                YELLOW;
            }

            cout << setw(14) << changeStream.str()
                 << setw(12) << percentStream.str()
                 << makeGraphBar(stock.changePercent);

            RESET;
            cout << "\n";
        }
    }

    Stock* findStock(const string& symbol) {
        for (auto& stock : stocks) {
            if (stock.symbol == symbol) {
                return &stock;
            }
        }
        return nullptr;
    }

    const vector<Stock>& getStocks() const {
        return stocks;
    }
};

// ================= UI FUNCTIONS =================
void showRecentActivity(const Portfolio& portfolio, int count = 5) {
    const auto& history = portfolio.getHistory();

    CYAN;
    cout << "\nRECENT ACTIVITY\n";
    cout << string(78, '-') << "\n";
    RESET;

    if (history.empty()) {
        YELLOW;
        cout << "No trades yet.\n";
        RESET;
        return;
    }

    int start = max(0, static_cast<int>(history.size()) - count);

    cout << left
         << setw(10) << "TYPE"
         << setw(12) << "SYMBOL"
         << setw(10) << "QTY"
         << setw(16) << "PRICE"
         << setw(16) << "TOTAL"
         << "TIME" << "\n";

    cout << string(78, '-') << "\n";

    for (int i = start; i < static_cast<int>(history.size()); i++) {
        const auto& trade = history[i];
        if (trade.type == "BUY") {
            GREEN;
        } 
        else {
            RED;
        }

        cout << left
             << setw(10) << trade.type
             << setw(12) << trade.symbol
             << setw(10) << trade.quantity
             << setw(16) << formatMoney(trade.price)
             << setw(16) << formatMoney(trade.total)
             << trade.time << "\n";
        RESET;
    }
}

void displayMainMenu(const Portfolio& portfolio, const Market& market) {
    double portfolioValue = portfolio.getPortfolioValue(market.getStocks());
    double profitLoss = portfolioValue - portfolio.getInitialBalance();

    BLUE;
    cout << "\n";
    printDivider(78);
    cout << setw(50) << right << "INVESTOPEDIA" << "\n";
    printDivider(78);
    RESET;

    cout << "Cash Balance    : " << formatMoney(portfolio.getBalance()) << "\n";
    cout << "Portfolio Value : " << formatMoney(portfolioValue) << "\n";
    cout << "Profit / Loss   : ";

    if (profitLoss >= 0) {
        GREEN;
        cout << formatSignedMoney(profitLoss) << "\n";
    } else {
        RED;
        cout << formatSignedMoney(profitLoss) << "\n";
    }
    RESET;

    cout << "\nMENU\n";
    cout << "1. View Market\n";
    cout << "2. View Portfolio\n";
    cout << "3. Buy Stocks\n";
    cout << "4. Sell Stocks\n";
    cout << "5. Trade History\n";
    cout << "6. Market Analysis\n";
    cout << "7. View Stock Graph\n";
    cout << "8. Exit\n";

    showRecentActivity(portfolio, 5);
}

void displayPortfolio(const Portfolio& portfolio, const Market& market) {
    const auto& holdings = portfolio.getHoldings();
    const auto& stocks = market.getStocks();
    double portfolioValue = portfolio.getPortfolioValue(stocks);

    CYAN;
    cout << "\n";
    printDivider(78);
    cout << setw(45) << right << "MY PORTFOLIO" << "\n";
    printDivider(78);
    RESET;

    cout << "Cash Balance    : " << formatMoney(portfolio.getBalance()) << "\n";
    cout << "Portfolio Value : " << formatMoney(portfolioValue) << "\n\n";

    if (holdings.empty()) {
        YELLOW;
        cout << "You do not own any stocks yet.\n";
        RESET;
        return;
    }

    cout << left
         << setw(14) << "SYMBOL"
         << setw(12) << "SHARES"
         << setw(18) << "PRICE"
         << setw(18) << "TOTAL VALUE" << "\n";

    cout << string(62, '-') << "\n";

    for (const auto& holding : holdings) {
        for (const auto& stock : stocks) {
            if (stock.symbol == holding.first) {
                double totalValue = holding.second * stock.price;

                cout << left
                     << setw(14) << holding.first
                     << setw(12) << holding.second
                     << setw(18) << formatMoney(stock.price)
                     << setw(18) << formatMoney(totalValue) << "\n";
                break;
            }
        }
    }
}

void buyStockMenu(Portfolio& portfolio, Market& market) {
    market.displayMarket();

    string symbol;
    int quantity;

    cout << "\nEnter stock symbol: ";
    cin >> symbol;
    toUpperCase(symbol);

    Stock* stock = market.findStock(symbol);
    if (stock == nullptr) {
        RED;
        cout << "Stock not found.\n";
        RESET;
        return;
    }

    cout << "Selected stock : " << stock->name << "\n";
    cout << "Current price  : " << formatMoney(stock->price) << "\n";
    cout << "Enter quantity : ";
    cin >> quantity;

    if (!cin || quantity <= 0) {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        RED;
        cout << "Invalid quantity.\n";
        RESET;
        return;
    }

    double totalCost = stock->price * quantity;
    cout << "Total cost     : " << formatMoney(totalCost) << "\n";

    if (portfolio.buyStock(*stock, quantity)) {
        GREEN;
        cout << "Purchase successful.\n";
        RESET;
    } else {
        RED;
        cout << "Not enough balance.\n";
        RESET;
    }
}

void sellStockMenu(Portfolio& portfolio, Market& market) {
    const auto& holdings = portfolio.getHoldings();

    if (holdings.empty()) {
        YELLOW;
        cout << "You do not own any stocks yet.\n";
        RESET;
        return;
    }

    string symbol;
    int quantity;

    cout << "\nEnter stock symbol: ";
    cin >> symbol;
    toUpperCase(symbol);

    Stock* stock = market.findStock(symbol);
    if (stock == nullptr) {
        RED;
        cout << "Stock not found.\n";
        RESET;
        return;
    }

    auto it = holdings.find(symbol);
    if (it == holdings.end()) {
        RED;
        cout << "You do not own this stock.\n";
        RESET;
        return;
    }

    cout << "You own        : " << it->second << " shares\n";
    cout << "Current price  : " << formatMoney(stock->price) << "\n";
    cout << "Enter quantity : ";
    cin >> quantity;

    if (!cin || quantity <= 0) {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        RED;
        cout << "Invalid quantity.\n";
        RESET;
        return;
    }

    if (portfolio.sellStock(*stock, quantity)) {
        GREEN;
        cout << "Sell successful.\n";
        cout << "Amount received: " << formatMoney(stock->price * quantity) << "\n";
        RESET;
    } else {
        RED;
        cout << "You do not have enough shares.\n";
        RESET;
    }
}

void showTradeHistory(const Portfolio& portfolio) {
    const auto& history = portfolio.getHistory();

    CYAN;
    cout << "\n";
    printDivider(92);
    cout << setw(52) << right << "TRADE HISTORY" << "\n";
    printDivider(92);
    RESET;

    if (history.empty()) {
        YELLOW;
        cout << "No trades yet.\n";
        RESET;
        return;
    }

    cout << left
         << setw(10) << "TYPE"
         << setw(12) << "SYMBOL"
         << setw(10) << "QTY"
         << setw(16) << "PRICE"
         << setw(16) << "TOTAL"
         << "TIME" << "\n";

    cout << string(92, '-') << "\n";

    for (const auto& trade : history) {
        if (trade.type == "BUY") {
            GREEN;
        } else {
            RED;
        }

        cout << left
             << setw(10) << trade.type
             << setw(12) << trade.symbol
             << setw(10) << trade.quantity
             << setw(16) << formatMoney(trade.price)
             << setw(16) << formatMoney(trade.total)
             << trade.time << "\n";
        RESET;
    }
}

void showMarketAnalysis(const Market& market) {
    const auto& stocks = market.getStocks();

    if (stocks.empty()) {
        YELLOW;
        cout << "No stocks loaded.\n";
        RESET;
        return;
    }

    int gainers = 0;
    int losers = 0;
    double totalChange = 0.0;

    vector<Stock> sorted = stocks;

    for (const auto& stock : stocks) {
        totalChange += stock.changePercent;
        if (stock.changePercent > 0) {
            gainers++;
        } else if (stock.changePercent < 0) {
            losers++;
        }
    }

    double avgChange = totalChange / stocks.size();

    CYAN;
    cout << "\n";
    printDivider(78);
    cout << setw(47) << right << "MARKET ANALYSIS" << "\n";
    printDivider(78);
    RESET;
    

    cout << "Total Stocks : " << stocks.size() << "\n";
    cout << "Gainers      : " << gainers << "\n";
    cout << "Losers       : " << losers << "\n";
    cout << "Avg Change   : " << fixed << setprecision(2) << avgChange << "%\n\n";

    sort(sorted.begin(), sorted.end(), [](const Stock& a, const Stock& b) {
        return a.changePercent > b.changePercent;
    });

    GREEN;
    cout << "Top 3 Gainers:\n";
    RESET;
    for (int i = 0; i < min(3, static_cast<int>(sorted.size())); i++) {
        GREEN;
        cout << sorted[i].symbol << " -> "
             << fixed << setprecision(2) << sorted[i].changePercent
             << "%  " << makeGraphBar(sorted[i].changePercent) << "\n";
        RESET;
    }

    sort(sorted.begin(), sorted.end(), [](const Stock& a, const Stock& b) {
        return a.changePercent < b.changePercent;
    });

    RED;
    cout << "\nTop 3 Losers:\n";
    RESET;
    for (int i = 0; i < min(3, static_cast<int>(sorted.size())); i++) {
        RED;
        cout << sorted[i].symbol << " -> "
             << fixed << setprecision(2) << sorted[i].changePercent
             << "%  " << makeGraphBar(sorted[i].changePercent) << "\n";
        RESET;
    }
}

void showStockGraph(Market& market) {
    string symbol;
    cout << "\nEnter stock symbol for graph: ";
    cin >> symbol;
    toUpperCase(symbol);

    Stock* stock = market.findStock(symbol);
    if (stock == nullptr) {
        RED;
        cout << "Stock not found.\n";
        RESET;
        return;
    }

    vector<double> prices;
    double simulatedPrice = stock->price;

    for (int i = 0; i < 24; i++) {
        double percentMove = ((rand() % 160) - 80) / 1000.0;
        simulatedPrice += simulatedPrice * percentMove;

        if (simulatedPrice < 1.0) {
            simulatedPrice = 1.0;
        }

        prices.push_back(simulatedPrice);
    }

    double maxPrice = *max_element(prices.begin(), prices.end());
    double minPrice = *min_element(prices.begin(), prices.end());
    double range = maxPrice - minPrice;

    if (range < 0.01) {
        range = 0.01;
    }

    const int chartHeight = 12;
    const int chartWidth = static_cast<int>(prices.size());

    vector<string> canvas(chartHeight, string(chartWidth, ' '));

    for (int i = 0; i < chartWidth; i++) {
        int y = static_cast<int>(round(((prices[i] - minPrice) / range) * (chartHeight - 1)));
        y = (chartHeight - 1) - y;

        if (y < 0) y = 0;
        if (y >= chartHeight) y = chartHeight - 1;

        canvas[y][i] = '*';

        if (i > 0) {
            int prevY = static_cast<int>(round(((prices[i - 1] - minPrice) / range) * (chartHeight - 1)));
            prevY = (chartHeight - 1) - prevY;

            if (prevY < 0) prevY = 0;
            if (prevY >= chartHeight) prevY = chartHeight - 1;

            if (prevY == y) {
                for (int j = min(i - 1, i); j <= max(i - 1, i); j++) {
                    canvas[y][j] = '-';
                }
                canvas[y][i] = '*';
            } else {
                int startY = min(prevY, y);
                int endY = max(prevY, y);
                for (int row = startY; row <= endY; row++) {
                    if (canvas[row][i] == ' ') {
                        canvas[row][i] = '|';
                    }
                }
                canvas[y][i] = '*';
            }
        }
    }

    CYAN;
    cout << "\n";
    printDivider(90);
    cout << "PRICE TREND GRAPH - " << stock->symbol << " (" << stock->name << ")\n";
    printDivider(90);
    RESET;

    cout << "Current Market Price : " << formatMoney(stock->price) << "\n";
    cout << "Highest Price        : " << formatMoney(maxPrice) << "\n";
    cout << "Lowest Price         : " << formatMoney(minPrice) << "\n\n";

    for (int row = 0; row < chartHeight; row++) {
        double labelValue = maxPrice - ((range / (chartHeight - 1)) * row);

        WHITE;
        cout << setw(12) << fixed << setprecision(2) << labelValue << " | ";

        for (int col = 0; col < chartWidth; col++) {
            if (canvas[row][col] == '*') {
                if (col == 0 || prices[col] >= prices[max(0, col - 1)]) {
                    GREEN;
                } else {
                    RED;
                }
                cout << "*";
            } else if (canvas[row][col] == '-') {
                CYAN;
                cout << "-";
            } else if (canvas[row][col] == '|') {
                CYAN;
                cout << "|";
            } else {
                cout << " ";
            }
            RESET;
        }
        cout << "\n";
    }

    WHITE;
    cout << string(15, ' ') << string(chartWidth + 2, '-') << "\n";
    cout << string(15, ' ') << "1";
    for (int i = 1; i < chartWidth - 1; i++) {
        cout << " ";
    }
    cout << chartWidth << "\n";
    RESET;

    cout << "\nRecent simulated prices:\n";
    for (size_t i = 0; i < prices.size(); i++) {
        double diff = (i == 0) ? 0.0 : prices[i] - prices[i - 1];

        if (diff >= 0) {
            GREEN;
        } else {
            RED;
        }

        cout << setw(2) << (i + 1) << ". "
             << setw(14) << formatMoney(prices[i])
             << "  Move: " << formatSignedNumber(diff) << "\n";
        RESET;
    }
}

// ================= MAIN =================
int main() {
    srand(static_cast<unsigned int>(time(nullptr)));

    Market market;
    Portfolio portfolio(100000.0);

    portfolio.loadPortfolioFromFile();
    portfolio.loadTradeHistoryFromFile();

    showLoading("Starting platform");
    market.updatePrices();

    while (true) {
        market.updatePrices();
        displayMainMenu(portfolio, market);

        int choice;
        cout << "\nEnter your choice: ";

        if (!(cin >> choice)) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            RED;
            cout << "Invalid input. Please enter a number from 1 to 8.\n";
            RESET;
            sleepMilliseconds(1000);
            continue;
        }

        switch (choice) {
        case 1:
            market.displayMarket();
            break;

        case 2:
            displayPortfolio(portfolio, market);
            break;
        case 3:
            buyStockMenu(portfolio, market);
            break;
        case 4:
            sellStockMenu(portfolio, market);
            break;
        case 5:
            showTradeHistory(portfolio);
            break;
        case 6:
            showMarketAnalysis(market);
            break;
        case 7:
            showStockGraph(market);
            break;
        case 8: {
            double finalValue = portfolio.getPortfolioValue(market.getStocks());
            double finalProfit = finalValue - portfolio.getInitialBalance();

            CYAN;
            cout << "\nThanks for using the Stock Market Trading Platform.\n";
            RESET;
            cout << "Final Portfolio Value : " << formatMoney(finalValue) << "\n";

            if (finalProfit >= 0) {
                GREEN;
                cout << "Total Profit          : " << formatSignedMoney(finalProfit) << "\n";
            } else {
                RED;
                cout << "Total Loss            : " << formatSignedMoney(finalProfit) << "\n";
            }
            RESET;

            return 0;
        }
        default:
            RED;
            cout << "Invalid choice. Please select from 1 to 8.\n";
            RESET;
        }
        cout << "\n";
        printDivider(78, '-');
    }
    return 0;
}
//g++ -std=c++17 stockmarket.cpp -o stockmarket.exe