// ================================================================
//  INVESTOPEDIA - TRADING TERMINAL v3.0
//  GCC 6.3 / MinGW.org compatible
//
//  Compile: g++ -std=c++14 -O2 -o stockmarket.exe stockmarket.cpp -lws2_32
//  Run:     .\stockmarket.exe
//  Open:    http://localhost:8080
// ================================================================

// Windows FIRST — always
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <process.h>    // _beginthreadex
#pragma comment(lib,"ws2_32.lib")
typedef int socklen_t;

// Standard includes
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <iomanip>
#include <ctime>
#include <cstdlib>
#include <cmath>
#include <chrono>
#include <limits>

using namespace std;

// ── NO std::mutex — use Windows CRITICAL_SECTION ─────────────────
CRITICAL_SECTION g_mkt_cs;
CRITICAL_SECTION g_con_cs;

struct Lock {
    CRITICAL_SECTION* cs;
    Lock(CRITICAL_SECTION* c): cs(c){ EnterCriticalSection(cs); }
    ~Lock(){ LeaveCriticalSection(cs); }
};

volatile bool g_running = true;

// ── CONFIG ────────────────────────────────────────────────────────
const int    PORT    = 8080;
const string AV_KEY  = "64G4CH9B6B34KB9S";
const string AV_HOST = "www.alphavantage.co";
const double INR     = 83.0;
const string DB_FILE = "database.json";

// ── COLORS ────────────────────────────────────────────────────────
void setColor(int c){ SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),c); }
#define RED     setColor(12)
#define GREEN   setColor(10)
#define YELLOW  setColor(14)
#define CYAN    setColor(11)
#define MAGENTA setColor(13)
#define WHITE   setColor(15)
#define BLUE    setColor(9)
#define RESET   setColor(7)

void sleepMs(int ms){ Sleep(ms); }
void clearScr(){ system("cls"); }

string nowStr(){
    time_t t = time(nullptr);
    char b[64];
    tm* lt = localtime(&t);
    strftime(b, sizeof(b), "%Y-%m-%d %H:%M:%S", lt);
    return b;
}

// ── STOCK ─────────────────────────────────────────────────────────
struct Stock {
    string symbol, name;
    double price, change, changePercent;
    int    volume;
    vector<double> history;

    Stock(string s, string n, double p)
      : symbol(s), name(n), price(p), change(0), changePercent(0), volume(1000000){
        for(int i=0;i<60;i++) history.push_back(p*INR + (rand()%40-20));
    }
    void updatePrice(){
        double old = price;
        price += price * ((rand()%200)-100) / 1000.0;
        if(price < 1) price = 1;
        change = price - old;
        changePercent = (change/old)*100.0;
        volume += (rand()%50000) - 25000;
        if(volume < 100000) volume = 100000;
        history.push_back(price*INR);
        if(history.size() > 60) history.erase(history.begin());
    }
    void applyReal(double usd){
        double old = price;
        price = usd;
        change = price - old;
        changePercent = old>0 ? (change/old)*100.0 : 0;
        history.push_back(price*INR);
        if(history.size() > 60) history.erase(history.begin());
    }
};

struct Trade { string symbol,type,time; int qty; double price,total; };

struct User {
    string username, passwordHash, name;
    double balance;
    map<string,int>    holdings;
    map<string,double> avgPrice;
    vector<Trade>      trades;
};

// ── PORTFOLIO ─────────────────────────────────────────────────────
class Portfolio {
    double bal, initBal;
    map<string,int> h;
    vector<Trade>   hist;
public:
    Portfolio(double s=100000.0):bal(s),initBal(s){}
    double getBalance() const{ return bal; }
    double getInit()    const{ return initBal; }
    map<string,int> getH()    const{ return h; }
    vector<Trade>   getHist() const{ return hist; }
    bool buy(const Stock& s, int q){
        double cost = s.price*INR*q;
        if(cost>bal) return false;
        bal-=cost; h[s.symbol]+=q;
        hist.push_back({s.symbol,"BUY",nowStr(),q,s.price*INR,cost});
        return true;
    }
    bool sell(const Stock& s, int q){
        auto it=h.find(s.symbol);
        if(it==h.end()||it->second<q) return false;
        double rev=s.price*INR*q; bal+=rev;
        it->second-=q; if(!it->second) h.erase(it);
        hist.push_back({s.symbol,"SELL",nowStr(),q,s.price*INR,rev});
        return true;
    }
    double getValue(const vector<Stock>& stocks) const{
        double t=bal;
        for(auto& hh:h)
            for(auto& s:stocks)
                if(s.symbol==hh.first){ t+=hh.second*s.price*INR; break; }
        return t;
    }
};

