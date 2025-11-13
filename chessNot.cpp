#include <iostream>
#include <vector>
#include <stack>
#include <set>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept> 
#include <cctype>

using namespace std;

// Chess token types
enum class ChessTokenType {
    MOVE_NUMBER,
    PAWN_MOVE,
    PIECE_MOVE,
    TWIN_PIECE_MOVE,
    PAWN_CAPTURE,
    PIECE_CAPTURE,
    TWIN_PIECE_CAPTURE,
    CASTLING,
    PROMOTION,
    PROMOTION_VIA_CAPTURE,
    CHECK,
    CHECKMATE,
    RESULT,
    END_OF_INPUT,
    INVALID
};

// Chess token structure
struct ChessToken {
    ChessTokenType type;
    string value;
    int position;

    ChessToken(ChessTokenType t, const string& v, int pos) : type(t), value(v), position(pos) {}
};

// ========== NFA/DFA STRUCTURES ==========
struct State {
    int id;
    map<char, vector<shared_ptr<State>>> transitions;
    bool isFinal;
    State(int id) : id(id), isFinal(false) {}
};

struct NFAPtr {
    shared_ptr<State> start;
    shared_ptr<State> end;
};

struct DFAState {
    int id;
    set<shared_ptr<State>> nfaStates;
    map<char, shared_ptr<DFAState>> transitions;
    bool isFinal;
    DFAState(int id, const set<shared_ptr<State>>& states)
        : id(id), nfaStates(states), isFinal(false) {}
};

class ChessNFA {
private:
    vector<shared_ptr<State>> states;
    int stateCounter;

public:
    ChessNFA() : stateCounter(0) {}

    shared_ptr<State> createState() {
        auto state = make_shared<State>(stateCounter++);
        states.push_back(state);
        return state;
    }

