#include <raylib.h>
#include <string>
#include <map>
#include <cmath> 
#include <iostream> 
#include <vector>

using namespace std;

const int BOARD_SIZE = 8;
const int TILE_SIZE = 80;
const int WINDOW_WIDTH = BOARD_SIZE * TILE_SIZE;
const int WINDOW_HEIGHT = WINDOW_WIDTH + 90;
const int PIECE_SIZE = 100;

enum GameStatus { PLAYING, CHECK, CHECKMATE, STALEMATE };

const char InitialBoard[BOARD_SIZE][BOARD_SIZE] = {
    {'r', 'n', 'b', 'q', 'k', 'b', 'n', 'r'}, 
    {'p', 'p', 'p', 'p', 'p', 'p', 'p', 'p'}, 
    {0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0},
    {'P', 'P', 'P', 'P', 'P', 'P', 'P', 'P'}, 
    {'R', 'N', 'B', 'Q', 'K', 'B', 'N', 'R'}  
};

char GameBoard[BOARD_SIZE][BOARD_SIZE];
char currentTurn = 'W'; 
bool castlingRights[4] = {true, true, true, true};
Vector2 enPassantTarget = {-1, -1}; 
Vector2 selectedTile = {-1, -1}; 
GameStatus gameStatus = PLAYING; 

bool isPromoting = false;
Vector2 promotionSquare = {-1, -1};
vector<Vector2> legalMoves;

struct MoveState {
    char board[BOARD_SIZE][BOARD_SIZE];
    char turn;
    bool castling[4];
    Vector2 enPassant;
    GameStatus status;
};

vector<MoveState> history;
int historyIndex = -1;

map<char, int> PieceMap = {
    {'K', 0}, {'Q', 1}, {'B', 2}, {'N', 3}, {'R', 4}, {'P', 5},
    {'k', 6}, {'q', 7}, {'b', 8}, {'n', 9}, {'r', 10}, {'p', 11}
};

void saveCurrentState();
void loadStateFromHistory(int index);
bool isPathClear(int sr, int sc, int er, int ec, char board[BOARD_SIZE][BOARD_SIZE]);
bool canPieceMoveTo(int sr, int sc, int er, int ec, char piece, char board[BOARD_SIZE][BOARD_SIZE], Vector2 epTarget, bool castling[4], bool forCheckValidation);
Vector2 findKing(char playerColor, char board[BOARD_SIZE][BOARD_SIZE]);
bool isKingInCheck(char playerColor, char board[BOARD_SIZE][BOARD_SIZE]);
bool isValidMove(int sr, int sc, int er, int ec);
bool hasValidMoves(char playerColor);
void updateGameStatusAndSaveHistory();
void applyMoveAndCheckStatus(int startRow, int startCol, int endRow, int endCol, char pieceToMove);
void handlePromotionChoice(char pieceType);
void calculateLegalMoves(int sr, int sc);

void loadStateFromHistory(int index) {
    if (index >= 0 && index < (int)history.size()) {
        historyIndex = index;
        const MoveState& state = history[index];
        
        for (int i = 0; i < BOARD_SIZE; ++i) {
            for (int j = 0; j < BOARD_SIZE; ++j) {
                GameBoard[i][j] = state.board[i][j];
            }
        }
        currentTurn = state.turn;
        for (int i = 0; i < 4; ++i) castlingRights[i] = state.castling[i];
        enPassantTarget = state.enPassant;
        gameStatus = state.status;
        
        selectedTile = {-1, -1};
        isPromoting = false;
        promotionSquare = {-1, -1};
        legalMoves.clear();
        cout << "History loaded to index " << historyIndex << ". " << (currentTurn == 'W' ? "White's" : "Black's") << " turn restored.\n";
    }
}


