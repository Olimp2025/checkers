#include <iostream>
#include <vector>
#include <string>
#include <limits>
#include <cstdlib>
#include <ctime>
#include <cctype>
#include <thread>
#include <future>
#include <chrono>

static constexpr int BOARD_SIZE = 8;

// -------------------- Структуры данных --------------------
struct MoveStep {
    int startRow, startCol;
    int endRow, endCol;
};

struct MoveSequence {
    std::vector<MoveStep> steps;
    int capturesCount = 0;
};

// Проверка валидности координат
bool onBoard(int r, int c) {
    return (r >= 0 && r < BOARD_SIZE && c >= 0 && c < BOARD_SIZE);
}

// pieceColor: 1 = белая, -1 = чёрная, 0 = нет фигуры
int pieceColor(char p) {
    if (p == 'w' || p == 'W') return 1;
    if (p == 'b' || p == 'B') return -1;
    return 0;
}

// Является ли дамкой
bool isKing(char p) {
    return (p == 'W' || p == 'B');
}

// Превращение в дамку при достижении конца
void promoteIfNeeded(std::vector<std::vector<char>>& board, int r, int c) {
    char &pc = board[r][c];
    if (pc == 'w' && r == 0) {
        pc = 'W';
    } else if (pc == 'b' && r == BOARD_SIZE - 1) {
        pc = 'B';
    }
}

// Инициализация доски
void initBoard(std::vector<std::vector<char>>& board) {
    board.assign(BOARD_SIZE, std::vector<char>(BOARD_SIZE, '.'));
    // Чёрные сверху (строки 0..2)
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < BOARD_SIZE; ++c) {
            if ((r + c) % 2 == 1) {
                board[r][c] = 'b';
            }
        }
    }
    // Белые снизу (строки 5..7)
    for (int r = 5; r < BOARD_SIZE; ++r) {
        for (int c = 0; c < BOARD_SIZE; ++c) {
            if ((r + c) % 2 == 1) {
                board[r][c] = 'w';
            }
        }
    }
}

// Печать доски (с учётом стороны пользователя)
void printBoard(const std::vector<std::vector<char>>& board, bool userIsWhite) {
    std::cout << "   | A B C D E F G H\n";
    std::cout << "   -----------------\n";

    if (userIsWhite) {
        for (int r = 0; r < BOARD_SIZE; ++r) {
            std::cout << std::format(" {:2d} | ", r + 1);
            for (int c = 0; c < BOARD_SIZE; ++c) {
                std::cout << board[r][c];
                if (c < BOARD_SIZE - 1) std::cout << " ";
            }
            std::cout << "\n";
        }
    } else {
        // Перевёрнуто
        for (int r = BOARD_SIZE - 1; r >= 0; --r) {
            std::cout << std::format(" {:2d} | ", BOARD_SIZE - r);
            for (int c = BOARD_SIZE - 1; c >= 0; --c) {
                std::cout << board[r][c];
                if (c > 0) std::cout << " ";
            }
            std::cout << "\n";
        }
    }
    std::cout << std::endl;
}

// Выполнить один шаг
bool makeOneStep(std::vector<std::vector<char>>& board, const MoveStep& step, bool isCapture) {
    char piece = board[step.startRow][step.startCol];
    board[step.startRow][step.startCol] = '.';
    board[step.endRow][step.endCol] = piece;

    if (isCapture) {
        int dirR = (step.endRow - step.startRow) > 0 ? 1 : -1;
        int dirC = (step.endCol - step.startCol) > 0 ? 1 : -1;

        int checkR = step.startRow + dirR;
        int checkC = step.startCol + dirC;
        while (checkR != step.endRow || checkC != step.endCol) {
            if (pieceColor(board[checkR][checkC]) != 0) {
                board[checkR][checkC] = '.';
                break;
            }
            checkR += dirR;
            checkC += dirC;
        }
    }
    promoteIfNeeded(board, step.endRow, step.endCol);
    return true;
}

// Выполнить всю последовательность (возможно, с множественным боем)
bool makeMoveSequence(std::vector<std::vector<char>>& board, const MoveSequence& seq) {
    if (seq.steps.empty()) return false;
    bool capture = (seq.capturesCount > 0);
    for (auto &st : seq.steps) {
        makeOneStep(board, st, capture);
    }
    return true;
}

