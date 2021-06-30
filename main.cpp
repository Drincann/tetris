#include <conio.h>
#include <stdio.h>
#include <time.h>
#include <windows.h>
#include <functional>
#include <iostream>
#include <unordered_map>
#include <vector>

using namespace std;

#define de drawElement

const int Height = 26;  //绘制的界面的高 (y轴)，以左上角为原点，向下为y正方向
const int Length = 66;  //绘制的界面的长 (x轴)，向右为x正方向
const char LEFT = 75;
const char RIGHT = 77;
const char DOWN = 80;
const char UP = 72;
const int WHITE = 15;
const char block[4] = "■";  //单个方块的元素，一个方块横向占有2个字符的位置

//需要绘制的界面的每个位置的元素
struct DrawElement {
    bool isBlock, changed;
    char c;     //不是方块的情况下c就储存该位置的char类型元素
    int color;  //否则记录该位置的方块颜色
    int hasCode;  //该位置的方块的对应游戏方块的哈希码
} drawElement[Length + 5][Height + 5];

//游戏方块图形
struct GameBlockGraph {
    bool hasBlock
        [4][4];  //类型id和旋转状态对应游戏方块图形内部坐标x,y位置上是否有方块
} mar[8][4];

//游戏方块
struct GameBlock {
    int x, y;   //游戏方块的左上角坐标
    int color;  //颜色
    int id;     //类型id
    int rot;  //旋转状态:0为未旋转,1为顺时针旋转90度,2为180,3为270
    int hasCode;  //独一无二的哈希码，用来识别区分每个游戏方块
};
GameBlock nowBlock;   //保存现在正在下落的游戏方块的信息
GameBlock nextBlock;  //保存确定好的下一个游戏方块的信息

int score = 0;   //当前分数
int hasCnt = 0;  //当前最大的哈希码

//右边栏一些信息的位置(有的是x有的是y) ,rightTabEdgeX为方块的后一个字符位置
int nextBlockY = 3, divideLineY = nextBlockY + 7, nowScoreY = divideLineY + 2,
    nowScoreX, rightTabEdgeX;

void draw();
void addBlock(int x, int* y, int color, int hasCode);
void addString(int x, int* y, const char* s[]);
void removeChar(int x, int y);
void removeBlock(int x, int y);
void addGameBlock(int px, int py, GameBlock gb);
void removeGameBlock(int px, int py, GameBlock gb);
void initMargin();
void initGameBlockGraph();
void initStartInterface();
void init();
void initGameInterface();
void printBlock(int color);
void moveCur(int x, int y);
void updateScoreView();
GameBlock randomGameBlock(int lx, int rx);
void updateNextBlockView();
void updateNextBlock();
bool outMargin(int x, int y);
bool canHold(int px, int py, GameBlock gb);
bool canMove(bool right, GameBlock gb);
bool judgeTouchBottom(GameBlock gb);
void downDrawElement(int sy);
void clearCal();
bool drop();
void move(char type);
bool summonBlock();
void rotateBlock();
void endGame();

namespace Game {

// 事件类型
namespace EventType {
string up = "up";
string down = "down";
string right = "left";
string left = "left";
string space = "space";
}  // namespace EventType

/**
 * @class
 * 事件分发器
 */
class EventEmitter {
   private:
    unordered_map<string, vector<function<void()>>>* eventMapPointer =
        new unordered_map<string, vector<function<void()>>>();
    unordered_map<string, vector<function<void()>>>& eventMap;

   public:
    EventEmitter() : eventMap(*this->eventMapPointer) {}
    ~EventEmitter() { delete this->eventMapPointer; }

    /**
     * @public
     * 注册事件
     */
    void on(string eventType, function<void()> callback) {
        if (this->eventMap.find(eventType) != this->eventMap.end()) {
            this->eventMap[eventType].emplace_back(callback);
        } else {
            /*
                这里不需要在堆开内存，因为容器会负责将栈上的 vector 在堆中 map
                原地构造
             */
            vector<function<void()>> callbacks = {callback};
            this->eventMap.insert(make_pair(eventType, callbacks));
        }
    }

