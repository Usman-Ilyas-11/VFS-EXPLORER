// --------------------------------------------mini_explorer_ui.cpp--------------------------------------------
#include <windows.h>
#include <iomanip>
#include <iostream>
#include <vector>
#include <string>
#include <stack>
#include <algorithm>
#include <sstream>
#include <fstream>   
#include <cstdlib>   
#include <conio.h>
#include <functional>

#define CLR_RESET  "\033[0m"
#define CLR_ROOT   "\033[93m"   // Yellow
#define CLR_LEFT   "\033[94m"   // Blue
#define CLR_RIGHT  "\033[95m"   // Pink
#define CLR_LINE   "\033[90m"   // Gray 

using std::cout;
using std::cin;
using std::endl;
using std::string;
using std::vector;
using std::stack;
using std::stringstream;

// ---------------------------------------------------------------structure ---------------------------------------------------------------
//expression Node
struct ExpNode {
    string val;
    ExpNode* left;
    ExpNode* right;

    ExpNode(string v) : val(v), left(nullptr), right(nullptr) {}
};

//simple Node
struct Node {
    string name;
    string fileContent;
    bool isFolder;
    bool isLocked;      
    bool isBookmarked;  
    vector<Node*> children;
    Node* parent;

    Node(const string& n = "", bool folder = true, Node* p = NULL) {
        name = n;
        isFolder = folder;
        parent = p;
        isLocked = false;
        isBookmarked = false;
    }
};
// action
struct Action {
    string type;
    Node* node;
    Node* parent;
    int index;
    string extra; 
    Action(const string &t = "", Node* n = nullptr, Node* p = nullptr, int i = -1, const string &e = "")
        : type(t), node(n), parent(p), index(i), extra(e) {}
};


// Trie for suggestions
struct TrieNode {
    char ch;
    vector<TrieNode*> children;
    bool isEnd;
    string word;

    TrieNode(char c = 0) : ch(c), isEnd(false), word("") {}
};

TrieNode* trieRoot = nullptr;
TrieNode* trieNewNode(char c) {
    return new TrieNode(c);
}

void trieInsert(const string& word) {
    if (!trieRoot) trieRoot = trieNewNode(0);

    TrieNode* cur = trieRoot;
    for (char c : word) {
        TrieNode* nxt = nullptr;
        for (auto child : cur->children) {
            if (child->ch == c) {
                nxt = child;
                break;
            }
        }
        if (!nxt) {
            nxt = trieNewNode(c);
            cur->children.push_back(nxt);
        }
        cur = nxt;
    }
    cur->isEnd = true;
    cur->word = word;
}

vector<string> trieSuggest(const string& prefix, int limit) {
    vector<string> results;
    if (!trieRoot || prefix.empty()) return results;

    TrieNode* cur = trieRoot;
    for (char c : prefix) {
        TrieNode* nxt = nullptr;
        for (auto child : cur->children) {
            if (child->ch == c) {
                nxt = child;
                break;
            }
        }
        if (!nxt) return results;
        cur = nxt;
    }

    std::function<void(TrieNode*, string)> dfs = [&](TrieNode* node, string path) {
        if ((int)results.size() >= limit) return;

        if (node->isEnd)
            results.push_back(node->word);

        for (auto child : node->children)
            dfs(child, path + child->ch);
    };

    dfs(cur, prefix);
    return results;
}

// Bookmarks
struct Bookmark {
    string name;
    Node* node;
    Bookmark* next;
    Bookmark(const string& n, Node* nd) : name(n), node(nd), next(NULL) {}
};

// ------------------------------------------------------------------ Globals -----------------------------------------------------------

Node* ROOT = new Node("ROOT", true, NULL);
Node* currentNode = ROOT;

vector<string> commandHistory;

Bookmark* bookmarksHead = NULL;
stack<Node*> backStack;
stack<Node*> forwardStack;
stack<Action> undoStack;
stack<Action> redoStack;

vector<string> treeLines;

string inputBuffer = "";


//----------------------------------------------------------For Centring text---------------------------------------------------

//Center Alignment
void center(const string& text) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);

    int width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    int realLength = 0;

    bool ansi = false;
    for (char c : text) {
        if (c == '\033') ansi = true;
        else if (ansi && c == 'm') ansi = false;
        else if (!ansi) realLength++;
    }

    int pad = (width - realLength) / 2;
    if (pad < 0) pad = 0;

    cout << string(pad, ' ') << text << endl;
}

//left alignment
int getLeftPadding() {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);

    int screenWidth = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    int boxWidth = 50; 
    return (screenWidth - boxWidth) / 2;
}

//NewLine Prompt
void showPromptNoNewline() {
    string prompt = "\033[96m📂 /" + currentNode->name + " > \033[0m";
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    int width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    int realLength = 0;
    bool ansi = false;
    for (char c : prompt) {
        if (c == '\033') ansi = true;
        else if (ansi && c == 'm') ansi = false;
        else if (!ansi) realLength++;
    }
    int pad = (width - realLength) / 2;
    if (pad < 0) pad = 0;
    cout << string(pad, ' ') << prompt;
}

//console width

int getConsoleWidth() {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    return csbi.srWindow.Right - csbi.srWindow.Left + 1;
}
void clearConsoleSim() {
system("cls");
}
//------------------------------------------------------ For prompt Entring----------------------------------------------------------------------

void parseCommandLine(const string& line, string& cmd, string& argRest) {
stringstream ss(line);
cmd = ""; argRest = "";
if (!(ss >> cmd)) return;
string rest; std::getline(ss, rest);
if (!rest.empty()) {
size_t pos = rest.find_first_not_of(" ");
if (pos != string::npos) rest = rest.substr(pos);
else rest = "";
}
argRest = rest;
}
string toLowerCopy(string s) {
    for (char &c : s) c = tolower(c);
    return s;
}

//display promptline
void showPrompt() {
    int padding = getLeftPadding();
    cout << string(padding, ' ')
         << "\033[96m📂 /" << currentNode->name << " > \033[0m";
}

//------------------------------------------------------ Header Box of The Project----------------------------------------------------------------------