void saveCurrentState() {
    if (historyIndex < (int)history.size() - 1) {
        history.erase(history.begin() + historyIndex + 1, history.end());
    }
    
    MoveState newState;
    for (int i = 0; i < BOARD_SIZE; ++i) {
        for (int j = 0; j < BOARD_SIZE; ++j) {
            newState.board[i][j] = GameBoard[i][j];
        }
    }
    newState.turn = currentTurn;
    for (int i = 0; i < 4; ++i) newState.castling[i] = castlingRights[i];
    newState.enPassant = enPassantTarget;
    newState.status = gameStatus;

    history.push_back(newState);
    historyIndex = (int)history.size() - 1;
}

void resetGame() {
    for (int i = 0; i < BOARD_SIZE; ++i) {
        for (int j = 0; j < BOARD_SIZE; ++j) {
            GameBoard[i][j] = InitialBoard[i][j];
        }
    }
    
    currentTurn = 'W';
    castlingRights[0] = castlingRights[1] = castlingRights[2] = castlingRights[3] = true;
    enPassantTarget = {-1, -1};
    selectedTile = {-1, -1};
    gameStatus = PLAYING;
    isPromoting = false; 
    promotionSquare = {-1, -1};
    legalMoves.clear();
    
    history.clear();
    historyIndex = -1;
    saveCurrentState(); 

    cout << "\n--- Game Reset to Initial Position ---\n";
}


bool isPathClear(int sr, int sc, int er, int ec, char board[BOARD_SIZE][BOARD_SIZE]) {
    int dRow = er - sr;
    int dCol = ec - sc;
    int rowStep = (dRow > 0) ? 1 : (dRow < 0 ? -1 : 0);
    int colStep = (dCol > 0) ? 1 : (dCol < 0 ? -1 : 0);
    int currentRow = sr + rowStep;
    int currentCol = sc + colStep;

    while (currentRow != er || currentCol != ec) {
        if (board[currentRow][currentCol] != 0) return false;
        currentRow += rowStep;
        currentCol += colStep;
    }
    return true;
}


bool canPieceMoveTo(int sr, int sc, int er, int ec, char piece, char board[BOARD_SIZE][BOARD_SIZE], Vector2 epTarget, bool castling[4], bool forCheckValidation) {
    char target = board[er][ec];
    if (!forCheckValidation && target != 0 && isupper(piece) == isupper(target)) return false; 
    if (sr == er && sc == ec) return false;

    int dRow = er - sr;
    int dCol = ec - sc;
    int absDRow = abs(dRow);
    int absDCol = abs(dCol);
    char type = (char)toupper(piece);

    switch (type) {
        case 'P':
        {
            int direction = isupper(piece) ? -1 : 1;
            if (absDCol == 1 && dRow == direction) {
                if (target != 0) return true;
                if (!forCheckValidation && er == epTarget.y && ec == epTarget.x) return true;
            }
            if (dCol == 0 && target == 0) {
                if (dRow == direction) return true;
                if (!forCheckValidation) {
                    int startRank = isupper(piece) ? 6 : 1;
                    if (sr == startRank && dRow == 2 * direction) {
                        return board[sr + direction][sc] == 0;
                    }
                }
            }
            return false;
        }
        case 'R': if (dRow != 0 && dCol != 0) return false; return isPathClear(sr, sc, er, ec, board);
        case 'B': if (absDRow != absDCol) return false; return isPathClear(sr, sc, er, ec, board);
        case 'N': return (absDRow == 1 && absDCol == 2) || (absDRow == 2 && absDCol == 1);
        case 'Q': {
            bool isStraight = (dRow == 0 || dCol == 0);
            bool isDiagonal = (absDRow == absDCol);
            if (isStraight || isDiagonal) return isPathClear(sr, sc, er, ec, board);
            return false;
        }
        case 'K': {
            if (absDRow <= 1 && absDCol <= 1) return true;
            if (!forCheckValidation && absDCol == 2 && dRow == 0) {
                int rank = isupper(piece) ? 7 : 0;
                bool isKingSide = dCol > 0;
                int rightIndex = (isupper(piece) ? 0 : 2) + (isKingSide ? 0 : 1);
                if (sr != rank || !castling[rightIndex]) return false;
                int pathCol1 = sc + (isKingSide ? 1 : -1);
                int pathCol2 = sc + (isKingSide ? 2 : -2);
                if (board[rank][pathCol1] != 0 || board[rank][pathCol2] != 0) return false;
                if (!isKingSide && board[rank][1] != 0) return false;
                if (isKingInCheck(isupper(piece) ? 'W' : 'B', board)) return false;
                return true;
            }
            return false;
        }
        default: return false;
    }
}