// Обычные ходы для простой шашки
std::vector<MoveSequence> getManSimpleMoves(const std::vector<std::vector<char>>& board,
                                            int r, int c, int color)
{
    std::vector<MoveSequence> result;
    int dr = (color == 1) ? -1 : 1;
    for (int dc : {-1, 1}) {
        int nr = r + dr;
        int nc = c + dc;
        if (onBoard(nr, nc) && board[nr][nc] == '.') {
            MoveSequence seq;
            seq.steps.push_back({r, c, nr, nc});
            seq.capturesCount = 0;
            result.push_back(seq);
        }
    }
    return result;
}

// Обычные ходы для дамки
std::vector<MoveSequence> getKingSimpleMoves(const std::vector<std::vector<char>>& board,
                                             int r, int c)
{
    std::vector<MoveSequence> result;
    std::vector<std::pair<int,int>> directions = {{1,1},{1,-1},{-1,1},{-1,-1}};
    for (auto [dr,dc] : directions) {
        int nr = r + dr;
        int nc = c + dc;
        while (onBoard(nr, nc) && board[nr][nc] == '.') {
            MoveSequence seq;
            seq.steps.push_back({r, c, nr, nc});
            seq.capturesCount = 0;
            result.push_back(seq);

            nr += dr;
            nc += dc;
        }
    }
    return result;
}

// Рекурсивный поиск боёв
void searchCaptures(std::vector<std::vector<char>> board,
                    int r, int c,
                    int color,
                    std::vector<std::pair<int,int>> used,
                    MoveSequence currentSeq,
                    std::vector<MoveSequence> &allSeq)
{
    bool man = !isKing(board[r][c]);
    auto wasUsed = [&](int rr, int cc){
        for (auto &[ur,uc] : used) {
            if (ur == rr && uc == cc) return true;
        }
        return false;
    };

    std::vector<std::pair<int,int>> directions = {{1,1},{1,-1},{-1,1},{-1,-1}};
    bool foundFurther = false;

    if (man) {
        // Простая: ±2
        for (auto &[dr,dc] : directions) {
            int midR = r + dr;
            int midC = c + dc;
            int landR = r + 2*dr;
            int landC = c + 2*dc;
            if (onBoard(midR, midC) && onBoard(landR, landC)) {
                if (pieceColor(board[midR][midC]) == -color && !wasUsed(midR, midC)) {
                    if (board[landR][landC] == '.') {
                        auto copyB = board;
                        MoveStep st{r,c, landR, landC};
                        makeOneStep(copyB, st, true);

                        auto used2 = used;
                        used2.push_back({midR, midC});

                        auto newSeq = currentSeq;
                        newSeq.steps.push_back(st);
                        newSeq.capturesCount++;

                        searchCaptures(copyB, landR, landC, color, used2, newSeq, allSeq);
                        foundFurther = true;
                    }
                }
            }
        }
    } else {
        // Дамка (дальний бой)
        for (auto &[dr,dc] : directions) {
            int stepR = r + dr;
            int stepC = c + dc;
            bool foeFound = false;
            int foeR=-1, foeC=-1;

            while (onBoard(stepR, stepC)) {
                if (!foeFound) {
                    if (board[stepR][stepC] == '.') {
                        stepR += dr;
                        stepC += dc;
                    } else {
                        if (pieceColor(board[stepR][stepC]) == -color && !wasUsed(stepR, stepC)) {
                            foeFound = true;
                            foeR = stepR;
                            foeC = stepC;
                            stepR += dr;
                            stepC += dc;
                        } else {
                            break;
                        }
                    }
                } else {
                    if (board[stepR][stepC] == '.') {
                        auto copyB = board;
                        MoveStep st{r,c, stepR, stepC};
                        makeOneStep(copyB, st, true);

                        auto used2 = used;
                        used2.push_back({foeR, foeC});

                        auto newSeq = currentSeq;
                        newSeq.steps.push_back(st);
                        newSeq.capturesCount++;

                        searchCaptures(copyB, stepR, stepC, color, used2, newSeq, allSeq);
                        foundFurther = true;

                        stepR += dr;
                        stepC += dc;
                    } else {
                        break;
                    }
                }
            }
        }
    }

    if (!foundFurther && currentSeq.capturesCount > 0) {
        allSeq.push_back(currentSeq);
    }
}