// ── MARKET ────────────────────────────────────────────────────────
class Market {
    vector<Stock> stocks;
public:
    Market(){
        stocks.push_back(Stock("AAPL",  "Apple Inc.",      175.25));
        stocks.push_back(Stock("GOOGL", "Google Inc.",     138.75));
        stocks.push_back(Stock("MSFT",  "Microsoft Corp.", 330.45));
        stocks.push_back(Stock("TSLA",  "Tesla Inc.",      210.30));
        stocks.push_back(Stock("AMZN",  "Amazon Inc.",     145.80));
        stocks.push_back(Stock("NVDA",  "NVIDIA Corp.",    480.90));
        stocks.push_back(Stock("META",  "Meta Platforms",  310.25));
        stocks.push_back(Stock("NFLX",  "Netflix Inc.",    485.60));
        stocks.push_back(Stock("AMD",   "AMD Inc.",        122.35));
        stocks.push_back(Stock("INTC",  "Intel Corp.",      44.80));
    }
    void updatePrices(){ for(auto& s:stocks) s.updatePrice(); }
    Stock* find(const string& sym){ for(auto& s:stocks) if(s.symbol==sym) return &s; return nullptr; }
    const vector<Stock>& get()    const{ return stocks; }
    vector<Stock>&       getMut()      { return stocks; }
    void display() const {
        CYAN;
        cout<<"+----------+--------------------+---------------+----------+\n";
        cout<<"|  SYMBOL  |      COMPANY       |  PRICE (INR)  |  CHANGE  |\n";
        cout<<"+----------+--------------------+---------------+----------+\n";
        RESET;
        for(auto& s:stocks){
            WHITE; cout<<"| "<<left<<setw(8)<<s.symbol<<"| "<<setw(18)<<s.name.substr(0,18)<<"| ";
            CYAN;  cout<<"Rs."<<right<<setw(9)<<fixed<<setprecision(0)<<(s.price*INR)<<"   | ";
            if(s.change>0)GREEN; else if(s.change<0)RED; else YELLOW;
            cout<<(s.change>=0?"+":"")<<setw(6)<<fixed<<setprecision(0)<<(s.change*INR)<<"  |\n"; RESET;
        }
        CYAN; cout<<"+----------+--------------------+---------------+----------+\n"; RESET;
    }
};

// ── GLOBAL INSTANCES ──────────────────────────────────────────────
Market    g_market;
Portfolio g_portfolio(100000.0);
vector<User> g_users;