Vector2 findKing(char playerColor, char board[BOARD_SIZE][BOARD_SIZE]) {
    char kingPiece = (playerColor == 'W') ? 'K' : 'k';
    for (int r = 0; r < BOARD_SIZE; ++r) {
        for (int c = 0; c < BOARD_SIZE; ++c) {
            if (board[r][c] == kingPiece) return {(float)c, (float)r};
        }
    }
    return {-1, -1};
}


bool isKingInCheck(char playerColor, char board[BOARD_SIZE][BOARD_SIZE]) {
    Vector2 kingPos = findKing(playerColor, board);
    char enemyColor = (playerColor == 'W') ? 'B' : 'W';

    for (int r = 0; r < BOARD_SIZE; ++r) {
        for (int c = 0; c < BOARD_SIZE; ++c) {
            char piece = board[r][c];
            if (piece != 0 && (isupper(piece) ? 'W' : 'B') == enemyColor) {
                if (canPieceMoveTo(r, c, (int)kingPos.y, (int)kingPos.x, piece, board, enPassantTarget, castlingRights, true)) {
                    return true;
                }
            }
        }
    }
    return false;
}


bool isValidMove(int sr, int sc, int er, int ec) {
    if (historyIndex < (int)history.size() - 1 || gameStatus == CHECKMATE || gameStatus == STALEMATE || isPromoting) return false;
    
    char pieceToMove = GameBoard[sr][sc];
    char targetPiece = GameBoard[er][ec];
    
    if (!canPieceMoveTo(sr, sc, er, ec, pieceToMove, GameBoard, enPassantTarget, castlingRights, false)) return false;

    char tempBoard[BOARD_SIZE][BOARD_SIZE];
    for (int i = 0; i < BOARD_SIZE; ++i) for (int j = 0; j < BOARD_SIZE; ++j) tempBoard[i][j] = GameBoard[i][j];
    
    tempBoard[er][ec] = pieceToMove;
    tempBoard[sr][sc] = 0;

    if (toupper(pieceToMove) == 'P' && targetPiece == 0 && abs(ec - sc) == 1 && er == enPassantTarget.y && ec == enPassantTarget.x) {
        tempBoard[sr][ec] = 0; 
    }
    
    if (toupper(pieceToMove) == 'K' && abs(ec - sc) == 2) {
        int rookStartCol = (ec > sc) ? 7 : 0;
        int rookEndCol = (ec > sc) ? 5 : 3;
        char rook = GameBoard[er][rookStartCol];
        tempBoard[er][rookEndCol] = rook;
        tempBoard[er][rookStartCol] = 0;
    }

    char playerColor = (isupper(pieceToMove) ? 'W' : 'B');
    if (isKingInCheck(playerColor, tempBoard)) return false;
    return true;
}


void calculateLegalMoves(int sr, int sc) {
    legalMoves.clear();
    for (int er = 0; er < BOARD_SIZE; ++er) {
        for (int ec = 0; ec < BOARD_SIZE; ++ec) {
            if (isValidMove(sr, sc, er, ec)) {
                legalMoves.push_back({(float)ec, (float)er});
            }
        }
    }
}