    /**
     * @public
     * 分发事件
     */
    void emit(string eventType) {
        if (this->eventMap.find(eventType) != this->eventMap.end()) {
            for (function<void()> callback : eventMap[eventType]) {
                callback();
            }
        }
    }
};

/**
 * 抽象层，对扩展开放
 * 初始化事件分发器
 */
EventEmitter& initEventDispatcher() {
    EventEmitter* dispatcher = new EventEmitter();
    return *dispatcher;
}

// 全局的事件分发器
EventEmitter dispatcher = initEventDispatcher();

// 游戏状态
HANDLE pauseMutex = CreateMutex(NULL, FALSE, NULL);  // 锁
bool touchBottom = false;

void gameInit() {
    init();
    system("pause>>nul");  //暂停，按任意键继续
    initGameInterface();   //初始化开始游戏后的界面
    updateNextBlock();  //先确定下一个游戏方块以便summonBlock生成
    summonBlock();      //生成新的方块
    updateNextBlock();  //确定下一个游戏方块
    updateNextBlockView();  //更新右边栏下一个游戏方块的显示
}

void gameProcess() {
    if (touchBottom) {  //如果游戏方块处于到底的状态
        touchBottom = false;
        clearCal();  //清空完整的行
    }
    updateScoreView();  //更新分数的显示
    if (drop()) {       //游戏方块下落，并返回下落后是否触底
        touchBottom = true;
        if (!summonBlock()) {  //生成新的游戏方块，如果不能生成则结束游戏
            endGame();
        }
        updateNextBlock();      //确定下一个游戏方块
        updateNextBlockView();  //更新右边栏下一个游戏方块的显示
    }
    for (int i = 1; i <= 70;
         i++) {  // 0.7秒进行一刻 (每0.7秒进行一次正常的下落和清空整行运算)
        draw();  //绘制界面
        Sleep(10);  // 10毫秒一帧(每10毫秒进行一次界面绘制和键盘响应)
        WaitForSingleObject(pauseMutex, INFINITE);
        ReleaseMutex(pauseMutex);
    }
}

/**
 * 通过互斥锁控制游戏暂停
 */
void gamePause() {
    WaitForSingleObject(pauseMutex, INFINITE);
}

void gameContinue() {
    ReleaseMutex(pauseMutex);
}

/**
 * 事件循环主线程
 */
void startEventLoop() {
    while (1) {
        if (kbhit() != 0) {  //有键盘输入
            char c = getch();
            if (c == ' ') {
                dispatcher.emit("space");
            } else if (c == UP) {
                dispatcher.emit("up");
            } else if (c == DOWN) {
                dispatcher.emit("down");
            } else if (c == LEFT) {
                dispatcher.emit("left");
            } else if (c == RIGHT) {
                dispatcher.emit("right");
            }
        }
    }
}

/**
 * 渲染线程
 */
DWORD rander(LPVOID lpParameter) {
    gameInit();

    while (1) {
        gameProcess();
    }
}

}  // namespace Game

//主函数
int main() {
    // 游戏逻辑
    Game::dispatcher.on("up", []() { rotateBlock(); });
    Game::dispatcher.on("down", []() {
        if (drop()) {  //下落一次
            if (!summonBlock()) {  //生成新的游戏方块，如果不能生成则结束游戏
                endGame();
            }
            updateNextBlock();      //确定下一个游戏方块
            updateNextBlockView();  //更新右边栏下一个游戏方块的显示
            clearCal();
        }
    });
    Game::dispatcher.on("left", []() { move(LEFT); });
    Game::dispatcher.on("right", []() { move(RIGHT); });
    bool pause = false;
    Game::dispatcher.on("space", [&]() {
        if (pause) {
            Game::gameContinue();
        } else {
            Game::gamePause();
        }
        pause = !pause;
    });

    CreateThread(NULL, 0, Game::rander, NULL, 0, NULL);
    Game::startEventLoop();

    return 0;
}

//在x,y处添加一个需要绘制的方块（由于方块占2字符，所以这里规定x必须为奇数）
void addBlock(int x, int y, int color, int hasCode) {
    if (!(x & 1))
        x--;  //如果x为偶数则-1强行变为奇数
    de[x][y].isBlock = true;
    de[x][y].color = color;
    de[x][y].hasCode = hasCode;
    de[x][y].changed = true;
    de[x + 1][y].isBlock = true;
    de[x + 1][y].color = color;
    de[x + 1][y].hasCode = hasCode;
    de[x][y].changed = true;
}

//在x,y处开始添加一段字符串
void addString(int x, int y, const char s[]) {
    int len = strlen(s);
    for (int i = x; i <= Length && i - x < len; i++) {
        de[i][y].isBlock = false;
        de[i][y].c = s[i - x];
        de[i][y].changed = true;
    }
}

//清除在x,y处的字符
void removeChar(int x, int y) {
    de[x][y].isBlock = false;
    de[x][y].c = 0;
    de[x][y].color = 0;
    de[x][y].hasCode = 0;
    de[x][y].changed = true;
}

