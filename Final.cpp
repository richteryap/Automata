#include <iostream>
#include <vector>
#include <stack>
#include <set>
#include <map>
#include <memory>
#include <cctype>
#include <algorithm>
#include <sstream>
#include <iomanip>

using namespace std;

// Token types
enum class TokenType {
    IDENTIFIER,
    NUMBER,
    OPERATOR,
    LPAREN,
    RPAREN,
    ASSIGN,
    END_OF_INPUT,
    INVALID
};

// Token structure
struct Token {
    TokenType type;
    string value;
    int position;

    Token(TokenType t, const string& v, int pos) : type(t), value(v), position(pos) {}
};

// Forward declarations
class Lexer;
class PDA;

class CompilerFrontEnd {
private:
    Lexer* lexer;
    PDA* pda;

public:
    CompilerFrontEnd();
    ~CompilerFrontEnd();
    
    void runSimulator();
    void demonstrateNFAToDFA();
    void analyzeInput(const string& input);
};

// NFA state structure
struct State {
    int id;
    map<char, vector<shared_ptr<State>>> transitions;
    bool isFinal;

    State(int id) : id(id), isFinal(false) {}
};

// NFA fragment
struct NFAPtr {
    shared_ptr<State> start;
    shared_ptr<State> end;
};

// DFA state structure
struct DFAState {
    int id;
    set<shared_ptr<State>> nfaStates;
    map<char, shared_ptr<DFAState>> transitions;
    bool isFinal;

    DFAState(int id, const set<shared_ptr<State>>& states) 
        : id(id), nfaStates(states), isFinal(false) {}
};

// Lexer class (from Part 5)
class Lexer {
private:
    shared_ptr<DFAState> identifierDFA;
    shared_ptr<DFAState> numberDFA;
    map<char, TokenType> singleCharTokens;
    vector<shared_ptr<State>> states;
    int stateCounter;

public:
    Lexer() : stateCounter(0) {
        initializeTokenDFAs();
        initializeSingleCharTokens();
    }

    void initializeTokenDFAs() {
        NFAPtr identifierNFA = createIdentifierNFA();
        identifierDFA = convertToDFA(identifierNFA);

        NFAPtr numberNFA = createNumberNFA();
        numberDFA = convertToDFA(numberNFA);
    }

    void initializeSingleCharTokens() {
        singleCharTokens['+'] = TokenType::OPERATOR;
        singleCharTokens['-'] = TokenType::OPERATOR;
        singleCharTokens['*'] = TokenType::OPERATOR;
        singleCharTokens['/'] = TokenType::OPERATOR;
        singleCharTokens['='] = TokenType::ASSIGN;
        singleCharTokens['('] = TokenType::LPAREN;
        singleCharTokens[')'] = TokenType::RPAREN;
    }

    vector<Token> tokenize(const string& input) {
        vector<Token> tokens;
        int pos = 0;
        int length = input.length();

        while (pos < length) {
            if (isspace(input[pos])) {
                pos++;
                continue;
            }

            Token token = getNextToken(input, pos);
            tokens.push_back(token);
            pos += token.value.length();
        }

        tokens.push_back(Token(TokenType::END_OF_INPUT, "", pos));
        return tokens;
    }

    void displayTokens(const vector<Token>& tokens) {
        cout << "\n=== LEXICAL ANALYSIS ===\n";
        cout << "Position | Type        | Value\n";
        cout << "---------|-------------|-------\n";
        
        for (const auto& token : tokens) {
            if (token.type == TokenType::END_OF_INPUT) {
                cout << " " << token.position << "     | END_OF_INPUT | \n";
                continue;
            }

            string typeStr;
            switch (token.type) {
                case TokenType::IDENTIFIER: typeStr = "IDENTIFIER"; break;
                case TokenType::NUMBER:     typeStr = "NUMBER    "; break;
                case TokenType::OPERATOR:   typeStr = "OPERATOR  "; break;
                case TokenType::LPAREN:     typeStr = "LPAREN    "; break;
                case TokenType::RPAREN:     typeStr = "RPAREN    "; break;
                case TokenType::ASSIGN:     typeStr = "ASSIGN    "; break;
                case TokenType::INVALID:    typeStr = "INVALID   "; break;
                default:                    typeStr = "UNKNOWN   "; break;
            }

            cout << " " << token.position << "     | " << typeStr << " | " << token.value << "\n";
        }
    }