bool hasValidMoves(char playerColor) {
    for (int sr = 0; sr < BOARD_SIZE; ++sr) {
        for (int sc = 0; sc < BOARD_SIZE; ++sc) {
            char piece = GameBoard[sr][sc];
            if (piece != 0 && (isupper(piece) ? 'W' : 'B') == playerColor) {
                for (int er = 0; er < BOARD_SIZE; ++er) {
                    for (int ec = 0; ec < BOARD_SIZE; ++ec) {
                        
                        char pieceToMove = GameBoard[sr][sc];
                        char targetPiece = GameBoard[er][ec];
                        
                        if (!canPieceMoveTo(sr, sc, er, ec, pieceToMove, GameBoard, enPassantTarget, castlingRights, false)) continue;
                        
                        char tempBoard[BOARD_SIZE][BOARD_SIZE];
                        for (int i = 0; i < BOARD_SIZE; ++i) for (int j = 0; j < BOARD_SIZE; ++j) tempBoard[i][j] = GameBoard[i][j];
                        
                        tempBoard[er][ec] = pieceToMove;
                        tempBoard[sr][sc] = 0;
                        
                        if (toupper(pieceToMove) == 'P' && targetPiece == 0 && abs(ec - sc) == 1 && er == enPassantTarget.y && ec == enPassantTarget.x) {
                            tempBoard[sr][ec] = 0; 
                        }
                        
                        if (toupper(pieceToMove) == 'K' && abs(ec - sc) == 2) {
                            int rookStartCol = (ec > sc) ? 7 : 0;
                            int rookEndCol = (ec > sc) ? 5 : 3;
                            char rook = GameBoard[er][rookStartCol]; 
                            tempBoard[er][rookEndCol] = rook;
                            tempBoard[er][rookStartCol] = 0;
                        }

                        if (!isKingInCheck(playerColor, tempBoard)) {
                             return true;
                        }
                    }
                }
            }
        }
    }
    return false;
}


void updateGameStatusAndSaveHistory() {
    if (!hasValidMoves(currentTurn)) {
        if (isKingInCheck(currentTurn, GameBoard)) {
            gameStatus = CHECKMATE;
        } else {
            gameStatus = STALEMATE;
        }
    } else if (isKingInCheck(currentTurn, GameBoard)) {
        gameStatus = CHECK;
    } else {
        gameStatus = PLAYING;
    }
    
    saveCurrentState();
}


void applyMoveAndCheckStatus(int startRow, int startCol, int endRow, int endCol, char pieceToMove) {
    char type = (char)toupper(pieceToMove);

    if (type == 'K') {
        if (currentTurn == 'W') { castlingRights[0] = false; castlingRights[1] = false; }
        else { castlingRights[2] = false; castlingRights[3] = false; }
    } else if (type == 'R') {
        if (currentTurn == 'W') { 
            if (startCol == 0) castlingRights[1] = false; 
            if (startCol == 7) castlingRights[0] = false; 
        } else {
            if (startCol == 0) castlingRights[3] = false; 
            if (startCol == 7) castlingRights[2] = false; 
        }
    }
    
    enPassantTarget = {-1, -1};
    if (type == 'P' && abs(endRow - startRow) == 2) {
        enPassantTarget = {(float)endCol, (float)(endRow + (currentTurn == 'W' ? 1 : -1))};
    }
    
    currentTurn = (currentTurn == 'W') ? 'B' : 'W';
    updateGameStatusAndSaveHistory();
    selectedTile = {-1, -1};
}


void handlePromotionChoice(char pieceType) {
    if (!isPromoting) return;

    int col = (int)promotionSquare.x;
    int row = (int)promotionSquare.y;
    
    char finalPiece = (currentTurn == 'W') ? pieceType : tolower(pieceType);
    GameBoard[row][col] = finalPiece;
    
    enPassantTarget = {-1, -1};
    currentTurn = (currentTurn == 'W') ? 'B' : 'W'; 

    updateGameStatusAndSaveHistory();
    
    isPromoting = false;
    promotionSquare = {-1, -1};
    cout << "Pawn promoted to " << finalPiece << ". New Status: " << gameStatus << "\n";
}