// Все боевые ходы для фигуры
std::vector<MoveSequence> getAllCapturesForPiece(const std::vector<std::vector<char>>& board,
                                                 int rr, int cc)
{
    std::vector<MoveSequence> result;
    if (pieceColor(board[rr][cc]) == 0) return result;

    MoveSequence initSeq;
    initSeq.capturesCount = 0;
    std::vector<std::pair<int,int>> used;

    searchCaptures(board, rr, cc, pieceColor(board[rr][cc]), used, initSeq, result);
    return result;
}

// ПАРАЛЛЕЛЬНЫЙ поиск боёв
std::vector<MoveSequence> findAllCaptures(const std::vector<std::vector<char>>& board,
                                          bool whiteTurn)
{
    std::vector<MoveSequence> finalMoves;
    int color = (whiteTurn ? 1 : -1);

    unsigned int hw = std::thread::hardware_concurrency();
    if (hw == 0) hw = 2;

    int chunkSize = BOARD_SIZE / hw;
    if (chunkSize < 1) chunkSize = 1;

    std::vector<std::future<std::vector<MoveSequence>>> futures;
    futures.reserve(hw);

    for (int rowSt = 0; rowSt < BOARD_SIZE; rowSt += chunkSize) {
        int rowEnd = std::min(rowSt + chunkSize, BOARD_SIZE);

        futures.push_back(std::async(std::launch::async,
            [rowSt,rowEnd,color,&board]() {
                std::vector<MoveSequence> localRes;
                for (int rr = rowSt; rr < rowEnd; ++rr) {
                    for (int cc = 0; cc < BOARD_SIZE; ++cc) {
                        if (pieceColor(board[rr][cc]) == color) {
                            auto caps = getAllCapturesForPiece(board, rr, cc);
                            localRes.insert(localRes.end(), caps.begin(), caps.end());
                        }
                    }
                }
                return localRes;
            }
        ));
    }

    for (auto &fut : futures) {
        auto partial = fut.get();
        finalMoves.insert(finalMoves.end(), partial.begin(), partial.end());
    }
    return finalMoves;
}

// ПАРАЛЛЕЛЬНЫЙ поиск обычных ходов
std::vector<MoveSequence> findAllNormalMoves(const std::vector<std::vector<char>>& board,
                                             bool whiteTurn)
{
    std::vector<MoveSequence> finalMoves;
    int color = (whiteTurn ? 1 : -1);

    unsigned int hw = std::thread::hardware_concurrency();
    if (hw == 0) hw = 2;

    int chunkSize = BOARD_SIZE / hw;
    if (chunkSize < 1) chunkSize = 1;

    std::vector<std::future<std::vector<MoveSequence>>> futures;
    futures.reserve(hw);

    for (int rowSt = 0; rowSt < BOARD_SIZE; rowSt += chunkSize) {
        int rowEnd = std::min(rowSt + chunkSize, BOARD_SIZE);

        futures.push_back(std::async(std::launch::async,
            [rowSt,rowEnd,color,&board]() {
                std::vector<MoveSequence> local;
                for (int rr = rowSt; rr < rowEnd; ++rr) {
                    for (int cc = 0; cc < BOARD_SIZE; ++cc) {
                        if (pieceColor(board[rr][cc]) == color) {
                            if (!isKing(board[rr][cc])) {
                                auto manMoves = getManSimpleMoves(board, rr, cc, color);
                                local.insert(local.end(), manMoves.begin(), manMoves.end());
                            } else {
                                auto kingMoves = getKingSimpleMoves(board, rr, cc);
                                local.insert(local.end(), kingMoves.begin(), kingMoves.end());
                            }
                        }
                    }
                }
                return local;
            }
        ));
    }

    for (auto &fut : futures) {
        auto partial = fut.get();
        finalMoves.insert(finalMoves.end(), partial.begin(), partial.end());
    }
    return finalMoves;
}

// Проверка, есть ли вообще ход
bool hasAnyMove(const std::vector<std::vector<char>>& board, bool whiteTurn) {
    auto captures = findAllCaptures(board, whiteTurn);
    if (!captures.empty()) return true;

    auto normals = findAllNormalMoves(board, whiteTurn);
    return !normals.empty();
}