    NFAPtr createCharNFA(char c) {
        auto start = createState();
        auto end = createState();
        start->transitions[c].push_back(end);
        end->isFinal = true;
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

        end->isFinal = true;
        return {start, end};
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

    NFAPtr createMoveNumberNFA() {
        NFAPtr digit = createCharClassNFA("[0-9]");
        NFAPtr oneOrMoreDigits = oneOrMore(digit);
        NFAPtr dot = createCharNFA('.');
        return concatenate(oneOrMoreDigits, dot); // 1., 23.
    }

    NFAPtr createPawnMoveNFA() {
        NFAPtr alphabet = createCharClassNFA("[a-h]");
        NFAPtr number = createCharClassNFA("[1-8]");
        return concatenate(alphabet, number); // e4, d5
    }

    NFAPtr createPieceMoveNFA() {
        NFAPtr alphabet = createCharClassNFA("[a-h]");
        NFAPtr number = createCharClassNFA("[1-8]");
        NFAPtr square = concatenate(alphabet, number);

        NFAPtr piece = createCharClassNFA("[KQRBN]");
        return concatenate(piece, square); // Nf3, Bb5
    }

    NFAPtr createTwinPieceMoveNFA() {
        NFAPtr alphabet = createCharClassNFA("[a-h]");
        NFAPtr number = createCharClassNFA("[1-8]");
        NFAPtr square = concatenate(alphabet, number);

        NFAPtr file = createCharClassNFA("[a-h]");
        NFAPtr rank = createCharClassNFA("[1-8]");
        NFAPtr fileOrRank = unionNFA(file, rank);

        NFAPtr piece = createCharClassNFA("[QRBN]");
        NFAPtr pieceWithFileOrRank = concatenate(piece, fileOrRank);
        return concatenate(pieceWithFileOrRank, square); // Nbd2, R1e4
    }

    NFAPtr createPawnCaptureNFA() {
        NFAPtr alphabet = createCharClassNFA("[a-h]");
        NFAPtr number = createCharClassNFA("[1-8]");
        NFAPtr square = concatenate(alphabet, number);

        NFAPtr pawn = createCharClassNFA("[a-h]");
        NFAPtr capture = createCharNFA('x');
        return concatenate(pawn, concatenate(capture, square)); // exd5, bxc6
    }

    NFAPtr createPieceCaptureNFA() {
        NFAPtr alphabet = createCharClassNFA("[a-h]");
        NFAPtr number = createCharClassNFA("[1-8]");
        NFAPtr square = concatenate(alphabet, number);

        NFAPtr piece = createCharClassNFA("[KQRBN]");
        NFAPtr capture = createCharNFA('x');
        return concatenate(piece, concatenate(capture, square)); // Nxd5, Bxe4
    }

    NFAPtr createTwinPieceCaptureNFA() {
        NFAPtr alphabet = createCharClassNFA("[a-h]");
        NFAPtr number = createCharClassNFA("[1-8]");
        NFAPtr square = concatenate(alphabet, number);

        NFAPtr file = createCharClassNFA("[a-h]");
        NFAPtr rank = createCharClassNFA("[1-8]");
        NFAPtr fileOrRank = unionNFA(file, rank);

        NFAPtr piece = createCharClassNFA("[QRBN]");
        NFAPtr pieceWithFileOrRank = concatenate(piece, fileOrRank);
        NFAPtr capture = createCharNFA('x');
        return concatenate(pieceWithFileOrRank, concatenate(capture, square)); // Raxd5, N1xe4
    }

    NFAPtr createCastlingNFA() {
        NFAPtr o1 = createCharNFA('O');
        NFAPtr dash1 = createCharNFA('-');
        NFAPtr o2 = createCharNFA('O');
        NFAPtr kingside = concatenate(o1, concatenate(dash1, o2));

        NFAPtr o3 = createCharNFA('O');
        NFAPtr dash2 = createCharNFA('-');
        NFAPtr o4 = createCharNFA('O');
        NFAPtr dash3 = createCharNFA('-');
        NFAPtr o5 = createCharNFA('O');
        NFAPtr queenside = concatenate(o3, concatenate(dash2, concatenate(o4, concatenate(dash3, o5))));
        return unionNFA(kingside, queenside);
    }

    NFAPtr createPromotionNFA() {
        NFAPtr alphabet = createCharClassNFA("[a-h]");
        NFAPtr number = createCharClassNFA("[1-8]");

        NFAPtr pawn = concatenate(alphabet, number);
        NFAPtr promote = createCharNFA('=');
        NFAPtr pieceToPromote = createCharClassNFA("[QRBN]");
        return concatenate(pawn, concatenate(promote, pieceToPromote)); // e8=Q, d1=N
    }

    NFAPtr createPromotionViaCaptureNFA() {
        NFAPtr alphabet = createCharClassNFA("[a-h]");
        NFAPtr number = createCharClassNFA("[1-8]");
        NFAPtr sqaure = concatenate(alphabet, number);

        NFAPtr pawn = createCharClassNFA("[a-h]");
        NFAPtr capture = createCharNFA('x');
        NFAPtr promote = createCharNFA('=');
        NFAPtr pieceToPromote = createCharClassNFA("[QRBN]");
        return concatenate(pawn, concatenate(capture, concatenate(sqaure, concatenate(promote, pieceToPromote)))); // exd8=Q
    }

    NFAPtr createBaseMovesNFA() {
        return unionNFA(createPawnMoveNFA(),
               unionNFA(createPieceMoveNFA(),
               unionNFA(createTwinPieceMoveNFA(),
               unionNFA(createPawnCaptureNFA(),
               unionNFA(createPieceCaptureNFA(),
               unionNFA(createTwinPieceCaptureNFA(),
               unionNFA(createCastlingNFA(),
               unionNFA(createPromotionNFA(),
                        createPromotionViaCaptureNFA()))))))));
    }

    NFAPtr createCheckNFA() {
        NFAPtr moves = createBaseMovesNFA();
        NFAPtr check = createCharNFA('+');
        return concatenate(moves, check); // e4+, Nf3+
    }

    NFAPtr createCheckmateNFA() {
        NFAPtr moves = createBaseMovesNFA();
        NFAPtr checkmate = createCharNFA('#');
        return concatenate(moves, checkmate); // e4#, Nf3#
    }

    NFAPtr createResultNFA() {
        NFAPtr zero1 = createCharNFA('0');
        NFAPtr one1 = createCharNFA('1');
        NFAPtr dash1 = createCharNFA('-');
        NFAPtr whiteWins = concatenate(one1, concatenate(dash1, zero1));

        NFAPtr zero2 = createCharNFA('0');
        NFAPtr one2 = createCharNFA('1');
        NFAPtr dash2 = createCharNFA('-');
        NFAPtr blackWins = concatenate(zero2, concatenate(dash2, one2));

        NFAPtr simpleResults = unionNFA(whiteWins, blackWins);

        NFAPtr half1 = createCharNFA('1');
        NFAPtr slash1 = createCharNFA('/');
        NFAPtr two1 = createCharNFA('2');
        NFAPtr dash3 = createCharNFA('-');
        NFAPtr half2 = createCharNFA('1');
        NFAPtr slash2 = createCharNFA('/');
        NFAPtr two2 = createCharNFA('2');

        NFAPtr draw = concatenate(half1, concatenate(slash1, concatenate(two1,
                            concatenate(dash3, concatenate(half2, concatenate(slash2, two2))))));
        return unionNFA(simpleResults, draw);
    }

    void epsilonClosure(const set<shared_ptr<State>>& states, set<shared_ptr<State>>& closure) {
        stack<shared_ptr<State>> stack;
        for (auto state : states) { stack.push(state); closure.insert(state); }
        while (!stack.empty()) {
            auto current = stack.top(); stack.pop();
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

    set<char> getAlphabet(NFAPtr nfa) {
        set<char> alphabet;
        set<int> visited;
        collectAlphabet(nfa.start, alphabet, visited);
        return alphabet;
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
        for (auto state : startClosure) { if (state->isFinal) { startDFA->isFinal = true; break; } }
        stack<shared_ptr<DFAState>> unprocessed;
        unprocessed.push(startDFA);
        while (!unprocessed.empty()) {
            auto currentDFA = unprocessed.top(); unprocessed.pop();
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

private:
    void parseCharacterClass(const string& charClass, set<char>& validChars) {
        if (charClass.empty() || charClass[0] != '[' || charClass.back() != ']') {
            cerr << "[ERROR] Invalid character class format.\n";
            return;
        }
        string content = charClass.substr(1, charClass.length() - 2);
        size_t i = 0;
        while (i < content.length()) {
            if (i + 2 < content.length() && content[i + 1] == '-') {
                char start = content[i];
                char endChar = content[i + 2];
                if (start > endChar) throw invalid_argument("Invalid character range");
                for (char c = start; c <= endChar; c++) { validChars.insert(c); }
                i += 3;
            } else {
                validChars.insert(content[i]);
                i += 1;
            }
        }
    }

    NFAPtr oneOrMore(NFAPtr a) {
        return concatenate(a, kleeneStar(a));
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

    void collectAlphabet(shared_ptr<State> state, set<char>& alphabet, set<int>& visited) {
        if (visited.count(state->id)) return;
        visited.insert(state->id);
        for (const auto& transition : state->transitions) {
            char symbol = transition.first;
            if (symbol != '\0') { alphabet.insert(symbol); }
            for (const auto& nextState : transition.second) {
                collectAlphabet(nextState, alphabet, visited);
            }
        }
    }
};

// ========== CHESS LEXICAL ANALYZER ==========
class ChessLexer {
private:
    shared_ptr<DFAState> moveNumberDFA;
    shared_ptr<DFAState> pawnMoveDFA;
    shared_ptr<DFAState> pieceMoveDFA;
    shared_ptr<DFAState> twinPieceMoveDFA;
    shared_ptr<DFAState> pawnCaptureDFA;
    shared_ptr<DFAState> pieceCaptureDFA;
    shared_ptr<DFAState> twinPieceCaptureDFA;
    shared_ptr<DFAState> castlingDFA;
    shared_ptr<DFAState> promotionDFA;
    shared_ptr<DFAState> promotionViaCaptureDFA;
    shared_ptr<DFAState> checkDFA;
    shared_ptr<DFAState> checkmateDFA;
    shared_ptr<DFAState> resultDFA;

public:
    ChessLexer() {
        initializeTokenDFAs();
    }

    void initializeTokenDFAs() {
        ChessNFA nfaBuilder;
        moveNumberDFA = nfaBuilder.convertToDFA(nfaBuilder.createMoveNumberNFA());
        pawnMoveDFA = nfaBuilder.convertToDFA(nfaBuilder.createPawnMoveNFA());
        pieceMoveDFA = nfaBuilder.convertToDFA(nfaBuilder.createPieceMoveNFA());
        twinPieceMoveDFA = nfaBuilder.convertToDFA(nfaBuilder.createTwinPieceMoveNFA());
        pawnCaptureDFA = nfaBuilder.convertToDFA(nfaBuilder.createPawnCaptureNFA());
        pieceCaptureDFA = nfaBuilder.convertToDFA(nfaBuilder.createPieceCaptureNFA());
        twinPieceCaptureDFA = nfaBuilder.convertToDFA(nfaBuilder.createTwinPieceCaptureNFA());
        castlingDFA = nfaBuilder.convertToDFA(nfaBuilder.createCastlingNFA()); 
        promotionDFA = nfaBuilder.convertToDFA(nfaBuilder.createPromotionNFA());
        promotionViaCaptureDFA = nfaBuilder.convertToDFA(nfaBuilder.createPromotionViaCaptureNFA());
        checkDFA = nfaBuilder.convertToDFA(nfaBuilder.createCheckNFA());
        checkmateDFA = nfaBuilder.convertToDFA(nfaBuilder.createCheckmateNFA());
        resultDFA = nfaBuilder.convertToDFA(nfaBuilder.createResultNFA());
    }

    pair<vector<ChessToken>, bool> tokenize(const string& input) {
        vector<ChessToken> tokens;
        int pos = 0;
        int length = input.length();
        bool hadLexicalError = false;

        while (pos < length) {
            if (isspace(input[pos])) {
                pos++;
                continue;
            }

            ChessToken token = getNextToken(input, pos);
            if (token.type == ChessTokenType::INVALID) {
                cout << "[LEXER ERROR] Invalid token at position " << pos 
                    << ": '" << input.substr(pos, 1) << "' in context: '"
                    << input.substr(max(0, pos-3), min(7, (int)input.length()-max(0,pos-3))) << "'" << endl;
                
                hadLexicalError = true;
                pos++;
            } else {
                tokens.push_back(token);
                pos += token.value.length();
            }
        }

        tokens.push_back(ChessToken(ChessTokenType::END_OF_INPUT, "", pos));
        return {tokens, hadLexicalError};
    }

    ChessToken getNextToken(const string& input, int startPos) {
        ChessToken token = tryMatchLongest(input, startPos);
        if (token.type != ChessTokenType::INVALID) {
            return token;
        }
        return ChessToken(ChessTokenType::INVALID, string(1, input[startPos]), startPos);
    }

    ChessToken tryMatchLongest(const string& input, int startPos) {
        vector<pair<ChessTokenType, string>> candidates;

        testDFAPattern(moveNumberDFA, input, startPos, ChessTokenType::MOVE_NUMBER, candidates);
        testDFAPattern(resultDFA, input, startPos, ChessTokenType::RESULT, candidates);
        testDFAPattern(castlingDFA, input, startPos, ChessTokenType::CASTLING, candidates);
        testDFAPattern(pawnMoveDFA, input, startPos, ChessTokenType::PAWN_MOVE, candidates);
        testDFAPattern(pieceMoveDFA, input, startPos, ChessTokenType::PIECE_MOVE, candidates);
        testDFAPattern(pawnCaptureDFA, input, startPos, ChessTokenType::PAWN_CAPTURE, candidates);
        testDFAPattern(pieceCaptureDFA, input, startPos, ChessTokenType::PIECE_CAPTURE, candidates);   
        testDFAPattern(promotionDFA, input, startPos, ChessTokenType::PROMOTION, candidates);
        testDFAPattern(twinPieceMoveDFA, input, startPos, ChessTokenType::TWIN_PIECE_MOVE, candidates);
        testDFAPattern(twinPieceCaptureDFA, input, startPos, ChessTokenType::TWIN_PIECE_CAPTURE, candidates);
        testDFAPattern(promotionViaCaptureDFA, input, startPos, ChessTokenType::PROMOTION_VIA_CAPTURE, candidates);
        testDFAPattern(checkmateDFA, input, startPos, ChessTokenType::CHECKMATE, candidates);
        testDFAPattern(checkDFA, input, startPos, ChessTokenType::CHECK, candidates);

        if (!candidates.empty()) {
            auto longest = candidates[0];
            for (const auto& candidate : candidates) {
                if (candidate.second.length() > longest.second.length()) {
                    longest = candidate;
                }
            }
            return ChessToken(longest.first, longest.second, startPos);
        }
        return ChessToken(ChessTokenType::INVALID, "", startPos);
    }

    void testDFAPattern(shared_ptr<DFAState> dfa, const string& input, int startPos, 
                         ChessTokenType type, vector<pair<ChessTokenType, string>>& candidates) {
        string matchedValue;
        if (tryMatchDFA(dfa, input, startPos, matchedValue)) {
            candidates.push_back({type, matchedValue});
        }
    }

    bool tryMatchDFA(shared_ptr<DFAState> dfa, const string& input, int startPos, string& matchedValue) {
        auto currentState = dfa;
        int pos = startPos;
        int lastAcceptPos = -1;
        string currentMatch;

        while (pos < input.length()) {
            char c = input[pos];
            if (!currentState->transitions.count(c)) {
                break;
            }

            currentState = currentState->transitions[input[pos]];
            pos++;
            currentMatch = input.substr(startPos, pos - startPos);

            if (currentState->isFinal) {
                lastAcceptPos = pos;
                matchedValue = currentMatch;
            }
        }
        return lastAcceptPos != -1;
    }

    void displayTokens(const vector<ChessToken>& tokens) {
        cout << "\n=== CHESS TOKEN STREAM (PART 5: Lexer Output) ===\n";
        cout << "Position | Type          | Value\n";
        cout << "---------|---------------|-------\n";
        
        for (const auto& token : tokens) {
            if (token.type == ChessTokenType::END_OF_INPUT) {
                cout << " " << token.position << "     | END_OF_INPUT  | \n";
                continue;
            }

            string typeStr;
            switch (token.type) {
                case ChessTokenType::CASTLING:    typeStr = "CASTLING   "; break;
                case ChessTokenType::MOVE_NUMBER: typeStr = "MOVE_NUMBER"; break;
                case ChessTokenType::PAWN_MOVE:   typeStr = "PAWN_MOVE  "; break;
                case ChessTokenType::PIECE_MOVE:  typeStr = "PIECE_MOVE "; break;
                case ChessTokenType::TWIN_PIECE_MOVE:typeStr = "TWIN_PIECE_MOVE"; break;
                case ChessTokenType::PAWN_CAPTURE:typeStr = "PAWN_CAPTURE"; break;
                case ChessTokenType::PIECE_CAPTURE:typeStr = "PIECE_CAPTURE"; break;
                case ChessTokenType::TWIN_PIECE_CAPTURE:typeStr = "TWIN_PIECE_CAPTURE"; break;
                case ChessTokenType::PROMOTION:   typeStr = "PROMOTION  "; break;
                case ChessTokenType::PROMOTION_VIA_CAPTURE:typeStr = "PROMOTION_VIA_CAPTURE"; break;
                case ChessTokenType::CHECK:       typeStr = "CHECK      "; break;
                case ChessTokenType::CHECKMATE:   typeStr = "CHECKMATE  "; break;
                case ChessTokenType::RESULT:      typeStr = "RESULT     "; break;
                case ChessTokenType::END_OF_INPUT:typeStr = "END_OF_INPUT"; break;
                case ChessTokenType::INVALID:     typeStr = "INVALID    "; break;
            }
            cout << " " << token.position << "     | " << typeStr << " | " << token.value << "\n";
        }
    }
};

// ========== CHESS SYNTAX VALIDATOR ==========
class ChessSyntaxValidator {
public:
    bool validateMoveSyntax(const vector<ChessToken>& tokens) {
        cout << "\n=== SYNTAX VALIDATION ===\n";
        bool hasErrors = false;
        
        for (size_t i = 0; i < tokens.size() - 1; ++i) {
            const auto& current = tokens[i];
            const auto& next = tokens[i+1];
            
            if (isPrimaryMove(current) && isPrimaryMove(next)) {
                if (current.position + current.value.length() == next.position) {
                    cout << "SYNTAX ERROR: Primary move tokens found **physically touching** in input: '" 
                         << current.value << "' at pos " << current.position << " followed by '" 
                         << next.value << "' at pos " << next.position << ". Tokens must be separated by space/number.\n";
                    hasErrors = true;
                    break;
                }
            }
        }
        
        if (!hasErrors) {
            cout << "Token stream structure appears lexically valid.\n";
        }
        return !hasErrors;
    }

private:
    bool isPrimaryMove(const ChessToken& token) {
        return token.type == ChessTokenType::MOVE_NUMBER ||
               token.type == ChessTokenType::PAWN_MOVE ||
               token.type == ChessTokenType::PIECE_MOVE ||
               token.type == ChessTokenType::TWIN_PIECE_MOVE ||
               token.type == ChessTokenType::PAWN_CAPTURE || 
               token.type == ChessTokenType::PIECE_CAPTURE ||
               token.type == ChessTokenType::TWIN_PIECE_CAPTURE || 
               token.type == ChessTokenType::CASTLING || 
               token.type == ChessTokenType::PROMOTION ||
               token.type == ChessTokenType::PROMOTION_VIA_CAPTURE || 
               token.type == ChessTokenType::CHECK ||
               token.type == ChessTokenType::CHECKMATE ||
               token.type == ChessTokenType::RESULT;
    }
};

// ========== CHESS PUSHDOWN AUTOMATA ==========
class ChessPDA {
private:
    int expectedMoveNumber;
    enum class MoveState { EXPECT_NUMBER, EXPECT_WHITE_MOVE, EXPECT_BLACK_MOVE, GAME_OVER };
    MoveState currentState;

    bool isMoveToken(ChessTokenType type) const {
        return type == ChessTokenType::PAWN_MOVE ||
               type == ChessTokenType::PIECE_MOVE ||
               type == ChessTokenType::TWIN_PIECE_MOVE ||
               type == ChessTokenType::PAWN_CAPTURE ||
               type == ChessTokenType::PIECE_CAPTURE ||
               type == ChessTokenType::TWIN_PIECE_CAPTURE ||
               type == ChessTokenType::CASTLING ||
               type == ChessTokenType::PROMOTION ||
               type == ChessTokenType::PROMOTION_VIA_CAPTURE ||
               type == ChessTokenType::CHECK ||
               type == ChessTokenType::CHECKMATE;
    }

public:
    ChessPDA() {
        resetPDA();
    }
    
    bool validateMoveSequence(const vector<ChessToken>& tokens) {
        cout << "\n=== PDA VALIDATION ===\n";
        resetPDA();
        
        for (size_t i = 0; i + 1 < tokens.size(); ++i) {
            const auto& token = tokens[i];
            
            if (currentState == MoveState::GAME_OVER) {
                if (token.type == ChessTokenType::END_OF_INPUT) {
                    break; 
                }
                cout << "SEQUENCE ERROR: Tokens found after game termination (" << token.value << ").\n";
                return false;
            }

            if (token.type == ChessTokenType::END_OF_INPUT) { 
                if (currentState == MoveState::EXPECT_WHITE_MOVE) {
                    cout << "SEQUENCE ERROR: Game ended abruptly. Expected White's move for turn " << expectedMoveNumber << ".\n";
                    return false;
                }
                if (currentState == MoveState::EXPECT_BLACK_MOVE) {
                    cout << "SEQUENCE WARNING: Game ended after White's move in turn " << expectedMoveNumber 
                         << ". Black's move is missing (Half-move).\n";
                }
                break; 
            }
            
            if (token.type == ChessTokenType::RESULT) {
                currentState = MoveState::GAME_OVER; 
                continue;
            }
            
            if (token.type == ChessTokenType::MOVE_NUMBER) {
                if (currentState != MoveState::EXPECT_NUMBER) {
                    cout << "SEQUENCE ERROR: Found MOVE_NUMBER (" << token.value << ") but expected a move or result.\n";
                    return false;
                }
                
                string numberStr = token.value.substr(0, token.value.length() - 1);
                int moveNumber = 0;
                try { moveNumber = stoi(numberStr); } catch (...) { return false; } 

                if (moveNumber != expectedMoveNumber) {
                    cout << "SEQUENCE ERROR: Expected move number " << expectedMoveNumber 
                         << " but found " << moveNumber << ".\n";
                    return false;
                }
                
                expectedMoveNumber++;
                currentState = MoveState::EXPECT_WHITE_MOVE;
                continue; 
            }
            
            if (isMoveToken(token.type)) {
                if (token.type == ChessTokenType::CHECKMATE) {
                    if (i + 1 < tokens.size() && tokens[i+1].type == ChessTokenType::RESULT) {
                    } else {
                        cout << "SEQUENCE ERROR: Checkmate (" << token.value 
                             << ") must be followed immediately by a game RESULT (e.g., 1-0 or 0-1).\n";
                        return false;
                    }
                }
                
                if (currentState == MoveState::EXPECT_WHITE_MOVE) {
                    currentState = MoveState::EXPECT_BLACK_MOVE;
                } else if (currentState == MoveState::EXPECT_BLACK_MOVE) {
                    currentState = MoveState::EXPECT_NUMBER;
                } else {
                    cout << "SEQUENCE ERROR: Found an unexpected move (" << token.value 
                         << ") when expecting move number or result.\n";
                    return false;
                }
                continue;
            }
        }
        cout << "PGN sequence successfully parsed.\n";
        return true;
    }

private:
    void resetPDA() {
        expectedMoveNumber = 1;
        currentState = MoveState::EXPECT_NUMBER;
    }
};

// ========== INTERACTIVE CHESS PARSER SIMULATOR ==========
class ChessParserSimulator {
private:
    ChessLexer lexer;
    ChessSyntaxValidator  syntaxValidator;
    ChessPDA  pda;

public:
    void processInput(const string& input) {
        cout << "\nPROCESSING: \n\"" << input << "\"\n";
        cout << "\n";

        auto [tokens, hadLexicalError] = lexer.tokenize(input);
        lexer.displayTokens(tokens);
        bool syntaxValid = syntaxValidator.validateMoveSyntax(tokens);
        bool pdaValid = pda.validateMoveSequence(tokens);
        
        cout << "\n--- DIAGNOSTIC CHECK ---\n";
        cout << "Lexical Errors Found: " << (hadLexicalError ? "TRUE" : "FALSE") << "\n";
        cout << "Syntax Valid: " << (syntaxValid ? "TRUE" : "FALSE") << "\n";
        cout << "PDA Valid: " << (pdaValid ? "TRUE" : "FALSE") << "\n";
        
        bool overallValid = syntaxValid && pdaValid && !hadLexicalError;
        
        cout << "\nRESULT: " << (overallValid ? "VALID (Lexical/Syntax/Sequence)" : "INVALID (Lexical/Syntax/Sequence)") << "\n";
        if (hadLexicalError) cout << "   - Lexical errors (unrecognized characters) found.\n";
        if (!syntaxValid) cout << "   - Syntax (token structure) errors found.\n";
        if (!pdaValid) cout << "   - PDA (sequence/turn order) errors found.\n";
        cout << string(50, '=') << "\n";
    }
};

int main() {
    ChessParserSimulator simulator;
    
    string input;
    cout << "=== CHESS PGN ANALYZER SIMULATOR ===\n";
    cout << "Enter chess notation (or 'quit' to exit):\n> ";
    getline(cin, input);

    if (input == "quit" || input == "exit" || input == "q") {
        return 0;
    }

    simulator.processInput(input);
    
    return 0;
}