    void demonstrateAutomata() {
        cout << "\n=== REGULAR LANGUAGE DEMONSTRATION ===\n";
        
        cout << "\n1. Identifier Pattern: [a-zA-Z_][a-zA-Z0-9_]*\n";
        NFAPtr idNFA = createIdentifierNFA();
        auto idDFA = convertToDFA(idNFA);
        displayDFA(idDFA);

        cout << "\n2. Number Pattern: [0-9]+(\\.[0-9]+)?\n";
        NFAPtr numNFA = createNumberNFA();
        auto numDFA = convertToDFA(numNFA);
        displayDFA(numDFA);
    }

private:
    // NFA/DFA implementation (from previous parts)
    shared_ptr<State> createState() {
        auto state = make_shared<State>(stateCounter++);
        states.push_back(state);
        return state;
    }

    NFAPtr createCharNFA(char c) {
        auto start = createState();
        auto end = createState();
        start->transitions[c].push_back(end);
        return {start, end};
    }

    NFAPtr createCharClassNFA(const string& charClass) {
        auto start = createState();
        auto end = createState();
        
        set<char> validChars;
        parseCharacterClass(charClass, validChars);
        
        for (char c : validChars) {
            start->transitions[c].push_back(end);
        }
        
        return {start, end};
    }

    void parseCharacterClass(const string& charClass, set<char>& validChars) {
        if (charClass.empty() || charClass[0] != '[' || charClass.back() != ']') {
            throw invalid_argument("Invalid character class format");
        }

        string content = charClass.substr(1, charClass.length() - 2);
        size_t i = 0;

        while (i < content.length()) {
            if (i + 2 < content.length() && content[i + 1] == '-') {
                char start = content[i];
                char end = content[i + 2];
                
                if (start > end) throw invalid_argument("Invalid character range");
                
                for (char c = start; c <= end; c++) {
                    validChars.insert(c);
                }
                i += 3;
            } else {
                validChars.insert(content[i]);
                i += 1;
            }
        }
    }

    NFAPtr concatenate(NFAPtr a, NFAPtr b) {
        a.end->transitions['\0'].push_back(b.start);
        a.end->isFinal = false;
        return {a.start, b.end};
    }

    NFAPtr unionNFA(NFAPtr a, NFAPtr b) {
        auto start = createState();
        auto end = createState();

        start->transitions['\0'].push_back(a.start);
        start->transitions['\0'].push_back(b.start);

        a.end->transitions['\0'].push_back(end);
        b.end->transitions['\0'].push_back(end);

        a.end->isFinal = false;
        b.end->isFinal = false;
        end->isFinal = true;

        return {start, end};
    }

    NFAPtr kleeneStar(NFAPtr a) {
        auto start = createState();
        auto end = createState();

        start->transitions['\0'].push_back(a.start);
        start->transitions['\0'].push_back(end);

        a.end->transitions['\0'].push_back(a.start);
        a.end->transitions['\0'].push_back(end);

        a.end->isFinal = false;
        end->isFinal = true;

        return {start, end};
    }

    NFAPtr oneOrMore(NFAPtr a) {
        return concatenate(a, kleeneStar(a));
    }

    NFAPtr zeroOrOne(NFAPtr a) {
        auto start = createState();
        auto end = createState();

        start->transitions['\0'].push_back(a.start);
        start->transitions['\0'].push_back(end);

        a.end->transitions['\0'].push_back(end);

        a.end->isFinal = false;
        end->isFinal = true;

        return {start, end};
    }

    NFAPtr createIdentifierNFA() {
        NFAPtr firstChar = createCharClassNFA("[a-zA-Z_]");
        NFAPtr subsequentChars = createCharClassNFA("[a-zA-Z0-9_]");
        NFAPtr zeroOrMoreSubsequent = kleeneStar(subsequentChars);
        NFAPtr identifier = concatenate(firstChar, zeroOrMoreSubsequent);
        return identifier;
    }