// Выбор случайного хода компьютером
MoveSequence chooseComputerMove(const std::vector<MoveSequence>& moves) {
    if (moves.empty()) {
        return MoveSequence{};
    }
    int idx = std::rand() % moves.size();
    return moves[idx];
}

// Преобразуем (r,c) → "A3"
std::string cellToString(int r, int c, bool userWhite) {
    if (!userWhite) {
        r = 7 - r;
        c = 7 - c;
    }
    char file = 'A' + c;
    char rank = '1' + r;
    return std::string{file, rank};
}

// Разбор строки "A3" -> (row, col)
bool parseCell(const std::string &cell, int &row, int &col, bool userWhite) {
    if (cell.size() != 2) return false;

    char file = static_cast<char>(std::toupper(cell[0]));
    if (file < 'A' || file > 'H') return false;
    int cRaw = file - 'A';

    char digit = cell[1];
    if (digit < '1' || digit > '8') return false;
    int rRaw = digit - '1';

    if (userWhite) {
        row = rRaw;
        col = cRaw;
    } else {
        row = 7 - rRaw;
        col = 7 - cRaw;
    }
    return onBoard(row, col);
}

// Считываем ввод человека
bool getMoveInput(int &fromR, int &fromC, int &toR, int &toC, bool userWhite) {
    std::string line;
    std::getline(std::cin, line);

    // trim
    while (!line.empty() && std::isspace(line.back())) line.pop_back();
    while (!line.empty() && std::isspace(line.front())) line.erase(line.begin());

    if (line.empty()) return false;

    auto pos = line.find(' ');
    if (pos == std::string::npos) return false;

    std::string fromCell = line.substr(0, pos);
    std::string toCell   = line.substr(pos+1);

    while (!toCell.empty() && std::isspace(toCell.back())) toCell.pop_back();
    while (!toCell.empty() && std::isspace(toCell.front())) toCell.erase(toCell.begin());

    int fr, fc, tr, tc;
    if (!parseCell(fromCell, fr, fc, userWhite)) return false;
    if (!parseCell(toCell,  tr, tc, userWhite))  return false;

    fromR = fr;
    fromC = fc;
    toR   = tr;
    toC   = tc;
    return true;
}

// Ход человека по координатам
bool humanMoveByCoords(std::vector<std::vector<char>> &board,
                       const std::vector<MoveSequence> &moves,
                       bool userWhite)
{
    while (true) {
        std::cout << "Введите ход (например, A3 B4): ";
        int fromR, fromC, toR, toC;
        if (!getMoveInput(fromR, fromC, toR, toC, userWhite)) {
            std::cout << "Некорректный ввод. Попробуйте снова.\n";
            continue;
        }
        bool found = false;
        for (auto &sq : moves) {
            if (!sq.steps.empty()) {
                auto &fst = sq.steps.front();
                auto &lst = sq.steps.back();
                if (fst.startRow == fromR && fst.startCol == fromC
                    && lst.endRow == toR && lst.endCol == toC)
                {
                    makeMoveSequence(board, sq);
                    found = true;
                    break;
                }
            }
        }
        if (found) {
            return true;
        }
        std::cout << "Некорректный ход.\n";
    }
}