//清除在x,y处的方块，x为奇数
void removeBlock(int x, int y) {
    if (!(x & 1))
        x--;  //如果x为偶数则-1强行变为奇数
    removeChar(x, y);
    removeChar(x + 1, y);
}

//添加一个需要绘制的游戏方块（整体的一个俄罗斯方块）,以px,py为游戏方块左上角坐标(x需为奇数)
void addGameBlock(int px, int py, GameBlock gb) {  // id为游戏方块类型的编号
    if (!(px & 1))
        px--;  //如果x为偶数则-1强行变为奇数
    for (int x = 0; x < 4; x++) {
        for (int y = 0; y < 4; y++) {
            if (mar[gb.id][gb.rot].hasBlock[x][y]) {
                addBlock(px + x * 2, py + y, gb.color, gb.hasCode);
            }
        }
    }
}

//清除一个游戏方块,以px,py为该游戏方块左上角坐标
void removeGameBlock(int px, int py, GameBlock gb) {
    if (!(px & 1))
        px--;  //如果x为偶数则-1强行变为奇数
    for (int x = 0; x < 4; x++) {
        for (int y = 0; y < 4; y++) {
            if (mar[gb.id][gb.rot].hasBlock[x][y]) {
                removeBlock(px + x * 2, py + y);
            }
        }
    }
}

//初始化边界
void initMargin() {
    for (int x = 1; x <= Length; x++) {
        for (int y = 1; y <= Height; y++) {
            if (x == 1 || y == 1 || x == Length || y == Height) {
                addBlock(x, y, WHITE, 0);
            }
        }
    }
}

//初始化游戏方块图形
void initGameBlockGraph() {
    // id1
    for (int i = 0; i < 4; i++)
        mar[1][0].hasBlock[0][i] = true;
    // id2
    for (int i = 0; i < 4; i++)
        mar[2][0].hasBlock[1][i] = true;
    mar[2][0].hasBlock[0][3] = true;
    // id3
    for (int i = 0; i < 4; i++)
        mar[3][0].hasBlock[0][i] = true;
    mar[3][0].hasBlock[1][3] = true;
    // id4
    mar[4][0].hasBlock[0][0] = true;
    mar[4][0].hasBlock[1][0] = true;
    mar[4][0].hasBlock[0][1] = true;
    mar[4][0].hasBlock[1][1] = true;
    // id5
    mar[5][0].hasBlock[0][0] = true;
    mar[5][0].hasBlock[1][0] = true;
    mar[5][0].hasBlock[1][1] = true;
    mar[5][0].hasBlock[2][1] = true;
    // id6
    mar[6][0].hasBlock[1][0] = true;
    mar[6][0].hasBlock[0][1] = true;
    mar[6][0].hasBlock[1][1] = true;
    mar[6][0].hasBlock[2][1] = true;
    // id7
    mar[7][0].hasBlock[1][0] = true;
    mar[7][0].hasBlock[2][0] = true;
    mar[7][0].hasBlock[0][1] = true;
    mar[7][0].hasBlock[1][1] = true;
    //旋转
    for (int i = 1; i <= 3; i++) {
        for (int j = 1; j <= 7; j++) {
            for (int x = 0; x < 4; x++) {
                for (int y = 0; y < 4; y++) {
                    mar[j][i].hasBlock[3 - y][x] = mar[j][i - 1].hasBlock[x][y];
                }
            }
        }
    }
    //确保左上角坐标
    for (int i = 1; i <= 7; i++) {
        for (int j = 1; j <= 3; j++) {
            bool have = false;
            while (!have) {  //对行
                for (int x = 0; x < 4; x++) {
                    if (mar[i][j].hasBlock[x][0]) {
                        have = true;
                        break;
                    }
                }
                if (!have) {
                    for (int y = 0; y < 3; y++) {
                        for (int x = 0; x < 4; x++)
                            mar[i][j].hasBlock[x][y] =
                                mar[i][j].hasBlock[x][y + 1];
                    }
                    for (int x = 0; x < 4; x++)
                        mar[i][j].hasBlock[x][3] = false;
                }
            }
            have = false;
            while (!have) {  //对列
                for (int y = 0; y < 4; y++) {
                    if (mar[i][j].hasBlock[0][y]) {
                        have = true;
                        break;
                    }
                }
                if (!have) {
                    memcpy(mar[i][j].hasBlock[0], mar[i][j].hasBlock[1],
                           4 * sizeof(bool));
                    memcpy(mar[i][j].hasBlock[1], mar[i][j].hasBlock[2],
                           4 * sizeof(bool));
                    memcpy(mar[i][j].hasBlock[2], mar[i][j].hasBlock[3],
                           4 * sizeof(bool));
                    memset(mar[i][j].hasBlock[3], 0, 4 * sizeof(bool));
                }
            }
        }
    }
}