void displayUIHeader() {
	
    system("cls");
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);

    int screenWidth = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    int boxWidth = 50;
    int padding = (screenWidth - boxWidth) / 2;

    auto center = [&](string text){
        cout << string(padding, ' ') << text;
    };

    cout << "\033[93m";  

    center("╔══════════════════════════════════════════════════╗\n");
    center("║ \033[97m          ⭐ MINI FILE EXPLORER ⭐               \033[93m║\n");
    center("╚══════════════════════════════════════════════════╝\n");

    // PATH BOX
    center("╔══════════════════════════════════════════════════╗\n");
    center("║ \033[97m📂 Path: /ROOT                                   \033[93m║\n");
    center("╚══════════════════════════════════════════════════╝\n");

    cout << "\033[0m"; 
}
void displayUIfoter() {
	system("cls");
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);

    int screenWidth = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    int boxWidth = 50; 
    int padding = (screenWidth - boxWidth) / 2;

    auto center = [&](string text){
        cout << string(padding, ' ') << text;
    };

    cout << "\033[93m"; 

    center("╔══════════════════════════════════════════════════╗\n");
    center("║  \033[97m     👋 Goodbye! Your workspace was saved.      \033[93m║\n");
    center("╚══════════════════════════════════════════════════╝\n");

    
    
    cout << "\033[0m"; 
}

//----------------------------------------------------------------uitilites-----------------------------------------------------------------


//------------------------------------->>>> For finding child nodes of folders
Node* findChild(Node* dir, const string& name) {
if (dir == NULL) return NULL;
for (size_t i = 0; i < dir->children.size(); ++i) {
if (dir->children[i]->name == name) return dir->children[i];
}
return NULL;
}

string joinPath(Node* node) {
vector<string> parts;
Node* cur = node;
while (cur != NULL) {
parts.insert(parts.begin(), cur->name);
cur = cur->parent;
}
string out = "/";
for (size_t i = 0; i < parts.size(); ++i) {
out += parts[i];
if (i + 1 < parts.size()) out += "/";
}
return out;
}
// find node by absolute path 
Node* findNodeByPath(const string& path) {
    if (path.empty()) return NULL;
    // path components split by '/'
    vector<string> parts;
    string cur;
    for (size_t i = 0; i < path.size(); ++i) {
        char c = path[i];
        if (c == '/') {
            if (!cur.empty()) { parts.push_back(cur); cur.clear(); }
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) parts.push_back(cur);

    if (parts.empty()) return NULL;
    // first must match ROOT name
    if (parts[0] != ROOT->name) return NULL;

    Node* node = ROOT;
    for (size_t i = 1; i < parts.size(); ++i) {
        node = findChild(node, parts[i]);
        if (!node) return NULL;
    }
    return node;
}


// ------------------------------------------>>>> File helpers
//create Physical file
bool createPhysicalFile(const string &filename) {
    std::ofstream out(filename.c_str(), std::ios::app); 
    if (!out) return false;
    out.close();
    return true;
}
// Delete physical file
bool deletePhysicalFile(const string &filename) {
    if (std::remove(filename.c_str()) == 0) return true;
    return false;
}
// read physical file 
string readPhysicalFile(const string &filename) {
    std::ifstream in(filename.c_str());
    if (!in) return string();
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}
// write/append physical file 
bool appendToPhysicalFile(const string &filename, const string &content) {
    std::ofstream out(filename.c_str(), std::ios::app);
    if (!out) return false;
    out << content;
    out.close();
    return true;
}

//-------------------------------->>>> For generating expression tree (infix,postfix,prefix)
int prec(const string& op) {
    if (op=="+"||op=="-") return 1;
    if (op=="*"||op=="/") return 2;
    if (op=="^") return 3;
    return 0;
}
bool isOperator(const string& s) {
    return s=="+"||s=="-"||s=="*"||s=="/"||s=="^";
}

vector<string> tokenize(const string& expr) {
    stringstream ss(expr);
    vector<string> t;
    string x;
    while (ss >> x) t.push_back(x);
    return t;
}
//for centring tree
string stripANSI(const string& s) {
    string res;
    bool esc = false;

    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\033')
            esc = true;
        else if (esc && s[i] == 'm')
            esc = false;
        else if (!esc)
            res += s[i];
    }
    return res;
}
//postfix tree
ExpNode* buildPostfixTree(const string& expr) {
    stack<ExpNode*> st;
    for (auto& t : tokenize(expr)) {
        if (!isOperator(t))
            st.push(new ExpNode(t));
        else {
            auto r = st.top(); st.pop();
            auto l = st.top(); st.pop();
            auto n = new ExpNode(t);
            n->left = l;
            n->right = r;
            st.push(n);
        }
    }
    return st.empty() ? nullptr : st.top();
}
//prefixtree
ExpNode* buildPrefixTree(const string& expr) {
    auto tokens = tokenize(expr);
    stack<ExpNode*> st;

    for (int i = tokens.size() - 1; i >= 0; --i) {
        if (!isOperator(tokens[i]))
            st.push(new ExpNode(tokens[i]));
        else {
            auto l = st.top(); st.pop();
            auto r = st.top(); st.pop();
            auto n = new ExpNode(tokens[i]);
            n->left = l;
            n->right = r;
            st.push(n);
        }
    }
    return st.empty() ? nullptr : st.top();
}
//converting infix to postfix for infixtree
string infixToPostfix(const string& expr) {
    stack<string> st;
    string out;

    for (auto& t : tokenize(expr)) {
        if (!isOperator(t) && t!="(" && t!=")") out += t+" ";
        else if (t=="(") st.push(t);
        else if (t==")") {
            while (!st.empty() && st.top()!="(") {
                out += st.top()+" "; st.pop();
            }
            st.pop();
        }
        else {
            while (!st.empty() && prec(st.top())>=prec(t)) {
                out += st.top()+" "; st.pop();
            }
            st.push(t);
        }
    }
    while (!st.empty()) {
        out += st.top()+" "; st.pop();
    }
    return out;
}
//infixtree
ExpNode* buildInfixTree(const string& expr) {
    return buildPostfixTree(infixToPostfix(expr));
}
//create tree
void genTree(
    ExpNode* node,
    string prefix,
    bool isLast,
    bool isRoot,
    bool isLeft
) {
    if (!node) return;

    string color =
        isRoot ? CLR_ROOT :
        isLeft ? CLR_LEFT :
                 CLR_RIGHT;

    string line = prefix;

    if (!isRoot) {
        line += string(CLR_LINE)
              + (isLast ? "└── " : "├── ")
              + CLR_RESET;
    }

    line += color + node->val + CLR_RESET;
    treeLines.push_back(line);

   string nextPrefix = prefix +
    (isLast
        ? "    "
        : string(CLR_LINE) + "│" + CLR_RESET + "   ");


    if (node->left)
        genTree(node->left, nextPrefix, false, false, true);

    if (node->right)
        genTree(node->right, nextPrefix, true, false, false);
}