// -------------------- main --------------------
int main() {
    setlocale(LC_ALL, "");
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    std::cout << "----ПРАВИЛА ИГРЫ В КЛАССИЧЕСКИЕ ШАШКИ----\n"
                 "1) Шашки ходят вперед. \n"
                 "2) Дамка ходит по диагонали на любое свободное поле как вперёд, так и назад, но не может перескакивать свои шашки или дамки.\n"
                 "3) Взятие обязательно. \n"
                 "4) Взятие простой шашкой производится как вперёд, так и назад.\n"
                 "5) Работает множественно взятие \n"
                 "6) Дамка бьёт по диагонали, как вперёд, так и назад, и становится на любое свободное поле после побитой шашки. \n"
                 "7) Аналогично, дамка может бить несколько фигур соперника и должна бить до тех пор, пока это возможно.\n"
                 "8) При нескольких вариантах взятия, например, одну шашку или две, игрок выбирает вариант взятия по своему усмотрению.\n"
                 "9) Белые ходят первыми\n\n"
                 "----УСЛОВИЯ ПОБЕДЫ---- \n"
                 "Вы съели все шашки и дамки соперника\n"
                 "Или вы обездвижели все шашки и дамки соперника\n\n"
    ;
    std::cout << "Выберите сторону:\n";
    std::cout << "1) Белые \n2) Чёрные \n";
    int choice = 0;
    while (true) {
        std::cout << "Введите 1 или 2: ";
        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }
        if (choice == 1 || choice == 2) break;
        std::cout << "Некорректный ввод.\n";
    }
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    bool userIsWhite = (choice == 1);

    // Инициализируем доску
    std::vector<std::vector<char>> board;
    initBoard(board);

    bool whiteMove = true;
    bool gameOver = false;
    int moveCount = 1;

    while (!gameOver) {
        auto startTime = std::chrono::steady_clock::now();

        printBoard(board, userIsWhite);

        bool isUserTurn = ((whiteMove && userIsWhite) ||
                           (!whiteMove && !userIsWhite));

        std::cout << std::format("{} ({}):\n",
                    (whiteMove ? "[Ход белых]" : "[Ход чёрных]"),
                    (isUserTurn ? "пользователь" : "компьютер"));

        // Проверяем наличие ходов
        if (!hasAnyMove(board, whiteMove)) {
            std::cout << std::format("{} нет ходов! Игра завершена.\n",
                                     (whiteMove ? "У белых" : "У чёрных"));
            gameOver = true;
        } else {
            // Пытаемся найти боевые ходы
            auto captures = findAllCaptures(board, whiteMove);
            if (!captures.empty()) {
                // Есть бой
                if (isUserTurn) {
                    std::cout << "Обязательный бой!\n";
                    humanMoveByCoords(board, captures, userIsWhite);
                } else {
                    auto compMove = chooseComputerMove(captures);
                    std::cout << std::format("Компьютер ({}) бьёт: ",
                                             (whiteMove ? "белые" : "чёрные"));
                    for (size_t i = 0; i < compMove.steps.size(); i++) {
                        auto &st = compMove.steps[i];
                        auto fs = cellToString(st.startRow, st.startCol, userIsWhite);
                        auto ls = cellToString(st.endRow, st.endCol, userIsWhite);
                        std::cout << std::format("({})->({})", fs, ls);
                        if (i+1 < compMove.steps.size()) std::cout << ", ";
                    }
                    std::cout << std::format(" [съедено: {}]\n", compMove.capturesCount);
                    makeMoveSequence(board, compMove);
                }
            } else {
                // Обычные ходы
                auto normals = findAllNormalMoves(board, whiteMove);
                if (normals.empty()) {
                    std::cout << "Нет ходов, завершаем.\n";
                    gameOver = true;
                } else {
                    if (isUserTurn) {
                        humanMoveByCoords(board, normals, userIsWhite);
                    } else {
                        auto compMove = chooseComputerMove(normals);
                        auto &fs = compMove.steps.front();
                        auto &ls = compMove.steps.back();
                        auto fromStr = cellToString(fs.startRow, fs.startCol, userIsWhite);
                        auto toStr   = cellToString(ls.endRow,   ls.endCol,   userIsWhite);
                        std::cout << std::format(
                            "Компьютер ({}) ходит: ({}) -> ({})\n",
                            (whiteMove ? "белые" : "чёрные"),
                            fromStr, toStr
                        );
                        makeMoveSequence(board, compMove);
                    }
                }
            }
        }

        // Проверяем, не выбиты ли все
        if (!gameOver) {
            int whiteCount=0, blackCount=0;
            for (int rr = 0; rr < BOARD_SIZE; rr++) {
                for (int cc = 0; cc < BOARD_SIZE; cc++) {
                    int col = pieceColor(board[rr][cc]);
                    if (col == 1) ++whiteCount;
                    if (col == -1) ++blackCount;
                }
            }
            if (whiteCount == 0) {
                std::cout << "Чёрные победили!\n";
                gameOver = true;
            } else if (blackCount == 0) {
                std::cout << "Белые победили!\n";
                gameOver = true;
            }
        }

        auto endTime = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

        std::cout << std::format("Ход #{} завершён за {} ms\n\n", moveCount, elapsed);

        if (!gameOver) {
            whiteMove = !whiteMove;
        }
        moveCount++;
    }

    std::cout << "Спасибо за игру!\n";
    return 0;
}