//初始化开始界面
void initStartInterface() {
    int x = Length / 2 - 8, y = Height / 2;
    addString(x, y, "按任意键开始游戏");
}

//主初始化
void init() {
    srand((int)time(NULL));  //以当前时间作为随机数生成种子
    initGameBlockGraph();
    system("mode con cols=66 lines=26");  //设置窗口大小
    CONSOLE_SCREEN_BUFFER_INFO info;
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(handle, &info);
    SMALL_RECT rect = info.srWindow;
    COORD size = {rect.Right + 1, rect.Bottom + 1};
    SetConsoleScreenBufferSize(handle, size);  //定义缓冲区大小
    initMargin();
    initStartInterface();
    draw();
}

//初始化开始游戏后的界面
void initGameInterface() {
    for (int x = Length / 2 - 8; x <= Length / 2 + 8; x++)
        removeChar(x, Height / 2);
    rightTabEdgeX =
        Length -
        14 * 2;  //右边栏的左边界方块的位置的x（占的两个字符的后一个字符的位置）
    for (int y = 2; y < Height; y++) {
        addBlock(rightTabEdgeX, y, WHITE, 0);
    }
    nowScoreX = rightTabEdgeX + 2;
    addString(rightTabEdgeX + 9, nextBlockY, "下一个方块");
    addString(rightTabEdgeX + 1, divideLineY, "--------------------------");
    addString(nowScoreX, nowScoreY, "当前分数：");
    addString(nowScoreX + 11, nowScoreY, "0");
    addString(nowScoreX, nowScoreY + 2, "[space]空格键暂停游戏");
    addString(nowScoreX, nowScoreY + 3, "↑方向键使方块旋转");
    addString(nowScoreX, nowScoreY + 4, "←→方向键控制左右移动");
    addString(nowScoreX, nowScoreY + 5, "↓方向键使方块加速下落");
}

//在当前位置打印颜色方块
void printBlock(int color) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
    printf("%s", block);
}

//移动光标至x,y，数字从0开始
void moveCur(int x, int y) {
    COORD pos = {x, y};
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
}

//绘制界面主函数
void draw() {
    CONSOLE_CURSOR_INFO cursor_info = {1, 0};
    SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE),
                         &cursor_info);  //隐藏光标
    moveCur(0, 0);
    for (int y = 1; y <= Height; y++) {
        for (int x = 1; x <= Length;) {
            if (drawElement[x][y].isBlock) {
                if (de[x][y].changed) {
                    moveCur(x - 1, y - 1);
                    printBlock(drawElement[x][y].color);
                    de[x][y].changed = false;
                    de[x + 1][y].changed = false;
                }
                x += 2;
            } else {
                if (de[x][y].changed) {
                    moveCur(x - 1, y - 1);
                    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                            WHITE);
                    printf("%c", de[x][y].c == 0 ? ' ' : de[x][y].c);
                    de[x][y].changed = false;
                }
                x++;
            }
        }
    }
}

//更新分数的显示
void updateScoreView() {
    addString(nowScoreX, nowScoreY, "当前分数：");
    char s[10];
    sprintf(s, "%d", score);
    addString(nowScoreX + 11, nowScoreY, s);
}

//返回一个随机的游戏方块,lx为方块位置随机的左边界，rx为右边界，随机的位置不包含这俩边界
GameBlock randomGameBlock(int lx, int rx) {
    GameBlock gb;
    gb.x = rand() % (rx - 7 - lx) + lx + 1;
    if (!(gb.x & 1))
        gb.x--;  //如果x为偶数则-1强行变为奇数
    gb.id = rand() % 7 + 1;
    gb.rot = rand() % 4;
    gb.color = rand() % 14 + 1;
    gb.y = 2;
    gb.hasCode = ++hasCnt;
    return gb;
}

//更新右边栏上的下一个游戏方块的显示
void updateNextBlockView() {
    removeGameBlock(rightTabEdgeX + 11, nextBlockY + 2, nowBlock);
    addGameBlock(rightTabEdgeX + 11, nextBlockY + 2, nextBlock);
}

//确定下一个游戏方块
void updateNextBlock() {
    nextBlock = randomGameBlock(2, rightTabEdgeX - 1);
}