    NFAPtr createNumberNFA() {
        NFAPtr integerPart = createCharClassNFA("[0-9]");
        NFAPtr oneOrMoreDigits = oneOrMore(integerPart);
        NFAPtr dot = createCharNFA('.');
        NFAPtr decimalDigits = createCharClassNFA("[0-9]");
        NFAPtr oneOrMoreDecimal = oneOrMore(decimalDigits);
        NFAPtr decimalPart = concatenate(dot, oneOrMoreDecimal);
        NFAPtr optionalDecimal = zeroOrOne(decimalPart);
        NFAPtr number = concatenate(oneOrMoreDigits, optionalDecimal);
        return number;
    }

    Token getNextToken(const string& input, int startPos) {
        char firstChar = input[startPos];
        if (singleCharTokens.count(firstChar)) {
            return Token(singleCharTokens[firstChar], string(1, firstChar), startPos);
        }

        Token identifierToken = tryMatchDFA(identifierDFA, input, startPos, TokenType::IDENTIFIER);
        if (identifierToken.type != TokenType::INVALID) {
            return identifierToken;
        }

        Token numberToken = tryMatchDFA(numberDFA, input, startPos, TokenType::NUMBER);
        if (numberToken.type != TokenType::INVALID) {
            return numberToken;
        }

        return Token(TokenType::INVALID, string(1, firstChar), startPos);
    }

    Token tryMatchDFA(shared_ptr<DFAState> dfa, const string& input, int startPos, TokenType type) {
        auto currentState = dfa;
        int pos = startPos;
        int lastAcceptPos = -1;
        string matchedValue;

        while (pos < input.length() && currentState->transitions.count(input[pos])) {
            currentState = currentState->transitions[input[pos]];
            pos++;

            if (currentState->isFinal) {
                lastAcceptPos = pos;
                matchedValue = input.substr(startPos, pos - startPos);
            }
        }

        if (lastAcceptPos != -1) {
            return Token(type, matchedValue, startPos);
        }

        return Token(TokenType::INVALID, "", startPos);
    }

    void epsilonClosure(const set<shared_ptr<State>>& states, set<shared_ptr<State>>& closure) {
        stack<shared_ptr<State>> stack;
        for (auto state : states) {
            stack.push(state);
            closure.insert(state);
        }

        while (!stack.empty()) {
            auto current = stack.top();
            stack.pop();

            if (current->transitions.count('\0')) {
                for (auto nextState : current->transitions['\0']) {
                    if (!closure.count(nextState)) {
                        closure.insert(nextState);
                        stack.push(nextState);
                    }
                }
            }
        }
    }

    shared_ptr<DFAState> convertToDFA(NFAPtr nfa) {
        map<set<shared_ptr<State>>, shared_ptr<DFAState>> dfaStateMap;
        vector<shared_ptr<DFAState>> dfaStates;
        int dfaStateCounter = 0;

        set<char> alphabet = getAlphabet(nfa);
        set<shared_ptr<State>> startSet = {nfa.start};
        set<shared_ptr<State>> startClosure;
        epsilonClosure(startSet, startClosure);

        auto startDFA = make_shared<DFAState>(dfaStateCounter++, startClosure);
        dfaStateMap[startClosure] = startDFA;
        dfaStates.push_back(startDFA);

        for (auto state : startClosure) {
            if (state->isFinal) {
                startDFA->isFinal = true;
                break;
            }
        }

        stack<shared_ptr<DFAState>> unprocessed;
        unprocessed.push(startDFA);

        while (!unprocessed.empty()) {
            auto currentDFA = unprocessed.top();
            unprocessed.pop();

            for (char c : alphabet) {
                if (c == '\0') continue;

                set<shared_ptr<State>> nextNFAStates;
                for (auto nfaState : currentDFA->nfaStates) {
                    if (nfaState->transitions.count(c)) {
                        for (auto nextState : nfaState->transitions[c]) {
                            nextNFAStates.insert(nextState);
                        }
                    }
                }

                if (!nextNFAStates.empty()) {
                    set<shared_ptr<State>> nextClosure;
                    epsilonClosure(nextNFAStates, nextClosure);

                    if (!dfaStateMap.count(nextClosure)) {
                        auto newDFA = make_shared<DFAState>(dfaStateCounter++, nextClosure);
                        dfaStateMap[nextClosure] = newDFA;
                        dfaStates.push_back(newDFA);

                        for (auto state : nextClosure) {
                            if (state->isFinal) {
                                newDFA->isFinal = true;
                                break;
                            }
                        }

                        unprocessed.push(newDFA);
                    }

                    currentDFA->transitions[c] = dfaStateMap[nextClosure];
                }
            }
        }

        return startDFA;
    }