//print created tree
void printExpressionTree(ExpNode* root, const string& title) {
    if (!root) return;

    treeLines.clear();
    genTree(root, "", true, true, false);

    int maxLen = 0;
    for (auto& l : treeLines)
        maxLen = std::max(maxLen, (int)stripANSI(l).length());

    int width = getConsoleWidth();
    int pad = std::max(0, (width - maxLen) / 2);

    cout << "\n";
    cout << string((width - title.length()) / 2, ' ')
         << CLR_ROOT << title << CLR_RESET << "\n\n";

    for (auto& l : treeLines)
        cout << string(pad, ' ') << l << "\n";

    cout << "\n";
}
//------------------------->>>>>for command suggesstion
void initCommandTrie() {
    vector<string> cmds = {

        "ls",
        "mkdir",
        "rm",
        "touch",
        "view",
        "edit",
        "rename",
        "cd",
        "tree",
        "infixtree",
        "prefixtree",
        "postfixtree",
        "undo",
        "redo",
        "search",
        "bookmark",
        "history",
        "clearhistory",
        "lock",
        "unlock",
        "clear",
        "exit"
        
    };

    for (auto &cmd : cmds)
        trieInsert(cmd);
}


// --------------------------->>>>>>>> For LOCK system
//lock item
void lockItem(Node* currentFolder, const string& name) {

    Node* item = findChild(currentFolder, name);

    if (!item) {
        cout << "\033[91m"; 
        center("❌ Item not found.\n");
        cout << "\033[0m";
        return;
    }

    if (item->isLocked) {
        center("🔐 Item is already locked.\n");
        return;
    }

    item->isLocked = true;

    cout << "\033[92m"; 
    center("🔒 Item locked successfully!\n");
    cout << "\033[0m";
}


//  UNLOCK ITEM 
void unlockItem(Node* currentFolder, const string& name) {

    Node* item = findChild(currentFolder, name);

    if (!item) {
        cout << "\033[91m";
        center("❌ Item not found.\n");
        cout << "\033[0m";
        return;
    }

    if (!item->isLocked) {
        center("🔓 Item is already unlocked.\n");
        return;
    }

    item->isLocked = false;

    cout << "\033[92m"; 
    center("🔓 Item unlocked successfully!\n");
    cout << "\033[0m";
}

// ----------------->>>>>>>>>> complete Bookmarks functions

//list
void bookmark_list() {
    int padding = getLeftPadding();
    Bookmark* b = bookmarksHead;

    cout << string(padding, ' ') << "🔖 Saved Bookmarks:\n";

    if (!b) {
        cout << string(padding, ' ') << "\033[91m(none)\033[0m\n";
        return;
    }

    while (b) {
        cout << string(padding, ' ') << "📌 "
             << b->name << "  ->  " << joinPath(b->node) << endl;
        b = b->next;
    }
}
//jump to bookmark
void bookmark_go(const string& name) {
    Bookmark* b = bookmarksHead;
    while (b) {
        if (b->name == name) {
            if (!b->node->isFolder) {
                center("📄 Opening bookmarked file...");
                center("📝 Content:");
                center(readPhysicalFile(b->node->name)); 
            } else {
                currentNode = b->node;
                center("📂 Jumped to Folder: " + name);
            }
            return;
        }
        b = b->next;
    }
    cout << "\033[31m"; center("❌ Bookmark Not Found!"); cout << "\033[0m";
}

//add new bookmark
void bookmark_add(const string& name) {
    if (name.empty()) { center("ℹ Usage: bookmark add <name>"); return; }
    if (name == ".") {
        Bookmark* b = new Bookmark(currentNode->name, currentNode);
        b->next = bookmarksHead; bookmarksHead = b;
        currentNode->isBookmarked = true;
        // push undo action
        undoStack.push(Action("bookmark_add", nullptr, nullptr, -1, currentNode->name));
        while (!redoStack.empty()) redoStack.pop();
        center("🔖 Bookmark Added: " + currentNode->name);
        return;
    }
    Node* target = findChild(currentNode, name);
    if (!target) { cout << "\033[31m"; center("❌ Item Not Found!"); cout << "\033[0m"; return; }
    Bookmark* b = new Bookmark(name, target);
    b->next = bookmarksHead; bookmarksHead = b;
    target->isBookmarked = true;
    // push undo action (bookmark_add)
    undoStack.push(Action("bookmark_add", nullptr, nullptr, -1, name));
    while (!redoStack.empty()) redoStack.pop();
    center("🔖 Bookmark Added: " + name);
}

//delete bookmark
void bookmark_remove(const string& name) {
    Bookmark* b = bookmarksHead; Bookmark* prev = NULL;
    while (b) {
        if (b->name == name) {
            if (b->node) b->node->isBookmarked = false;
            if (prev) prev->next = b->next; else bookmarksHead = b->next;
            undoStack.push(Action("bookmark_remove", nullptr, nullptr, -1, name));
            while (!redoStack.empty()) redoStack.pop();
            delete b;
            center("🗑 Bookmark Removed: " + name);
            return;
        }
        prev = b; b = b->next;
    }
    cout << "\033[31m"; center("❌ Bookmark Not Found!"); cout << "\033[0m";
}