void drawBoard(Texture2D pieceTexture) {
    const Color LIGHT_TILE = {240, 217, 181, 255}; 
    const Color DARK_TILE = {181, 136, 99, 255};  
    const Color HIGHLIGHT_COLOR = {100, 255, 100, 150}; 
    const Color CHECK_COLOR = {255, 0, 0, 150}; 
    const Color TURN_COLOR_W = {0, 121, 241, 255};
    const Color TURN_COLOR_B = {230, 41, 55, 255};
    const Color LEGAL_MOVE_DOT = {100, 100, 100, 100};

    bool isKingChecked = isKingInCheck(currentTurn, GameBoard);

    for (int row = 0; row < BOARD_SIZE; ++row) {
        for (int col = 0; col < BOARD_SIZE; ++col) {
            int x = col * TILE_SIZE;
            int y = row * TILE_SIZE;

            bool isLight = (row + col) % 2 == 0;
            DrawRectangle(x, y, TILE_SIZE, TILE_SIZE, isLight ? LIGHT_TILE : DARK_TILE);
            
            if (col == selectedTile.x && row == selectedTile.y) {
                DrawRectangle(x, y, TILE_SIZE, TILE_SIZE, HIGHLIGHT_COLOR);
            }

            Vector2 kingPos = findKing(currentTurn, GameBoard);
            if (isKingChecked && col == kingPos.x && row == kingPos.y) {
                 DrawRectangle(x, y, TILE_SIZE, TILE_SIZE, CHECK_COLOR);
            }
            
            for (const auto& move : legalMoves) {
                if (col == move.x && row == move.y) {
                    float centerX = (float)x + TILE_SIZE / 2.0f;
                    float centerY = (float)y + TILE_SIZE / 2.0f;
                    
                    char targetPiece = GameBoard[row][col];
                    
                    if (targetPiece != 0) {
                        DrawCircleLines(centerX, centerY, TILE_SIZE / 2.0f - 5.0f, LEGAL_MOVE_DOT);
                    } else {
                        DrawCircle(centerX, centerY, TILE_SIZE * 0.15f, LEGAL_MOVE_DOT);
                    }
                }
            }

            if (col == enPassantTarget.x && row == enPassantTarget.y) {
                 DrawRectangleLines(x, y, TILE_SIZE, TILE_SIZE, YELLOW);
            }

            char pieceCode = GameBoard[row][col];
            if (pieceCode != 0) {
                int pieceIndex = PieceMap[pieceCode];
                Rectangle sourceRec = { (float)pieceIndex * PIECE_SIZE, 0.0f, (float)PIECE_SIZE, (float)PIECE_SIZE };
                Rectangle destRec = { (float)x, (float)y, (float)TILE_SIZE, (float)TILE_SIZE };
                DrawTexturePro(pieceTexture, sourceRec, destRec, {0, 0}, 0.0f, WHITE);
            }
        }
    }
    
    Color turnColor = currentTurn == 'W' ? TURN_COLOR_W : TURN_COLOR_B;
    const char* turnText = currentTurn == 'W' ? "WHITE's Turn" : "BLACK's Turn";
    
    if (gameStatus == CHECKMATE) {
        turnColor = LIME;
        turnText = currentTurn == 'W' ? "CHECKMATE! BLACK WINS!" : "CHECKMATE! WHITE WINS!";
    } else if (gameStatus == STALEMATE) {
        turnColor = GRAY;
        turnText = "STALEMATE! DRAW!";
    } else if (gameStatus == CHECK) {
        turnColor = RED;
        turnText = TextFormat("%s (IN CHECK!)", turnText);
    }
    
    DrawText(turnText, 10, WINDOW_WIDTH + 10, 20, turnColor);
    
    if (historyIndex < (int)history.size() - 1) {
         DrawText("[HISTORY VIEW]", WINDOW_WIDTH - 150, WINDOW_WIDTH + 10, 20, ORANGE);
    }
}