    set<char> getAlphabet(NFAPtr nfa) {
        set<char> alphabet;
        set<int> visited;
        collectAlphabet(nfa.start, alphabet, visited);
        return alphabet;
    }

    void collectAlphabet(shared_ptr<State> state, set<char>& alphabet, set<int>& visited) {
        if (visited.count(state->id)) return;
        visited.insert(state->id);

        for (const auto& transition : state->transitions) {
            char symbol = transition.first;
            if (symbol != '\0') alphabet.insert(symbol);
            for (const auto& nextState : transition.second) {
                collectAlphabet(nextState, alphabet, visited);
            }
        }
    }

    void displayDFA(shared_ptr<DFAState> start) {
        cout << "DFA States and Transitions:\n";
        set<int> visited;
        displayDFAState(start, visited);
    }

    void displayDFAState(shared_ptr<DFAState> state, set<int>& visited) {
        if (visited.count(state->id)) return;
        visited.insert(state->id);

        cout << "State " << state->id;
        if (state->isFinal) cout << " [FINAL]";
        cout << ":\n";

        for (const auto& transition : state->transitions) {
            cout << "  --" << transition.first << "--> State " << transition.second->id << "\n";
        }

        for (const auto& transition : state->transitions) {
            displayDFAState(transition.second, visited);
        }
    }
};

// PDA State
enum class PDAState {
    START,
    EXPECT_ASSIGNMENT,
    AFTER_ASSIGNMENT,
    EXPECT_EXPR,
    IN_EXPR,
    AFTER_OPERATOR,
    ACCEPT,
    REJECT
};

// PDA class (from Part 6)
class PDA {
private:
    stack<string> stackVar;

public:
    PDA() {}