// ------------------------------>>>>>>>>>>>>>> complete search functionality
void searchDFS(Node* node, const string& keyword, vector<string>& results, const string& path="") {
if (!node) return;
string currentPath = path + "/" + node->name;
if (node->name.find(keyword) != string::npos) results.push_back(currentPath);
for (size_t i = 0; i < node->children.size(); ++i) {
searchDFS(node->children[i], keyword, results, currentPath);
}
}

void searchBFS(Node* start, const string& keyword, vector<string>& results) {
if (!start) return;
vector<Node*> queue;
vector<string> paths;
queue.push_back(start);
paths.push_back("/" + start->name);


for (size_t i = 0; i < queue.size(); ++i) {
    Node* node = queue[i];
    string path = paths[i];
    if (node->name.find(keyword) != string::npos) results.push_back(path);
    for (size_t j = 0; j < node->children.size(); ++j) {
        queue.push_back(node->children[j]);
        paths.push_back(path + "/" + node->children[j]->name);
    }
  }
}

// ------------------------->>>>>>>>>>>>>>>>> Tree Printing of complete workspace


void generateTree(Node* node, string prefix, bool isLast) {
    if (!node) return;

    string icon;
    if (node->isLocked) icon = "🔒" + string(node->isFolder ? "📁 " : "📄 ");
    else icon = node->isFolder ? "📁 " : "📄 ";

    string current = prefix + (prefix == "" ? "" : (isLast ? "└── " : "├── ")) + icon + node->name;
    treeLines.push_back(current);

    string newPrefix = prefix + (isLast ? "    " : "│   ");

    for (size_t i = 0; i < node->children.size(); i++)
        generateTree(node->children[i], newPrefix, i == node->children.size() - 1);
}



// ------------------------------------------------------>>>>>>>>>>>>> cmd_Commands-----------------------------------------------------------

//-------------------------->>>>Expression tree commands
//postfixtree
void cmd_postfix_tree(const string& file) {
    std::ifstream in(file);
    if (!in) { center("❌ File not found\n"); return; }
    string expr; getline(in, expr);
    printExpressionTree(buildPostfixTree(expr),
        "🌳 POSTFIX EXPRESSION TREE 🌳");
}

//prefixtree
void cmd_prefix_tree(const string& file) {
    std::ifstream in(file);
    if (!in) { center("❌ File not found\n"); return; }
    string expr; getline(in, expr);
    printExpressionTree(buildPrefixTree(expr),
        "🌳 PREFIX EXPRESSION TREE 🌳");
}

//infixtree
void cmd_infix_tree(const string& file) {
    std::ifstream in(file);
    if (!in) { center("❌ File not found\n"); return; }
    string expr; getline(in, expr);
    printExpressionTree(buildInfixTree(expr),
        "🌳 INFIX EXPRESSION TREE 🌳");
}


//----------------------------->>>> create file commands
//create
void cmd_touch(const string& name) {
    if (name.empty()) {
	 cout << "\033[91m"; 
	 center("❌ touch: missing file name"); 
	 cout << "\033[0m"; return; 
	 }
    if (findChild(currentNode, name)) {
	 cout << "\033[91m"; 
	 center("⚠️ File already exists"); 
	 cout << "\033[0m"; 
	 return; 
	 }
    Node* f = new Node(name, false, currentNode);
    
    if (!createPhysicalFile(name)) {
        cout << "\033[91m"; 
		center("❌ Failed to create file on disk"); 
		cout << "\033[0m";
        delete f; return;
    }
    currentNode->children.push_back(f);
    
    int idx = (int)currentNode->children.size() - 1;
    undoStack.push(Action("create", f, currentNode, idx, ""));
    while (!redoStack.empty()) redoStack.pop();
    cout << "\033[92m"; center("📄 File created: " + name); cout << "\033[0m";
}

//view
void cmd_view(const string& name) {
    if (name.empty()) { 
	cout << "\033[91m"; 
	center("❌ view: missing filename"); 
	cout << "\033[0m"; 
	return; 
	}
    Node* f = findChild(currentNode, name);
    if (!f || f->isFolder) { 
	cout << "\033[91m"; 
	center("❌ File not found: " + name); 
	cout << "\033[0m"; 
	return; 
	}
    if (f->isLocked) { 
	cout << "\033[91m"; 
	center("🔒 Cannot view: File is LOCKED"); 
	cout << "\033[0m"; 
	return; 
	}
    
    string data = readPhysicalFile(name);
    cout << "\033[96m"; 
	center("📄 File content: " + name); 
	cout << "\033[0m";
    if (data.empty()) center("(empty)");
    else {
        std::istringstream ss(data);
        string line;
        while (std::getline(ss, line)) center(line);
    }
}

//edit
void cmd_edit(const string& name) {
    if (name.empty()) {
	 cout << "\033[91m"; 
	 center("❌ edit: missing filename"); 
	 cout << "\033[0m"; 
	 return; 
	 }
    Node* f = findChild(currentNode, name);
    if (!f || f->isFolder) {
	 cout << "\033[91m"; 
	 center("❌ File not found"); 
	 cout << "\033[0m"; 
	 return;
	  }
    if (f->isLocked) { 
	cout << "\033[91m"; 
	center("🔒 Cannot edit: File is LOCKED"); 
	cout << "\033[0m"; 
	return; 
	}
    cout << "\033[93m"; 
	center("✏ Enter text (':wq' save, ':q' cancel):"); 
	cout << "\033[0m";
    string line, newContent;
    
    while (true) {
        if (!std::getline(cin, line)) break;
        if (line == ":wq") break;
        if (line == ":q") { 
		cout << "\033[91m"; center("🚫 Edit cancelled"); 
		cout << "\033[0m"; 
		return; 
		}
        newContent += line + "\n";
    }
    if (!newContent.empty()) {
        if (!appendToPhysicalFile(name, newContent)) { 
		cout << "\033[91m"; 
		center("❌ Failed to write to disk"); 
		cout << "\033[0m"; 
		return; 
		}
        f->fileContent += newContent;
    }
    cout << "\033[92m"; center("💾 Saved"); cout << "\033[0m";
}
//----------------------------->>>>>>>>>   folder commanDs
//list
void cmd_ls() {
    cout << "\033[96m";
    center("📂 Contents");
    cout << "\033[0m";

    if (currentNode->children.empty()) {
        center("(empty)");
        return;
    }

    for (size_t i = 0; i < currentNode->children.size(); ++i) {
        Node* c = currentNode->children[i];
        string icon = c->isFolder ? "📁 " : "📄 ";
        center(icon + c->name);
    }
}