void drawPromotionMenu(Texture2D pieceTexture) {
    if (!isPromoting) return;
    
    int col = (int)promotionSquare.x;
    int menuWidth = TILE_SIZE;
    
    DrawRectangle(0, 0, WINDOW_WIDTH, WINDOW_WIDTH, {0, 0, 0, 180});
    DrawRectangle(col * TILE_SIZE - 2, 0 - 2, menuWidth + 4, WINDOW_WIDTH + 4, GOLD);

    char options[4] = {'Q', 'R', 'B', 'N'};
    char pieceColor = currentTurn;

    for (int i = 0; i < 4; ++i) {
        char pieceCode = (pieceColor == 'W') ? options[i] : tolower(options[i]);
        int drawY = (pieceColor == 'W') ? (i * TILE_SIZE) : ((7 - i) * TILE_SIZE);

        Rectangle tileRect = { (float)(col * TILE_SIZE), (float)drawY, (float)TILE_SIZE, (float)TILE_SIZE };
        
        if (CheckCollisionPointRec(GetMousePosition(), tileRect)) {
            DrawRectangleRec(tileRect, {150, 150, 255, 200}); 
        } else {
            bool isLight = ((drawY/TILE_SIZE) + col) % 2 == 0; 
            DrawRectangleRec(tileRect, isLight ? LIGHTGRAY : DARKGRAY);
        }

        int pieceIndex = PieceMap[pieceCode];
        Rectangle sourceRec = { (float)pieceIndex * PIECE_SIZE, 0.0f, (float)PIECE_SIZE, (float)PIECE_SIZE };
        Rectangle destRec = { (float)(col * TILE_SIZE), (float)drawY, (float)TILE_SIZE, (float)TILE_SIZE };
        DrawTexturePro(pieceTexture, sourceRec, destRec, {0, 0}, 0.0f, WHITE);
    }
}