    bool parse(const vector<Token>& tokens) {
        stackVar = stack<string>();
        stackVar.push("$");
        PDAState currentState = PDAState::START;
        int tokenIndex = 0;

        cout << "\n=== SYNTACTIC ANALYSIS ===\n";
        cout << "PDA Parsing Steps:\n";
        cout << "State        | Stack Top  | Input Token  | Action\n";
        cout << "-------------|------------|--------------|--------\n";

        while (currentState != PDAState::ACCEPT && currentState != PDAState::REJECT) {
            Token currentToken = (tokenIndex < tokens.size()) ? tokens[tokenIndex] : Token(TokenType::END_OF_INPUT, "", -1);
            string stackTop = stackVar.top();

            displayState(currentState, stackTop, currentToken);

            switch (currentState) {
                case PDAState::START:
                    if (currentToken.type == TokenType::IDENTIFIER) {
                        if (tokenIndex + 1 < tokens.size() && tokens[tokenIndex + 1].type == TokenType::ASSIGN) {
                            currentState = PDAState::EXPECT_ASSIGNMENT;
                            tokenIndex++;
                            cout << "goto EXPECT_ASSIGNMENT, consume identifier\n";
                        } else {
                            stackVar.push("E");
                            currentState = PDAState::EXPECT_EXPR;
                            cout << "Push E, goto EXPECT_EXPR\n";
                        }
                    }
                    else if (currentToken.type == TokenType::NUMBER || currentToken.type == TokenType::LPAREN) {
                        stackVar.push("E");
                        currentState = PDAState::EXPECT_EXPR;
                        cout << "Push E, goto EXPECT_EXPR\n";
                    }
                    else {
                        currentState = PDAState::REJECT;
                        cout << "REJECT: Expected identifier, number, or '('\n";
                    }
                    break;

                case PDAState::EXPECT_ASSIGNMENT:
                    if (currentToken.type == TokenType::ASSIGN) {
                        currentState = PDAState::AFTER_ASSIGNMENT;
                        tokenIndex++;
                        cout << "goto AFTER_ASSIGNMENT, consume '='\n";
                    } else {
                        currentState = PDAState::REJECT;
                        cout << "REJECT: Expected '=' after identifier\n";
                    }
                    break;

                case PDAState::AFTER_ASSIGNMENT:
                    stackVar.push("E");
                    currentState = PDAState::EXPECT_EXPR;
                    cout << "Push E, goto EXPECT_EXPR\n";
                    break;

                case PDAState::EXPECT_EXPR:
                    if (currentToken.type == TokenType::IDENTIFIER || 
                        currentToken.type == TokenType::NUMBER) {
                        stackVar.pop(); // Pop E
                        currentState = PDAState::IN_EXPR;
                        tokenIndex++;
                        cout << "Pop E, goto IN_EXPR, consume token\n";
                    }
                    else if (currentToken.type == TokenType::LPAREN) {
                        stackVar.pop(); // Pop E
                        stackVar.push(")");
                        stackVar.push("E");
                        currentState = PDAState::EXPECT_EXPR;
                        tokenIndex++;
                        cout << "Pop E, push ') E', goto EXPECT_EXPR, consume '('\n";
                    }
                    else {
                        currentState = PDAState::REJECT;
                        cout << "REJECT: Expected expression after '='\n";
                    }
                    break;

                case PDAState::IN_EXPR:
                    if (currentToken.type == TokenType::OPERATOR) {
                        stackVar.push("E");
                        currentState = PDAState::AFTER_OPERATOR;
                        tokenIndex++;
                        cout << "Push E, goto AFTER_OPERATOR, consume operator\n";
                    }
                    else if (currentToken.type == TokenType::RPAREN && stackTop == ")") {
                        stackVar.pop(); // Pop )
                        currentState = PDAState::IN_EXPR;
                        tokenIndex++;
                        cout << "Pop ')', goto IN_EXPR, consume ')'\n";
                    }
                    else if (currentToken.type == TokenType::END_OF_INPUT && stackTop == "$") {
                        stackVar.pop(); // Pop $
                        currentState = PDAState::ACCEPT;
                        cout << "Pop '$', ACCEPT\n";
                    }
                    else if (currentToken.type == TokenType::END_OF_INPUT) {
                        currentState = PDAState::REJECT;
                        cout << "REJECT: Unmatched parentheses or incomplete expression\n";
                    }
                    else {
                        currentState = PDAState::REJECT;
                        cout << "REJECT: Unexpected token in expression\n";
                    }
                    break;

                case PDAState::AFTER_OPERATOR:
                    if (currentToken.type == TokenType::IDENTIFIER || 
                        currentToken.type == TokenType::NUMBER) {
                        stackVar.pop(); // Pop E
                        currentState = PDAState::IN_EXPR;
                        tokenIndex++;
                        cout << "Pop E, goto IN_EXPR, consume token\n";
                    }
                    else if (currentToken.type == TokenType::LPAREN) {
                        stackVar.pop(); // Pop E
                        stackVar.push(")");
                        stackVar.push("E");
                        currentState = PDAState::EXPECT_EXPR;
                        tokenIndex++;
                        cout << "Pop E, push ') E', goto EXPECT_EXPR, consume '('\n";
                    }
                    else {
                        currentState = PDAState::REJECT;
                        cout << "REJECT: Expected expression after operator\n";
                    }
                    break;

                default:
                    currentState = PDAState::REJECT;
                    cout << "REJECT: Invalid state\n";
                    break;
            }

            if (tokenIndex > tokens.size() + 5) {
                cout << "REJECT: Infinite loop detected\n";
                return false;
            }
        }

        bool result = currentState == PDAState::ACCEPT;
        cout << "\nSYNTAX RESULT: " << (result ? "VALID" : "INVALID") << "\n";
        return result;
    }

private:
    void displayState(PDAState state, const string& stackTop, const Token& token) {
        string stateStr;
        switch (state) {
            case PDAState::START: stateStr = "START"; break;
            case PDAState::EXPECT_ASSIGNMENT: stateStr = "EXPECT_ASGN"; break;
            case PDAState::AFTER_ASSIGNMENT: stateStr = "AFTER_ASGN"; break;
            case PDAState::EXPECT_EXPR: stateStr = "EXPECT_EXPR"; break;
            case PDAState::IN_EXPR: stateStr = "IN_EXPR"; break;
            case PDAState::AFTER_OPERATOR: stateStr = "AFTER_OP"; break;
            case PDAState::ACCEPT: stateStr = "ACCEPT"; break;
            case PDAState::REJECT: stateStr = "REJECT"; break;
        }

        string tokenStr;
        switch (token.type) {
            case TokenType::IDENTIFIER: tokenStr = "IDENT:" + token.value; break;
            case TokenType::NUMBER: tokenStr = "NUM:" + token.value; break;
            case TokenType::OPERATOR: tokenStr = "OP:" + token.value; break;
            case TokenType::LPAREN: tokenStr = "LPAREN"; break;
            case TokenType::RPAREN: tokenStr = "RPAREN"; break;
            case TokenType::ASSIGN: tokenStr = "ASSIGN"; break;
            case TokenType::END_OF_INPUT: tokenStr = "END"; break;
            case TokenType::INVALID: tokenStr = "INVALID"; break;
        }

        cout << left << setw(12) << stateStr << " | "
                << setw(11) << stackTop << " | "
                << setw(12) << tokenStr << " | ";
    }
};