//create folder
void cmd_mkdir(const string& name) {
    if (name.empty()) { 
	center("❌ mkdir: missing name"); 
	return;
	 }
    if (findChild(currentNode, name)) { 
	center("⚠ Folder already exists.");
	return; 
	}
    Node* n = new Node(name, true, currentNode);
    currentNode->children.push_back(n);
    int idx = (int)currentNode->children.size() - 1;
    undoStack.push(Action("create", n, currentNode, idx, ""));
    while (!redoStack.empty()) redoStack.pop();
    center("📁 Folder created: " + name);
}

//delete folder/file
void cmd_rm(const string& name) {
    if (name.empty()) {
	 center("❌ rm: missing name");
	  return; 
	  }
    Node* target = findChild(currentNode, name);
    if (!target) { 
	cout << "\033[91m"; 
	center("❌ Item not found: " + name); 
	cout << "\033[0m"; return; 
	}
    if (target->isLocked) {
	 cout << "\033[91m"; 
	 center("🔒 Cannot delete: Item is LOCKED."); 
	 cout << "\033[0m"; return; 
	 }
    auto &children = currentNode->children;
    auto it = std::find(children.begin(), children.end(), target);
    int idx = (it == children.end() ? -1 : (int)distance(children.begin(), it));
    if (idx == -1) {
	 center("⚠ Internal error: cannot find child index.");
	 return; 
	 }
    undoStack.push(Action("delete", target, currentNode, idx, ""));
    while (!redoStack.empty()) redoStack.pop();
    children.erase(it);
    if (!target->isFolder) deletePhysicalFile(target->name);
    if (target->isBookmarked) {
        target->isBookmarked = false;
        Bookmark* b = bookmarksHead; Bookmark* prev = NULL;
        while (b) {
            if (b->node == target) {
                if (prev) prev->next = b->next; else bookmarksHead = b->next;
                delete b; break;
            }
            prev = b; b = b->next;
        }
    }
    center("🗑️ Deleted: " + name);
}

//rename folder /file
void cmd_rename(const string& rest) {
    size_t spacePos = rest.find(" ");
    if (spacePos == string::npos) {
        center("\033[91m❌ rename: missing arguments\033[0m");
        return;
    }

    string oldName = rest.substr(0, spacePos);
    string newName = rest.substr(spacePos + 1);

    Node* target = findChild(currentNode, oldName);
    if (!target) {
        center("\033[91m🚫 Item not found\033[0m");
        return;
    }

    if (target->isLocked) {
        center("\033[93m🔒 Cannot rename: Item is locked\033[0m");
        return;
    }

    if (findChild(currentNode, newName)) {
        center("\033[93m⚠️ Name already exists\033[0m");
        return;
    }

    target->name = newName;
    center("\033[92m✨ Renamed Successfully\033[0m");
}

// going into folder
void cmd_cd(const string& name) {
    if (name.empty()) { 
	center("❌ cd: missing argument"); 
	return;
	 }
    if (name == "/") { 
	currentNode = ROOT; 
	center("📁 Now at ROOT"); 
	return; 
	}
    if (name == "..") { 
	if (currentNode->parent) { 
	currentNode = currentNode->parent; 
	center("⬆️ Moved to parent folder");
	 } 
	 else 
	 center("⚠ Already at ROOT"); 
	 return; 
	 }
    Node* target = findChild(currentNode, name);
    if (!target || !target->isFolder) {
	 center("❌ Folder not found: " + name); 
	 return; 
	 }
    if (target->isLocked) { 
	center("🔒 Cannot enter: Folder is LOCKED.");
	 return; 
	 }
    currentNode = target; center("📁 Entered: " + name);
}

//search any file folder
void cmd_search(const string& rest) {
    string keyword = rest;
    bool useDFS = true;
    size_t spacePos = rest.find(" ");
    if (spacePos != string::npos) {
        keyword = rest.substr(0, spacePos);
        string mode = rest.substr(spacePos + 1);
        if (mode == "bfs") useDFS = false;
    }

    vector<string> results;
    if (useDFS) searchDFS(ROOT, keyword, results);
    else searchBFS(ROOT, keyword, results);

    center("🔍 Search results for \"" + keyword + "\" (" + (useDFS ? "DFS" : "BFS") + ")");

    if (results.empty()) {
        center("(🚫 No matches)");
    } else {
        for (size_t i = 0; i < results.size(); ++i) {
            center(results[i]);
        }
    }
}


//--------------------------->>>>>>>>>tree for workspace
void cmd_tree() {
    treeLines.clear();

    generateTree(ROOT, "", true);

    int maxLen = 0;
    for (auto &line : treeLines)
        maxLen = std::max(maxLen, (int)line.length());

    int screenWidth = getConsoleWidth();
    int offset = std::max(0, (screenWidth - maxLen) / 2);

    cout << "\n";
    string title = "🌲 DIRECTORY TREE VIEW 🌲";
    cout << string((screenWidth - title.length()) / 2, ' ') << title << "\n\n";

    for (auto &line : treeLines) {
        cout << string(offset, ' ') << line << "\n";
    }

    cout << "\n";
}

// ----------------------------------->>>>>>>>>> history commands
void cmd_history() {
    int padding = getLeftPadding();
    
    if (commandHistory.empty()) {
        cout << string(padding, ' ') << "\033[91m(No history)\033[0m\n";
        return;
    }

    cout << string(padding, ' ') << "📜 Command History:\n";
    for (const auto &cmd : commandHistory) {
        cout << string(padding, ' ') << "➡️  " << cmd << endl;
    }
}
void clearCommandHistory() {
    commandHistory.clear();
    std::ofstream out("history.txt", std::ios::trunc);
    out.close();
    center("\033[92m🧹 Command History Cleared\033[0m");
}