//判断x，y处位置是否为边界或超出游戏主界面（在右边栏及其边界的位置也算超出游戏主界面）
bool outMargin(int x, int y) {
    if (x <= 2 || y == 1 || y == Height)
        return true;
    if (x >= nowScoreX - 3)
        return true;
    return false;
}

//判断在px,py位置为游戏方块左上角时能否放置这个游戏方块
bool canHold(int px, int py, GameBlock gb) {
    for (int x = 0; x < 4; x++) {
        for (int y = 0; y < 4; y++) {
            if (mar[gb.id][gb.rot].hasBlock[x][y] &&
                (outMargin(px + x * 2, py + y) ||
                 (de[px + x * 2][py + y].isBlock &&
                  de[px + x * 2][py + y].hasCode != gb.hasCode)))
                return false;
        }
    }
    return true;
}

//判断能否左右移动，right为true表示判断能否右移，反之判断左移
//,需要传递用来判断的游戏方块
bool canMove(bool right, GameBlock gb) {
    if (!(gb.x & 1))
        gb.x--;  //如果x为偶数则-1强行变为奇数
    return canHold(gb.x + (right ? 2 : -2), gb.y, gb);
    return true;
}

//判断游戏方块是否已经触底
bool judgeTouchBottom(GameBlock gb) {
    if (!(gb.x & 1))
        gb.x--;  //如果x为偶数则-1强行变为奇数
    return !canHold(gb.x, gb.y + 1, gb);
}

// drawElement,y坐标小于sy的下移,把sy覆盖
void downDrawElement(int sy) {
    for (int y = sy; y >= 3; y--) {
        for (int x = 3; x < rightTabEdgeX - 1; x++) {
            if (!de[x][y].isBlock && !de[x][y].isBlock)
                continue;
            if (!de[x][y - 1].isBlock)
                removeChar(x, y);
            else {
                de[x][y] = de[x][y - 1];
                de[x][y].changed = true;
            }
        }
    }
    for (int x = 3; x < rightTabEdgeX - 1; x++) {
        if (de[x][2].isBlock)
            removeChar(x, 2);
    }
}

//清空完整的行并更新分数的变量值
void clearCal() {
    int num = 0;
    for (int y = 2; y <= Height - 1; y++) {
        bool full = true;
        for (int x = 3; x < rightTabEdgeX - 1; x += 2) {
            if (!de[x][y].isBlock) {
                full = false;
                break;
            }
        }
        if (full) {
            downDrawElement(y);
            num++;
        }
    }
    score += num * 5;
}

//游戏方块下落，并返回下落后是否触底
bool drop() {
    if (judgeTouchBottom(nowBlock))
        return true;
    removeGameBlock(nowBlock.x, nowBlock.y, nowBlock);
    nowBlock.y++;
    addGameBlock(nowBlock.x, nowBlock.y, nowBlock);
    return judgeTouchBottom(nowBlock);
}

//游戏方块左右移动 ,type为RIGHT或者LEFT
void move(char type) {
    bool right = type == RIGHT ? true : false;
    if (canMove(right, nowBlock)) {
        removeGameBlock(nowBlock.x, nowBlock.y, nowBlock);
        nowBlock.x += right ? 2 : -2;
        addGameBlock(nowBlock.x, nowBlock.y, nowBlock);
    }
}

//生成新的游戏方块并返回能否生成
bool summonBlock() {
    nowBlock = nextBlock;
    if (!canHold(nowBlock.x, nowBlock.y, nowBlock))
        return false;
    addGameBlock(nowBlock.x, nowBlock.y, nowBlock);
    return true;
}

//旋转游戏方块
void rotateBlock() {
    GameBlock tmp = nowBlock;
    tmp.rot = (tmp.rot + 1) % 4;
    if (canHold(nowBlock.x, nowBlock.y, tmp)) {
        removeGameBlock(nowBlock.x, nowBlock.y, nowBlock);
        addGameBlock(tmp.x, tmp.y, tmp);
        nowBlock = tmp;
    }
}

//结束游戏
void endGame() {
    for (int x = 3; x <= Length - 2; x++) {
        for (int y = 2; y < Height; y++) {
            if (de[x][y].isBlock || de[x][y].c != 0)
                removeChar(x, y);
        }
    }
    int x = Length / 2 - 5, y = Height / 2 - 2;
    addString(x, y, "游戏结束");
    addString(x, y + 2, "分数：");
    char s[10];
    sprintf(s, "%d", score);
    addString(x + 7, y + 2, s);
    draw();
    Sleep(3000);
    exit(0);
}