int main() {
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Raylib Chess Engine - Complete");
    SetTargetFPS(60);
    resetGame(); 

    Texture2D pieceTexture = LoadTexture("chess_pieces.png");

    if (pieceTexture.id == 0) {
        cout << "WARNING: Failed to load 'chess_pieces.png'. Using placeholder.\n";
        Image placeholder = GenImageColor(PIECE_SIZE * 12, PIECE_SIZE, LIME); 
        pieceTexture = LoadTextureFromImage(placeholder);
        UnloadImage(placeholder); 
    }
    
    Rectangle undoButton = { 50.0f, WINDOW_WIDTH + 45, 80, 40 };
    Rectangle redoButton = { WINDOW_WIDTH - 130.0f, WINDOW_WIDTH + 45, 80, 40 };
    Rectangle resetButton = { (float)WINDOW_WIDTH/2 - 100, (float)WINDOW_WIDTH + 45, 200, 40 };

    while (!WindowShouldClose()) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 mousePos = GetMousePosition();

            if (isPromoting) {
                int col = (int)promotionSquare.x;
                char options[4] = {'Q', 'R', 'B', 'N'};
                char pieceColor = currentTurn;

                for (int i = 0; i < 4; ++i) {
                    int drawY = (pieceColor == 'W') ? (i * TILE_SIZE) : ((7 - i) * TILE_SIZE);
                    Rectangle tileRect = { (float)(col * TILE_SIZE), (float)drawY, (float)TILE_SIZE, (float)TILE_SIZE };
                    
                    if (CheckCollisionPointRec(mousePos, tileRect)) {
                        handlePromotionChoice(options[i]);
                        legalMoves.clear();
                        break;
                    }
                }
                continue; 
            }
            
            if (CheckCollisionPointRec(mousePos, resetButton)) {
                resetGame();
            } else if (CheckCollisionPointRec(mousePos, undoButton) && historyIndex > 0) {
                loadStateFromHistory(historyIndex - 1);
            } else if (CheckCollisionPointRec(mousePos, redoButton) && historyIndex < (int)history.size() - 1) {
                loadStateFromHistory(historyIndex + 1);
            } else if (mousePos.y < WINDOW_WIDTH && gameStatus != CHECKMATE && gameStatus != STALEMATE) {
                int col = (int)(mousePos.x / TILE_SIZE);
                int row = (int)(mousePos.y / TILE_SIZE);
                if (col >= 0 && col < BOARD_SIZE && row >= 0 && row < BOARD_SIZE) {
                    if (selectedTile.x == -1) {
                        char piece = GameBoard[row][col];
                        if (piece != 0 && (isupper(piece) ? 'W' : 'B') == currentTurn) { 
                            selectedTile = {(float)col, (float)row};
                            calculateLegalMoves(row, col);
                        }
                    } else {
                        int startCol = (int)selectedTile.x;
                        int startRow = (int)selectedTile.y;
                        char pieceToMove = GameBoard[startRow][startCol];
                        char type = (char)toupper(pieceToMove);
                        bool moveExecuted = false;

                        if (startCol == col && startRow == row) {
                            selectedTile = {-1, -1};
                            legalMoves.clear();
                        } else if (isValidMove(startRow, startCol, row, col)) {
                            if (type == 'K' && abs(col - startCol) == 2) {
                                int rookStartCol = (col > startCol) ? 7 : 0;
                                int rookEndCol = (col > startCol) ? 5 : 3;
                                GameBoard[row][col] = pieceToMove;
                                GameBoard[startRow][startCol] = 0; 
                                GameBoard[row][rookEndCol] = GameBoard[row][rookStartCol];
                                GameBoard[row][rookStartCol] = 0;
                                moveExecuted = true;
                            } else if (type == 'P' && GameBoard[row][col] == 0 && abs(col - startCol) == 1 && row == enPassantTarget.y && col == enPassantTarget.x) {
                                GameBoard[startRow][col] = 0;
                                GameBoard[row][col] = pieceToMove;
                                GameBoard[startRow][startCol] = 0;
                                moveExecuted = true;
                            } else {
                                GameBoard[row][col] = pieceToMove;
                                GameBoard[startRow][startCol] = 0; 
                                moveExecuted = true;
                            }

                            if (moveExecuted) {
                                legalMoves.clear();
                                if (type == 'P' && (row == 0 || row == 7)) {
                                    isPromoting = true;
                                    promotionSquare = {(float)col, (float)row};
                                    selectedTile = {-1, -1};
                                } else {
                                    applyMoveAndCheckStatus(startRow, startCol, row, col, pieceToMove);
                                }
                            }
                        } else {
                            char piece = GameBoard[row][col];
                            if (piece != 0 && (isupper(piece) ? 'W' : 'B') == currentTurn) { 
                                selectedTile = {(float)col, (float)row};
                                calculateLegalMoves(row, col);
                            } else {
                                selectedTile = {-1, -1};
                                legalMoves.clear();
                            }
                        }
                    }
                }
            }
        }
        
        BeginDrawing();
        ClearBackground(RAYWHITE); 
        drawBoard(pieceTexture);
        
        Color undoColor = historyIndex > 0 ? BLUE : LIGHTGRAY;
        DrawRectangleRec(undoButton, undoColor);
        DrawText("<--", undoButton.x + 28, undoButton.y + 10, 20, WHITE);

        Color resetColor = GRAY;
        DrawRectangleRec(resetButton, resetColor);
        DrawText("RESET BOARD", resetButton.x + 20, resetButton.y + 10, 20, WHITE);
        
        Color redoColor = historyIndex < (int)history.size() - 1 ? BLUE : LIGHTGRAY;
        DrawRectangleRec(redoButton, redoColor);
        DrawText("-->", redoButton.x + 28, redoButton.y + 10, 20, WHITE);

        drawPromotionMenu(pieceTexture);
        EndDrawing();
    }

    UnloadTexture(pieceTexture);
    CloseWindow();
    return 0;
}