//Undo
void cmd_undo() {
    if (undoStack.empty()) { center("⚠ Nothing to undo"); return; }
    Action a = undoStack.top(); undoStack.pop();

    if (a.type == "create") {
        if (a.parent) {
            auto &children = a.parent->children;
            auto it = std::find(children.begin(), children.end(), a.node);
            if (it != children.end()) children.erase(it);
            
            if (a.node && !a.node->isFolder) deletePhysicalFile(a.node->name);
            redoStack.push(Action("create", a.node, a.parent, a.index, ""));
            center("↩️ Undo: removed created item");
        } else center("⚠ Undo error: parent missing");
    }
    else if (a.type == "delete") {
        if (a.parent) {
            auto &children = a.parent->children;
            int idx = a.index;
            if (idx < 0 || idx > (int)children.size()) idx = (int)children.size();
            children.insert(children.begin() + idx, a.node);
            a.node->parent = a.parent;
            if (!a.node->isFolder) createPhysicalFile(a.node->name);
            redoStack.push(Action("delete", a.node, a.parent, idx, ""));
            center("↩️ Undo: restored deleted item");
        } else center("⚠ Undo error: parent missing");
    }
    else if (a.type == "bookmark_add") {
        string bname = a.extra;
        Bookmark* b = bookmarksHead; Bookmark* prev = NULL;
        while (b) {
            if (b->name == bname) {
                if (b->node) b->node->isBookmarked = false;
                if (prev) prev->next = b->next; else bookmarksHead = b->next;
                delete b;
                redoStack.push(Action("bookmark_add", nullptr, nullptr, -1, bname));
                center("↩️ Undo: removed bookmark " + bname);
                return;
            }
            prev = b; b = b->next;
        }
        center("⚠ Undo bookmark_add: not found");
    }
    else if (a.type == "bookmark_remove") {
    	
        string bname = a.extra;
        std::function<Node*(Node*)> findByName = [&](Node* nd)->Node* {
            if (!nd) return nullptr;
            if (nd->name == bname) return nd;
            for (auto ch : nd->children) {
                Node* r = findByName(ch);
                if (r) return r;
            }
            return nullptr;
        };
        Node* target = findByName(ROOT);
        if (target) {
            Bookmark* nb = new Bookmark(bname, target);
            nb->next = bookmarksHead; bookmarksHead = nb;
            target->isBookmarked = true;
            redoStack.push(Action("bookmark_remove", nullptr, nullptr, -1, bname));
            center("↩️ Undo: restored bookmark " + bname);
        } else center("⚠ Undo bookmark_remove: original target not found");
    }
    else if (a.type == "rename") {
        string oldNew = a.extra;
        size_t sep = oldNew.find('|');
        string oldName = (sep==string::npos? oldNew : oldNew.substr(0, sep));
        string newName = (sep==string::npos? string() : oldNew.substr(sep+1));
        if (a.node) {
            a.node->name = oldName;
            redoStack.push(Action("rename", a.node, a.parent, a.index, oldName + "|" + newName));
            center("↩️ Undo: rename reverted to " + oldName);
        } else center("⚠ Undo rename: node missing");
    }
    else {
        center("⚠ Unsupported undo action");
    }
}

//redo
void cmd_redo() {
    if (redoStack.empty()) { center("⚠ Nothing to redo"); return; }
    Action a = redoStack.top(); redoStack.pop();

    if (a.type == "create") {
        if (a.parent) {
            auto &children = a.parent->children;
            int idx = a.index;
            if (idx < 0 || idx > (int)children.size()) idx = (int)children.size();
            children.insert(children.begin() + idx, a.node);
            a.node->parent = a.parent;
            if (!a.node->isFolder) createPhysicalFile(a.node->name);
            undoStack.push(Action("create", a.node, a.parent, idx, ""));
            center("🔁 Redo: recreated item");
        } else center("⚠ Redo error: parent missing");
    }
    else if (a.type == "delete") {
        if (a.parent) {
            auto &children = a.parent->children;
            auto it = std::find(children.begin(), children.end(), a.node);
            if (it != children.end()) children.erase(it);
            if (!a.node->isFolder) deletePhysicalFile(a.node->name);
            undoStack.push(Action("delete", a.node, a.parent, a.index, ""));
            center("🔁 Redo: deleted item again");
        } else center("⚠ Redo error: parent missing");
    }
    else if (a.type == "bookmark_add") {
        string bname = a.extra;
        std::function<Node*(Node*)> findByName = [&](Node* nd)->Node* {
            if (!nd) return nullptr;
            if (nd->name == bname) return nd;
            for (auto ch : nd->children) {
                Node* r = findByName(ch);
                if (r) return r;
            }
            return nullptr;
        };
        Node* target = findByName(ROOT);
        if (target) {
            Bookmark* nb = new Bookmark(bname, target);
            nb->next = bookmarksHead; bookmarksHead = nb;
            target->isBookmarked = true;
            undoStack.push(Action("bookmark_add", nullptr, nullptr, -1, bname));
            center("🔁 Redo: bookmark added " + bname);
        } else center("⚠ Redo bookmark_add: node not found");
    }
    else if (a.type == "bookmark_remove") {
        string bname = a.extra;
        Bookmark* b = bookmarksHead; Bookmark* prev = NULL;
        while (b) {
            if (b->name == bname) {
                if (b->node) b->node->isBookmarked = false;
                if (prev) prev->next = b->next; else bookmarksHead = b->next;
                delete b;
                undoStack.push(Action("bookmark_remove", nullptr, nullptr, -1, bname));
                center("🔁 Redo: bookmark removed " + bname);
                return;
            }
            prev = b; b = b->next;
        }
        center("⚠ Redo bookmark_remove: not found");
    }
    else if (a.type == "rename") {
        string oldNew = a.extra;
        size_t sep = oldNew.find('|');
        string oldName = (sep==string::npos? oldNew : oldNew.substr(0, sep));
        string newName = (sep==string::npos? string() : oldNew.substr(sep+1));
        if (a.node) {
            a.node->name = newName;
            undoStack.push(Action("rename", a.node, a.parent, a.index, oldName + "|" + newName));
            center("🔁 Redo: renamed to " + newName);
        } else center("⚠ Redo rename: node missing");
    }
    else {
        center("⚠ Unsupported redo action");
    }
}