// ── SHA-256 ───────────────────────────────────────────────────────
typedef unsigned int u32; typedef unsigned long long u64;
static const u32 SK[64]={
  0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
  0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
  0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
  0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
  0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
  0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
  0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
  0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
#define RR(x,n)(((x)>>(n))|((x)<<(32-(n))))
#define CH(x,y,z)(((x)&(y))^(~(x)&(z)))
#define MJ(x,y,z)(((x)&(y))^((x)&(z))^((y)&(z)))
#define E0(x)(RR(x,2)^RR(x,13)^RR(x,22))
#define E1(x)(RR(x,6)^RR(x,11)^RR(x,25))
#define G0(x)(RR(x,7)^RR(x,18)^((x)>>3))
#define G1(x)(RR(x,17)^RR(x,19)^((x)>>10))
string sha256(const string& in){
    u32 h[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    vector<unsigned char> m(in.begin(),in.end());
    u64 bits=m.size()*8; m.push_back(0x80);
    while(m.size()%64!=56) m.push_back(0);
    for(int i=7;i>=0;i--) m.push_back((bits>>(i*8))&0xff);
    for(size_t i=0;i<m.size();i+=64){
        u32 w[64];
        for(int j=0;j<16;j++) w[j]=(m[i+j*4]<<24)|(m[i+j*4+1]<<16)|(m[i+j*4+2]<<8)|m[i+j*4+3];
        for(int j=16;j<64;j++) w[j]=G1(w[j-2])+w[j-7]+G0(w[j-15])+w[j-16];
        u32 a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
        for(int j=0;j<64;j++){
            u32 t1=hh+E1(e)+CH(e,f,g)+SK[j]+w[j],t2=E0(a)+MJ(a,b,c);
            hh=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
        }
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=hh;
    }
    ostringstream os; for(int i=0;i<8;i++) os<<hex<<setw(8)<<setfill('0')<<h[i];
    return os.str();
}

// ── JSON HELPERS ──────────────────────────────────────────────────
string jEsc(const string& s){
    string o; for(char c:s){ if(c=='"')o+="\\\""; else if(c=='\\')o+="\\\\"; else o+=c; } return o;
}
string jStr(const string& o, const string& k){
    size_t p=o.find("\""+k+"\""); if(p==string::npos) return "";
    p=o.find(':',p); p=o.find('"',p); if(p==string::npos) return "";
    size_t e=p+1;
    while(e<o.size()){ if(o[e]=='\\'){e+=2;continue;} if(o[e]=='"')break; e++; }
    return o.substr(p+1,e-p-1);
}
double jNum(const string& o, const string& k){
    size_t p=o.find("\""+k+"\""); if(p==string::npos) return 0;
    p=o.find(':',p)+1;
    while(p<o.size()&&(o[p]==' '||o[p]=='\t')) p++;
    size_t e=p;
    while(e<o.size()&&(isdigit(o[e])||o[e]=='.'||o[e]=='-')) e++;
    if(p==e) return 0; return stod(o.substr(p,e-p));
}

// ── DATABASE ──────────────────────────────────────────────────────
void dbSave(){
    ofstream f(DB_FILE);
    f<<"{\n  \"users\":[\n";
    for(size_t i=0;i<g_users.size();i++){
        auto& u=g_users[i];
        f<<"    {\"username\":\""<<jEsc(u.username)<<"\","
          <<"\"password\":\""<<u.passwordHash<<"\","
          <<"\"name\":\""<<jEsc(u.name)<<"\","
          <<"\"balance\":"<<fixed<<setprecision(2)<<u.balance<<","
          <<"\"portfolio\":[";
        bool first=true;
        for(auto& h:u.holdings){
            if(!first) f<<","; first=false;
            double avg=u.avgPrice.count(h.first)?u.avgPrice.at(h.first):0;
            f<<"{\"ticker\":\""<<h.first<<"\",\"quantity\":"<<h.second
             <<",\"avgBuyPrice\":"<<fixed<<setprecision(2)<<avg<<"}";
        }
        f<<"]}";
        if(i+1<g_users.size()) f<<",";
        f<<"\n";
    }
    f<<"  ]\n}\n";
}
void dbLoad(){
    ifstream f(DB_FILE); if(!f.is_open()){ dbSave(); return; }
    ostringstream ss; ss<<f.rdbuf(); string c=ss.str();
    size_t pos=c.find('['); if(pos==string::npos) return; pos++;
    while(pos<c.size()){
        size_t s=c.find('{',pos); if(s==string::npos) break;
        int d=0; size_t e=s;
        for(size_t i=s;i<c.size();i++){ if(c[i]=='{')d++; else if(c[i]=='}'){d--;if(!d){e=i;break;}} }
        string uo=c.substr(s,e-s+1);
        User u;
        u.username=jStr(uo,"username"); u.passwordHash=jStr(uo,"password");
        u.name=jStr(uo,"name"); u.balance=jNum(uo,"balance");
        size_t pa=uo.find("\"portfolio\"");
        if(pa!=string::npos){
            pa=uo.find('[',pa);
            if(pa!=string::npos){
                size_t pp=pa+1;
                while(pp<uo.size()){
                    size_t is=uo.find('{',pp); if(is==string::npos) break;
                    int d2=0; size_t ie=is;
                    for(size_t ii=is;ii<uo.size();ii++){ if(uo[ii]=='{')d2++; else if(uo[ii]=='}'){d2--;if(!d2){ie=ii;break;}} }
                    string item=uo.substr(is,ie-is+1);
                    string tk=jStr(item,"ticker");
                    int qty=(int)jNum(item,"quantity");
                    double avg=jNum(item,"avgBuyPrice");
                    if(!tk.empty()){ u.holdings[tk]=qty; u.avgPrice[tk]=avg; }
                    pp=ie+1;
                }
            }
        }
        if(!u.username.empty()) g_users.push_back(u);
        pos=e+1;
    }
}
User* findUser(const string& un){ for(auto& u:g_users) if(u.username==un) return &u; return nullptr; }

// ── ALPHA VANTAGE ─────────────────────────────────────────────────
double fetchAVPrice(const string& symbol){
    string req =
        "GET /query?function=GLOBAL_QUOTE&symbol="+symbol+"&apikey="+AV_KEY+" HTTP/1.0\r\n"
        "Host: "+AV_HOST+"\r\n"
        "Connection: close\r\n\r\n";
    struct hostent* he = gethostbyname(AV_HOST.c_str());
    if(!he) return -1;
    SOCKET sock = socket(AF_INET,SOCK_STREAM,0);
    if(sock==INVALID_SOCKET) return -1;
    sockaddr_in sa={};
    sa.sin_family=AF_INET; sa.sin_port=htons(80);
    sa.sin_addr=*((struct in_addr*)he->h_addr);
    if(connect(sock,(sockaddr*)&sa,sizeof(sa))<0){ closesocket(sock); return -1; }
    send(sock,req.c_str(),(int)req.size(),0);
    string resp; char buf[4096]; int n;
    while((n=recv(sock,buf,sizeof(buf)-1,0))>0){ buf[n]=0; resp+=buf; }
    closesocket(sock);
    size_t p=resp.find("\"05. price\"");
    if(p==string::npos) return -1;
    p=resp.find('"',resp.find(':',p)+1)+1;
    size_t e=resp.find('"',p);
    if(e==string::npos) return -1;
    try{ return stod(resp.substr(p,e-p)); } catch(...){ return -1; }
}

void fetchAllRealPrices(){
    { Lock lk(&g_con_cs); YELLOW; cout<<"\n[AV] Fetching real prices...\n"; RESET; }
    auto& stocks=g_market.getMut();
    int ok=0;
    for(auto& s:stocks){
        double p=fetchAVPrice(s.symbol);
        if(p>0){
            { Lock lk(&g_mkt_cs); s.applyReal(p); }
            Lock lk(&g_con_cs);
            GREEN; cout<<"[AV] "<<s.symbol<<" $"<<fixed<<setprecision(2)<<p
                       <<" = Rs."<<fixed<<setprecision(0)<<p*INR<<"\n"; RESET;
            ok++;
        } else {
            Lock lk(&g_con_cs);
            YELLOW; cout<<"[AV] "<<s.symbol<<" failed\n"; RESET;
        }
        Sleep(500);
    }
    Lock lk(&g_con_cs);
    GREEN; cout<<"[AV] Done: "<<ok<<"/10\n"; RESET;
}

// ── HTTP HELPERS ──────────────────────────────────────────────────
string urlDec(const string& s){
    string o; for(size_t i=0;i<s.size();i++){
        if(s[i]=='+') o+=' ';
        else if(s[i]=='%'&&i+2<s.size()){ int h; istringstream is(s.substr(i+1,2)); is>>hex>>h; o+=(char)h; i+=2; }
        else o+=s[i];
    } return o;
}
map<string,string> parseJson(const string& body){
    map<string,string> p;
    for(auto& k:vector<string>{"username","password","name","ticker"}) p[k]=jStr(body,k);
    double qty=jNum(body,"quantity"), price=jNum(body,"price");
    if(qty!=0){ ostringstream os; os<<(int)qty; p["quantity"]=os.str(); }
    if(price!=0){ ostringstream os; os<<fixed<<setprecision(2)<<price; p["price"]=os.str(); }
    return p;
}
string mkResp(int code, const string& body, const string& ct="application/json"){
    static map<int,string> st={{200,"OK"},{400,"Bad Request"},{401,"Unauthorized"},{403,"Forbidden"},{404,"Not Found"}};
    string status=st.count(code)?st[code]:"Error";
    ostringstream r;
    r<<"HTTP/1.1 "<<code<<" "<<status<<"\r\n"
      <<"Content-Type: "<<ct<<"\r\n"
      <<"Access-Control-Allow-Origin: *\r\n"
      <<"Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
      <<"Access-Control-Allow-Headers: Content-Type\r\n"
      <<"Content-Length: "<<body.size()<<"\r\n"
      <<"Connection: close\r\n\r\n"<<body;
    return r.str();
}
string jOK (const string& m){ return "{\"success\":true,\"message\":\""+m+"\"}"; }
string jERR(const string& m){ return "{\"success\":false,\"message\":\""+m+"\"}"; }

string serveFile(const string& path, const string& mime){
    ifstream f(path,ios::binary); if(!f.is_open()) return "";
    ostringstream ss; ss<<f.rdbuf();
    return mkResp(200,ss.str(),mime);
}

// ── JSON BUILDERS ─────────────────────────────────────────────────
string buildStocksJson(){
    ostringstream o; o<<fixed<<setprecision(2);
    o<<"{\"stocks\":[";
    auto& st=g_market.get();
    for(size_t i=0;i<st.size();i++){
        auto& s=st[i];
        o<<"{\"symbol\":\""<<s.symbol<<"\","
          <<"\"name\":\""<<s.name<<"\","
          <<"\"priceINR\":"<<(s.price*INR)<<","
          <<"\"changeINR\":"<<(s.change*INR)<<","
          <<"\"changePercent\":"<<s.changePercent<<","
          <<"\"volume\":"<<s.volume<<","
          <<"\"history\":[";
        for(size_t j=0;j<s.history.size();j++){ o<<s.history[j]; if(j+1<s.history.size()) o<<","; }
        o<<"]}"; if(i+1<st.size()) o<<",";
    }
    o<<"]}"; return o.str();
}
string buildUserJson(User* u){
    ostringstream o; o<<fixed<<setprecision(2);
    o<<"{\"balance\":"<<u->balance<<",\"name\":\""<<jEsc(u->name)<<"\",\"portfolio\":[";
    bool first=true;
    for(auto& h:u->holdings){
        if(!first) o<<","; first=false;
        double avg=u->avgPrice.count(h.first)?u->avgPrice.at(h.first):0;
        o<<"{\"ticker\":\""<<h.first<<"\",\"quantity\":"<<h.second<<",\"avgBuyPrice\":"<<avg<<"}";
    }
    o<<"]}"; return o.str();
}

// ── ROUTE HANDLER ─────────────────────────────────────────────────
string handleRoute(const string& method, const string& path,
                   const string& query,  const string& body){
    // Serve frontend files
    if(method=="GET"&&(path=="/"||path=="/index.html")){
        string r=serveFile("../frontend/index.html","text/html; charset=utf-8");
        if(r.empty()) r=serveFile("frontend/index.html","text/html; charset=utf-8");
        return r.empty()?mkResp(404,jERR("index.html not found")):r;
    }
    if(method=="GET"&&path=="/script.js"){
        string r=serveFile("../frontend/script.js","application/javascript");
        if(r.empty()) r=serveFile("frontend/script.js","application/javascript");
        return r.empty()?mkResp(404,jERR("script.js not found")):r;
    }
    if(method=="GET"&&path=="/style.css"){
        string r=serveFile("../frontend/style.css","text/css");
        if(r.empty()) r=serveFile("frontend/style.css","text/css");
        return r.empty()?mkResp(404,jERR("style.css not found")):r;
    }
    // GET /stocks
    if(method=="GET"&&path=="/stocks"){
        Lock lk(&g_mkt_cs);
        return mkResp(200,buildStocksJson());
    }
    // POST /register
    if(method=="POST"&&path=="/register"){
        auto p=parseJson(body);
        if(p["username"].empty()||p["password"].empty()||p["name"].empty())
            return mkResp(400,jERR("All fields required"));
        Lock lk(&g_mkt_cs);
        if(findUser(p["username"])) return mkResp(400,jERR("Username taken"));
        User u; u.username=p["username"]; u.passwordHash=sha256(p["password"]);
        u.name=p["name"]; u.balance=100000.0;
        g_users.push_back(u); dbSave();
        { Lock cl(&g_con_cs); GREEN; cout<<"\n[WEB] Register: "<<p["username"]<<"\n"; RESET; }
        return mkResp(200,jOK("Account created!"));
    }
    // POST /login
    if(method=="POST"&&path=="/login"){
        auto p=parseJson(body);
        if(p["username"].empty()||p["password"].empty())
            return mkResp(400,jERR("Username and password required"));
        Lock lk(&g_mkt_cs);
        User* u=findUser(p["username"]);
        if(!u||u->passwordHash!=sha256(p["password"]))
            return mkResp(401,jERR("Invalid username or password"));
        ostringstream o; o<<fixed<<setprecision(2);
        o<<"{\"success\":true,\"username\":\""<<jEsc(u->username)<<"\","
          <<"\"name\":\""<<jEsc(u->name)<<"\",\"balance\":"<<u->balance<<"}";
        { Lock cl(&g_con_cs); CYAN; cout<<"\n[WEB] Login: "<<u->username<<"\n"; RESET; }
        return mkResp(200,o.str());
    }
    // GET /portfolio
    if(method=="GET"&&path=="/portfolio"){
        string un;
        istringstream qs(query); string tok;
        while(getline(qs,tok,'&')){
            size_t eq=tok.find('=');
            if(eq!=string::npos&&tok.substr(0,eq)=="username") un=urlDec(tok.substr(eq+1));
        }
        if(un.empty()) return mkResp(400,jERR("Username required"));
        Lock lk(&g_mkt_cs);
        User* u=findUser(un); if(!u) return mkResp(404,jERR("User not found"));
        return mkResp(200,buildUserJson(u));
    }
    // POST /buy
    if(method=="POST"&&path=="/buy"){
        auto p=parseJson(body);
        if(p["username"].empty()||p["ticker"].empty()||p["quantity"].empty()||p["price"].empty())
            return mkResp(400,jERR("Invalid buy request"));
        int qty=stoi(p["quantity"]); double price=stod(p["price"]);
        if(qty<=0||price<=0) return mkResp(400,jERR("Invalid qty/price"));
        Lock lk(&g_mkt_cs);
        User* u=findUser(p["username"]); if(!u) return mkResp(404,jERR("User not found"));
        double cost=price*qty;
        if(u->balance<cost){
            ostringstream o; o<<fixed<<setprecision(2);
            o<<"Need Rs."<<cost<<" but have Rs."<<u->balance;
            return mkResp(400,jERR(o.str()));
        }
        u->balance-=cost;
        int oq=u->holdings[p["ticker"]];
        double oa=u->avgPrice.count(p["ticker"])?u->avgPrice[p["ticker"]]:price;
        int nq=oq+qty;
        u->avgPrice[p["ticker"]]=(oa*oq+price*qty)/nq;
        u->holdings[p["ticker"]]=nq;
        u->trades.push_back({p["ticker"],"BUY",nowStr(),qty,price,cost});
        dbSave();
        { Lock cl(&g_con_cs); GREEN; cout<<"\n[WEB] BUY "<<qty<<"x "<<p["ticker"]<<" @ Rs."<<fixed<<setprecision(0)<<price<<" | "<<p["username"]<<"\n"; RESET; }
        return mkResp(200,buildUserJson(u));
    }
    // POST /sell
    if(method=="POST"&&path=="/sell"){
        auto p=parseJson(body);
        if(p["username"].empty()||p["ticker"].empty()||p["quantity"].empty()||p["price"].empty())
            return mkResp(400,jERR("Invalid sell request"));
        int qty=stoi(p["quantity"]); double price=stod(p["price"]);
        if(qty<=0||price<=0) return mkResp(400,jERR("Invalid qty/price"));
        Lock lk(&g_mkt_cs);
        User* u=findUser(p["username"]); if(!u) return mkResp(404,jERR("User not found"));
        if(!u->holdings.count(p["ticker"])||u->holdings[p["ticker"]]<qty){
            int own=u->holdings.count(p["ticker"])?u->holdings[p["ticker"]]:0;
            return mkResp(400,jERR("Not enough shares. You own: "+to_string(own)));
        }
        u->holdings[p["ticker"]]-=qty;
        if(!u->holdings[p["ticker"]]) u->holdings.erase(p["ticker"]);
        double rev=price*qty; u->balance+=rev;
        u->trades.push_back({p["ticker"],"SELL",nowStr(),qty,price,rev});
        dbSave();
        { Lock cl(&g_con_cs); RED; cout<<"\n[WEB] SELL "<<qty<<"x "<<p["ticker"]<<" @ Rs."<<fixed<<setprecision(0)<<price<<" | "<<p["username"]<<"\n"; RESET; }
        return mkResp(200,buildUserJson(u));
    }
    return mkResp(404,jERR("Not found"));
}

// ── HTTP SERVER (Windows threads) ────────────────────────────────
struct ClientData { SOCKET sock; };

unsigned __stdcall serveClientThread(void* arg){
    SOCKET sock=((ClientData*)arg)->sock;
    delete (ClientData*)arg;
    char buf[32768]={0};
    int n=recv(sock,buf,sizeof(buf)-1,0);
    if(n>0){
        string raw(buf,n);
        istringstream ss(raw); string line; getline(ss,line);
        istringstream fl(line); string method,full; fl>>method>>full;
        string path=full, query="";
        size_t qp=full.find('?');
        if(qp!=string::npos){ query=full.substr(qp+1); path=full.substr(0,qp); }
        if(method=="OPTIONS"){
            string r="HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\n"
                     "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                     "Access-Control-Allow-Headers: Content-Type\r\n"
                     "Content-Length: 0\r\nConnection: close\r\n\r\n";
            send(sock,r.c_str(),(int)r.size(),0);
        } else {
            int clen=0; string body="";
            size_t he=raw.find("\r\n\r\n");
            if(he!=string::npos){
                string hdrs=raw.substr(0,he);
                size_t cp=hdrs.find("Content-Length:");
                if(cp==string::npos) cp=hdrs.find("content-length:");
                if(cp!=string::npos) clen=stoi(hdrs.substr(hdrs.find(':',cp)+1));
                if(clen>0) body=raw.substr(he+4,clen);
            }
            string resp=handleRoute(method,path,query,body);
            send(sock,resp.c_str(),(int)resp.size(),0);
        }
    }
    closesocket(sock);
    return 0;
}

unsigned __stdcall httpServerThread(void*){
    WSADATA wsa; WSAStartup(MAKEWORD(2,2),&wsa);
    SOCKET srv=socket(AF_INET,SOCK_STREAM,0);
    int opt=1; setsockopt(srv,SOL_SOCKET,SO_REUSEADDR,(char*)&opt,sizeof(opt));
    sockaddr_in addr={}; addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=INADDR_ANY; addr.sin_port=htons(PORT);
    if(bind(srv,(sockaddr*)&addr,sizeof(addr))<0){
        Lock cl(&g_con_cs); RED; cout<<"[ERROR] Port "<<PORT<<" in use!\n"; RESET; return 1;
    }
    listen(srv,20);
    {
        Lock cl(&g_con_cs); GREEN;
        cout<<"\n+================================================+\n";
        cout<<"|  SERVER STARTED!                               |\n";
        cout<<"|  Chrome mein kholo: http://localhost:8080      |\n";
        cout<<"+================================================+\n";
        RESET;
    }
    while(g_running){
        sockaddr_in ca={}; int cl2=sizeof(ca);
        SOCKET cs=accept(srv,(sockaddr*)&ca,(socklen_t*)&cl2);
        if(cs==INVALID_SOCKET) continue;
        ClientData* cd=new ClientData(); cd->sock=cs;
        HANDLE h=(HANDLE)_beginthreadex(NULL,0,serveClientThread,cd,0,NULL);
        if(h) CloseHandle(h);
    }
    return 0;
}

unsigned __stdcall avFetchThread(void*){
    fetchAllRealPrices();
    return 0;
}

unsigned __stdcall marketThread(void*){
    int tick=0, avTick=0;
    while(g_running){
        Sleep(1000); tick++; avTick++;
        { Lock lk(&g_mkt_cs); g_market.updatePrices(); }
        if(avTick>=60){ avTick=0; HANDLE h=(HANDLE)_beginthreadex(NULL,0,avFetchThread,NULL,0,NULL); if(h) CloseHandle(h); }
        {
            Lock cl(&g_con_cs);
            auto& stocks=g_market.get();
            int g=0,l=0; for(auto& s:stocks){ if(s.change>0)g++; else if(s.change<0)l++; }
            cout<<"\r";
            CYAN; cout<<"["<<setw(4)<<tick<<"s] "; RESET;
            for(auto& s:stocks){
                if(s.change>=0) GREEN; else RED;
                cout<<s.symbol<<":"<<fixed<<setprecision(0)<<(s.price*INR)<<(s.change>=0?"+":"-")<<" ";
            }
            WHITE; cout<<"G:"; GREEN; cout<<g; WHITE; cout<<" L:"; RED; cout<<l; RESET;
            cout.flush();
        }
    }
    return 0;
}

unsigned __stdcall avStartupThread(void*){
    Sleep(2000);
    fetchAllRealPrices();
    return 0;
}

// ── BOOT ──────────────────────────────────────────────────────────
void showBoot(){
    clearScr();
    CYAN;
    cout<<"################################################################################\n";
    cout<<"#                   INVESTOPEDIA - TRADING TERMINAL v3.0                      #\n";
    cout<<"################################################################################\n\n";
    RESET;
    vector<string> msgs={"Initializing engine...","Starting HTTP server...","Connecting to Alpha Vantage...","Loading database...","Ready!"};
    for(auto& m:msgs){
        YELLOW; cout<<"  "<<m; RESET;
        cout<<" ["; for(int i=0;i<20;i++){ if(i%3==0){GREEN;cout<<"#";}else if(i%3==1){CYAN;cout<<"|";}else{BLUE;cout<<"-";} cout.flush(); Sleep(30); }
        GREEN; cout<<"] OK\n"; RESET; Sleep(80);
    }
    GREEN;
    cout<<"\n================================================\n";
    cout<<"  OPEN CHROME: http://localhost:8080\n";
    cout<<"================================================\n\n";
    RESET; Sleep(400);
}

// ── CONSOLE MENU ─────────────────────────────────────────────────
void consoleMenu(){
    while(true){
        cout<<"\n\n"; BLUE;
        cout<<"================================================================================\n";
        cout<<"                         INVESTOPEDIA - CONSOLE\n";
        cout<<"================================================================================\n"; RESET;
        double val=g_portfolio.getValue(g_market.get());
        double pl=val-g_portfolio.getInit();
        YELLOW; cout<<"Cash: "; GREEN; cout<<"Rs."<<fixed<<setprecision(2)<<g_portfolio.getBalance();
        YELLOW; cout<<"  Portfolio: "; if(pl>=0)GREEN; else RED;
        cout<<"Rs."<<fixed<<setprecision(2)<<val<<" (P/L: "<<(pl>=0?"+":"")<<pl<<")";
        YELLOW; cout<<"  Web: "; CYAN; cout<<"localhost:8080\n"; RESET;
        MAGENTA;
        cout<<"\n  [1] Market  [2] Portfolio  [3] Buy  [4] Sell  [5] History  [6] Analysis  [7] Exit\n\n";
        RESET; CYAN; cout<<"Select: "; RESET;
        int ch; if(!(cin>>ch)){ cin.clear(); cin.ignore(1000,'\n'); continue; }
        switch(ch){
        case 1:
            clearScr(); { Lock lk(&g_mkt_cs); g_market.display(); }
            cout<<"\nEnter to continue..."; cin.ignore(); cin.get(); break;
        case 2:{
            clearScr(); CYAN; cout<<"\n=== MY PORTFOLIO ===\n"; RESET;
            auto hh=g_portfolio.getH();
            if(hh.empty()){ YELLOW; cout<<"No stocks yet.\n"; RESET; }
            else for(auto& h:hh){
                auto* s=g_market.find(h.first);
                WHITE; cout<<h.first<<" | "<<h.second<<" shares";
                if(s) cout<<" | Rs."<<fixed<<setprecision(0)<<s->price*INR<<" each";
                cout<<"\n"; RESET;
            }
            cout<<"\nEnter..."; cin.ignore(); cin.get(); break;
        }
        case 3:{
            clearScr(); { Lock lk(&g_mkt_cs); g_market.display(); }
            cout<<"Symbol: "; string sym; cin>>sym;
            transform(sym.begin(),sym.end(),sym.begin(),::toupper);
            Stock* s=nullptr; { Lock lk(&g_mkt_cs); s=g_market.find(sym); }
            if(!s){ RED; cout<<"Not found!\n"; RESET; Sleep(1000); break; }
            cout<<"Quantity: "; int qty; cin>>qty;
            { Lock lk(&g_mkt_cs);
              if(g_portfolio.buy(*s,qty)){ GREEN; cout<<"Bought "<<qty<<"x "<<sym<<" @ Rs."<<fixed<<setprecision(0)<<s->price*INR<<"\n"; RESET; }
              else { RED; cout<<"Not enough balance!\n"; RESET; } }
            Sleep(1200); break;
        }
        case 4:{
            cout<<"Symbol: "; string sym; cin>>sym;
            transform(sym.begin(),sym.end(),sym.begin(),::toupper);
            cout<<"Quantity: "; int qty; cin>>qty;
            Stock* s=nullptr; { Lock lk(&g_mkt_cs); s=g_market.find(sym); }
            if(!s){ RED; cout<<"Not found!\n"; RESET; Sleep(1000); break; }
            { Lock lk(&g_mkt_cs);
              if(g_portfolio.sell(*s,qty)){ GREEN; cout<<"Sold "<<qty<<"x "<<sym<<"!\n"; RESET; }
              else { RED; cout<<"Not enough shares!\n"; RESET; } }
            Sleep(1200); break;
        }
        case 5:{
            clearScr(); CYAN; cout<<"\n=== TRADE HISTORY ===\n"; RESET;
            for(auto& t:g_portfolio.getHist()){
                if(t.type=="BUY") GREEN; else RED;
                cout<<t.type<<" "<<t.qty<<"x "<<t.symbol<<" @ Rs."<<fixed<<setprecision(0)<<t.price<<" | "<<t.time<<"\n"; RESET;
            }
            cout<<"\nEnter..."; cin.ignore(); cin.get(); break;
        }
        case 6:{
            clearScr(); Lock lk(&g_mkt_cs);
            auto& st=g_market.get(); int g=0,l=0; double tc=0;
            for(auto& s:st){ tc+=s.changePercent; if(s.change>0)g++; else if(s.change<0)l++; }
            CYAN; cout<<"\n=== MARKET ANALYSIS ===\n"; RESET;
            cout<<"Gainers: "; GREEN; cout<<g; RESET;
            cout<<"  Losers: "; RED; cout<<l; RESET;
            cout<<"  Avg: "<<fixed<<setprecision(2)<<tc/st.size()<<"%\n";
            cout<<"\nEnter..."; cin.ignore(); cin.get(); break;
        }
        case 7:
            g_running=false; CYAN; cout<<"\nGoodbye!\n"; RESET; Sleep(400); return;
        }
    }
}

// ── MAIN ──────────────────────────────────────────────────────────
int main(){
    srand((unsigned)time(nullptr));
    InitializeCriticalSection(&g_mkt_cs);
    InitializeCriticalSection(&g_con_cs);
    dbLoad();
    showBoot();
    HANDLE h1=(HANDLE)_beginthreadex(NULL,0,httpServerThread,NULL,0,NULL);
    HANDLE h2=(HANDLE)_beginthreadex(NULL,0,marketThread,NULL,0,NULL);
    HANDLE h3=(HANDLE)_beginthreadex(NULL,0,avStartupThread,NULL,0,NULL);
    if(h1) CloseHandle(h1);
    if(h2) CloseHandle(h2);
    if(h3) CloseHandle(h3);
    Sleep(500);
    consoleMenu();
    DeleteCriticalSection(&g_mkt_cs);
    DeleteCriticalSection(&g_con_cs);
    return 0;
}