// CompilerFrontEnd implementation
CompilerFrontEnd::CompilerFrontEnd() {
    lexer = new Lexer();
    pda = new PDA();
}

CompilerFrontEnd::~CompilerFrontEnd() {
    delete lexer;
    delete pda;
}

void CompilerFrontEnd::runSimulator() {
    cout << "========================================\n";
    cout << "    COMPILER FRONT-END SIMULATOR\n";
    cout << "========================================\n";
    
    while (true) {
        cout << "\n=== MAIN MENU ===\n";
        cout << "1. Analyze Calculator Expression\n";
        cout << "2. Demonstrate NFA/DFA Construction\n";
        cout << "3. Exit\n";
        cout << "Choose option: ";
        
        int choice;
        if (!(cin >> choice)) {
            cin.clear();
            cin.ignore(10000, '\n');
            cout << "Invalid input! Please enter a number 1-3.\n";
            continue;
        }
        cin.ignore();
        
        switch (choice) {
            case 1: {
                cout << "Enter calculator expression: ";
                string input;
                getline(cin, input);
                analyzeInput(input);
                break;
            }
            case 2:
                lexer->demonstrateAutomata();
                break;
            case 3:
                cout << "Exiting simulator...\n";
                return;
            default:
                cout << "Invalid choice!\n";
                continue;
        }
    }
}

void CompilerFrontEnd::analyzeInput(const string& input) {
    cout << "\n=== ANALYZING: \"" << input << "\" ===\n";
    
    // Lexical Analysis
    vector<Token> tokens = lexer->tokenize(input);
    lexer->displayTokens(tokens);
    
    // Syntactic Analysis
    bool syntaxValid = pda->parse(tokens);
    
    cout << "\n=== FINAL RESULT ===\n";
    cout << "Expression: \"" << input << "\"\n";
    cout << "Lexical Analysis: " << (tokens.size() > 0 ? "COMPLETED" : "FAILED") << "\n";
    cout << "Syntactic Analysis: " << (syntaxValid ? "VALID" : "INVALID") << "\n";
    cout << "Overall: " << (syntaxValid ? "VALID EXPRESSION" : "INVALID EXPRESSION") << "\n";
}

int main() {
    CompilerFrontEnd simulator;
    simulator.runSimulator();
    return 0;
}