//-----------------------------------------------------------Complete File handling------------------------------------------------

// ------------------>>>>>>>>save and load history
void saveHistoryToFile() {
    std::ofstream out("history.txt", std::ios::trunc);
    for (size_t i = 0; i < commandHistory.size(); ++i) out << commandHistory[i] << "\n";
    out.close();
}
void loadHistoryFromFile() {
    commandHistory.clear();
    std::ifstream in("history.txt");
    if (!in) return;
    string line;
    while (std::getline(in, line)) {
        if (!line.empty()) commandHistory.push_back(line);
    }
    in.close();
}

// ------------------------------->>>>>save bookmarks
void saveBookmarks(std::ofstream &out) {
    out << "BOOKMARKS\n";
    Bookmark* b = bookmarksHead;
    while (b) {
        if (b->node) {
            out << "BOOKMARK " << b->name << " | " << joinPath(b->node) << "\n";
        }
        b = b->next;
    }
}

// ------------------------------>>>>>>>>>> save / load workspace 
void saveWorkspace(Node* node, std::ofstream &out, int depth = 0) {
    if (!node) return;
    out << depth << " " << (node->isFolder ? "D" : "F") << " " << (node->isLocked ? "1" : "0") << " " << node->name << "\n";
    if (!node->isFolder) {
        std::istringstream ss(node->fileContent);
        string line;
        while (std::getline(ss, line)) out << "CONTENT " << line << "\n";
    }
    for (Node* child : node->children) saveWorkspace(child, out, depth + 1);
}

//------------------------------>>>>>>>>> Load workspace + bookmarks
void loadWorkspace(Node* root, std::ifstream &in) {
    root->children.clear();
    currentNode = root;
    while (bookmarksHead) {
	 Bookmark* t = bookmarksHead;
	  bookmarksHead = bookmarksHead->next;
	  delete t; 
	  }

    vector<Node*> depthNodes;
    depthNodes.resize(1);
    depthNodes[0] = root;

    string line;
    Node* lastFileNode = NULL;
    bool inBookmarkSection = false;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (!inBookmarkSection && line == "BOOKMARKS") {
		 inBookmarkSection = true; 
		 continue;
		  }
        if (inBookmarkSection) {
            if (line.rfind("BOOKMARK ", 0) == 0) {
                string rest = line.substr(9);
                size_t sep = rest.find(" | ");
                string bname, bpath;
                if (sep != string::npos) { 
				bname = rest.substr(0, sep);
				 bpath = rest.substr(sep + 3);
				  }
                else {
                    size_t sp = rest.find(' ');
                    if (sp != string::npos) {
					 bname = rest.substr(0, sp); 
					 bpath = rest.substr(sp + 1);
					  }
                    else { 
					bname = rest; 
					bpath = "";
					 }
                }
                if (!bpath.empty()) {
                    Node* target = findNodeByPath(bpath);
                    if (target) { 
					Bookmark* nb = new Bookmark(bname, target); 
					nb->next = bookmarksHead; bookmarksHead = nb; 
					target->isBookmarked = true; 
					}
                }
            }
            continue;
        }
        if (line.rfind("CONTENT ", 0) == 0) {
            if (lastFileNode) lastFileNode->fileContent += line.substr(8) + "\n";
            continue;
        }
        stringstream ss(line);
        int depth;
        char type;
        int lockState;
        if (!(ss >> depth >> type >> lockState)) continue;
        string name;
        getline(ss, name);
        if (!name.empty() && name[0] == ' ') name = name.substr(1);
        if (depth == 0) { 
		root->name = name;
		root->isFolder = (type == 'D'); 
		root->isLocked = (lockState == 1); 
		lastFileNode = NULL;
		continue; 
		  }
        if ((int)depth >= (int)depthNodes.size()) depthNodes.resize(depth + 1, NULL);
        Node* parent = depthNodes[depth - 1];
        if (!parent) { 
		lastFileNode = NULL; 
		continue; 
		}
        Node* newNode = new Node(name, (type == 'D'), parent);
        newNode->isLocked = (lockState == 1);
        parent->children.push_back(newNode);
        depthNodes[depth] = newNode;
        lastFileNode = (newNode->isFolder ? NULL : newNode);
    }
}


//-------------------------------------------------------->>>>>>>>>     Main part of Project      -------------------------------------------
int main() {
	SetConsoleOutputCP(CP_UTF8); 
    system("chcp 65001 > nul"); 
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hConsole, &dwMode);
    dwMode |= 0x0004; 
    SetConsoleMode(hConsole, dwMode);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);

    int screenWidth = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    int boxWidth = 50;
    int padding = (screenWidth - boxWidth) / 2;

    auto center = [&](string text){
    cout << string(padding, ' ') << text;
};

    initCommandTrie(); 
     
    
    displayUIHeader();

     
    std::ifstream in("workspace.txt");
     
    loadHistoryFromFile();
    if (in) {
        loadWorkspace(ROOT, in);
        in.close();
        cout << "\033[93m";
        center("╔══════════════════════════════════════════════════╗\n");
        center("║ \033[97m📂 Auto-loaded workspace from workspace.txt      \033[93m║\n");
        center("╚══════════════════════════════════════════════════╝\n");
        cout << "\033[0m";
    }

    cout << "\033[92m";
    center("💡 Tip: Type \033[93mhelp\033[92m to view available commands\n");
    cout << "\033[0m";


    showPrompt();
    
    while (true) {

        char ch = _getch();
        
        if (ch == 13) {
            cout << endl;
            if (!inputBuffer.empty()) {
                commandHistory.push_back(inputBuffer);
                saveHistoryToFile();
            }

            string cmd, rest;
            parseCommandLine(inputBuffer, cmd, rest);
            inputBuffer.clear();

            if (cmd == "exit") {
                std::ofstream out("workspace.txt");
                if (out) {
                    saveWorkspace(ROOT, out, 0);
                    saveBookmarks(out);
                    out.close();
                }
                saveHistoryToFile();
                cout << "\033[93m";
                center("╔══════════════════════════════════════════════════╗\n");
                center("║ \033[97m            📁 Auto-saved workspace              \033[93m║\n");
                center("╚══════════════════════════════════════════════════╝\n");
                cout << "\033[0m";
                break;
            }

            else if (cmd == "help"){
            	cout << "\033[93m";
                center("╔══════════════════════════════════════════════════╗\n");
                center("║ \033[97m             📘 HELP MENU 📘                     \033[93m║\n");
                center("╠══════════════════════════════════════════════════╣\n");
                center("║ \033[97m📁 ls                       → list items         \033[93m║\n");
                center("║ \033[97m📂 mkdir <name>             → create folder      \033[93m║\n");
                center("║ \033[97m❌ rm <name>                → remove item        \033[93m║\n");
                center("║ \033[97m📄 touch <file>             → create file        \033[93m║\n");
                center("║ \033[97m👀 view <file>              → view file          \033[93m║\n");
                center("║ \033[97m📝 edit <file>              → edit file          \033[93m║\n");
                center("║ \033[97m✏ rename <old> <new>        → rename item        \033[93m║\n");
                center("║ \033[97m🔁 cd <name|..>             → change folder      \033[93m║\n");
                center("║ \033[97m🌳 tree                     → directory tree     \033[93m║\n");
                center("║ \033[97m🌳 infixtree <file.txt>     → infixtree    \033[93m      ║\n");
                center("║ \033[97m🌳 prefixtree <file.txt>    → prefixtree         \033[93m║\n");
                center("║ \033[97m🌳 postfixtree <file.txt>   →  postfixtree       \033[93m║\n");
                center("║ \033[97m↩ undo / redo               → undo redo          \033[93m║\n");
                center("║ \033[97m🔍 search <k> [dfs|bfs]     → search item        \033[93m║\n");
                center("║ \033[97m🔖 bookmark add <name>      → add bookmark       \033[93m║\n");
                center("║ \033[97m📌 bookmark list            → list bookmarks     \033[93m║\n");
                center("║ \033[97m🚀 bookmark go <name>       → go bookmark        \033[93m║\n");
                center("║ \033[97m🗑 bookmark remove <name>    → del bookmark       \033[93m║\n");
                center("║ \033[97m🕘 history                  → cmd history        \033[93m║\n");
                center("║ \033[97m🧹 clearhistory             → clear history      \033[93m║\n");
                center("║ \033[97m🔒 lock <item>              → lock file/folder   \033[93m║\n");
                center("║ \033[97m🔓 unlock <item>            → unlock item        \033[93m║\n");
                center("║ \033[97m🧼 clear                    → refresh UI         \033[93m║\n");
                center("║ \033[97m❎ exit                     → quit explorer      \033[93m║\n");
                center("╚══════════════════════════════════════════════════╝\n");
                cout << "\033[0m";
			}
            else if (cmd == "postfixtree") cmd_postfix_tree(rest);
            else if (cmd == "prefixtree")  cmd_prefix_tree(rest);
            else if (cmd == "infixtree")   cmd_infix_tree(rest);
            else if (cmd == "ls") cmd_ls();
            else if (cmd == "mkdir") cmd_mkdir(rest);
            else if (cmd == "rm") cmd_rm(rest);
            else if (cmd == "cd") cmd_cd(rest);
            else if (cmd == "tree") cmd_tree();
            else if (cmd == "undo") cmd_undo();
            else if (cmd == "redo") cmd_redo();
            else if (cmd == "rename") cmd_rename(rest);
            else if (cmd == "search") cmd_search(rest);
            else if (cmd == "touch") cmd_touch(rest);
            else if (cmd == "view") cmd_view(rest);
            else if (cmd == "edit") cmd_edit(rest);
            else if (cmd == "lock") {
                if (rest.empty())
                   center("\033[93m⚠️ Usage: lock <name>\033[0m");
                else
                   lockItem(currentNode, rest);
            }
            else if (cmd == "unlock") {
                if (rest.empty())
                   center("\033[93m⚠️ Usage: unlock <name>\033[0m");
                else
                   unlockItem(currentNode, rest);
            }
            else if (cmd == "bookmark") {
                string sub, arg;
                parseCommandLine(rest, sub, arg);
                if (sub == "add") bookmark_add(arg);
                else if (sub == "list") bookmark_list();
                else if (sub == "go") bookmark_go(arg);
                else if (sub == "remove") bookmark_remove(arg);
                else center("❌ Unknown bookmark command: " + sub);
            }
            else if (cmd == "history") cmd_history();
            else if (cmd == "clearhistory") clearCommandHistory();

            else if (cmd == "clear") {
                clearConsoleSim();
                displayUIHeader();
                
        cout << "\033[0m";
                cout << "\033[92m";
                center("      💡 Tip: Type \033[93mhelp\033[92m to view available commands\n");
                cout << "\033[0m";
            } 
			else {
                center("         \033[91m❌ Unknown Command: " + cmd + "\033[0m\n");
            }

            showPrompt();
            continue;
       }
       
        else if (ch == '\t') {
            vector<string> suggestions;
            string lowerBuf = toLowerCopy(inputBuffer);
            suggestions = trieSuggest(lowerBuf, 1);
            if (!suggestions.empty())
                inputBuffer = suggestions[0];
        }

        else if (ch == 8) {
            if (!inputBuffer.empty())
                inputBuffer.pop_back();
        }

        else if (isprint(ch)) {
            inputBuffer.push_back(ch);
        }

        cout << "\r"; 
        showPrompt();
        cout << inputBuffer;

        cout << "\033[J"; 
        cout.flush();

    }
displayUIfoter();
    return 0;
}
