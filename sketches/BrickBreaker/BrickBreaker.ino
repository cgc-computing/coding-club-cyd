#include <TFT_eSPI.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

// ---------- Touch pins (CYD) ----------
#define XPT2046_IRQ  36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK  25
#define XPT2046_CS   33

// Raw touch calibration (typical CYD values)
#define TOUCH_X_MIN 200
#define TOUCH_X_MAX 3700
#define TOUCH_Y_MIN 240
#define TOUCH_Y_MAX 3800

TFT_eSPI tft = TFT_eSPI();
SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);

// ---------- Screen ----------
const int SCR_W = 320;
const int SCR_H = 240;

// ---------- Bricks ----------
const int BRICK_COLS = 10;
const int BRICK_ROWS = 5;
const int BRICK_W = 30;
const int BRICK_H = 12;
const int BRICK_GAP = 2;
const int BRICK_OFFSET_X = 6;
const int BRICK_OFFSET_Y = 30;
bool bricks[BRICK_ROWS][BRICK_COLS];
const uint16_t rowColors[BRICK_ROWS] = {
  TFT_RED, TFT_ORANGE, TFT_YELLOW, TFT_GREEN, TFT_CYAN
};

// ---------- Paddle ----------
const int PADDLE_W = 50;
const int PADDLE_H = 8;
const int PADDLE_Y = SCR_H - 16;
float paddleX = (SCR_W - PADDLE_W) / 2.0;
int prevPaddleX = (int)paddleX;

// ---------- Ball ----------
const int BALL_R = 4;
float ballX, ballY, ballVX, ballVY;
int prevBallX, prevBallY;
bool ballLaunched = false;

// ---------- Game state ----------
int score = 0;
int lives = 3;
int bricksLeft = 0;
bool gameOver = false;
bool gameWon = false;
unsigned long lastFrame = 0;

// ---------- Prototypes ----------
void resetGame();
void resetBall();
void drawBricks();
void drawBrick(int r, int c);
void drawHUD();
void updateTouch();
void updateBall();
void showMessage(const char *msg, uint16_t color);

void setup() {
  Serial.begin(115200);

  tft.init();
  tft.setRotation(1);   // landscape; use 3 if upside down
  tft.fillScreen(TFT_BLACK);

  touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(touchSPI);
  ts.setRotation(1);

  randomSeed(analogRead(34));
  resetGame();
}

void resetGame() {
  score = 0;
  lives = 3;
  gameOver = false;
  gameWon = false;
  bricksLeft = BRICK_ROWS * BRICK_COLS;
  for (int r = 0; r < BRICK_ROWS; r++)
    for (int c = 0; c < BRICK_COLS; c++)
      bricks[r][c] = true;

  tft.fillScreen(TFT_BLACK);
  drawBricks();
  drawHUD();
  paddleX = (SCR_W - PADDLE_W) / 2.0;
  prevPaddleX = (int)paddleX;
  resetBall();
}

void resetBall() {
  ballLaunched = false;
  ballX = paddleX + PADDLE_W / 2.0;
  ballY = PADDLE_Y - BALL_R - 1;
  prevBallX = (int)ballX;
  prevBallY = (int)ballY;
  float angle = random(-30, 31) * PI / 180.0;
  float speed = 3.0;
  ballVX = speed * sin(angle);
  ballVY = -speed * cos(angle);
}

void drawBrick(int r, int c) {
  int x = BRICK_OFFSET_X + c * (BRICK_W + BRICK_GAP);
  int y = BRICK_OFFSET_Y + r * (BRICK_H + BRICK_GAP);
  if (bricks[r][c]) tft.fillRect(x, y, BRICK_W, BRICK_H, rowColors[r]);
  else              tft.fillRect(x, y, BRICK_W, BRICK_H, TFT_BLACK);
}

void drawBricks() {
  for (int r = 0; r < BRICK_ROWS; r++)
    for (int c = 0; c < BRICK_COLS; c++)
      drawBrick(r, c);
}

void drawHUD() {
  tft.fillRect(0, 0, SCR_W, 22, TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(4, 4);
  tft.print("Score:");
  tft.print(score);
  tft.setCursor(SCR_W - 90, 4);
  tft.print("Lives:");
  tft.print(lives);
}

void showMessage(const char *msg, uint16_t color) {
  tft.setTextSize(3);
  tft.setTextColor(color, TFT_BLACK);
  int w = tft.textWidth(msg);
  tft.setCursor((SCR_W - w) / 2, SCR_H / 2 - 12);
  tft.print(msg);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  const char *sub = "Tap to play again";
  tft.setCursor((SCR_W - tft.textWidth(sub)) / 2, SCR_H / 2 + 20);
  tft.print(sub);
}

// Returns true if the screen is currently touched
void updateTouch() {
  if (ts.tirqTouched() && ts.touched()) {
    TS_Point p = ts.getPoint();
    int sx = map(p.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, SCR_W);
    sx = constrain(sx, 0, SCR_W - 1);

    // Smooth paddle movement toward touch
    float target = sx - PADDLE_W / 2.0;
    target = constrain(target, 0, SCR_W - PADDLE_W);
    paddleX += (target - paddleX) * 0.6;

    if (!ballLaunched && !gameOver && !gameWon) {
      static unsigned long touchStart = 0;
      if (touchStart == 0) touchStart = millis();
      if (millis() - touchStart > 150) {
        ballLaunched = true;
        touchStart = 0;
      }
    }
  }
}

void updateBall() {
  if (!ballLaunched) {
    ballX = paddleX + PADDLE_W / 2.0;
    ballY = PADDLE_Y - BALL_R - 1;
    return;
  }

  ballX += ballVX;
  ballY += ballVY;

  // Walls
  if (ballX - BALL_R < 0)      { ballX = BALL_R;            ballVX = -ballVX; }
  if (ballX + BALL_R > SCR_W)  { ballX = SCR_W - BALL_R;    ballVX = -ballVX; }
  if (ballY - BALL_R < 22)     { ballY = 22 + BALL_R;       ballVY = -ballVY; }

  // Paddle
  if (ballVY > 0 &&
      ballY + BALL_R >= PADDLE_Y &&
      ballY + BALL_R <= PADDLE_Y + PADDLE_H + 4 &&
      ballX >= paddleX - BALL_R &&
      ballX <= paddleX + PADDLE_W + BALL_R) {
    float hit = (ballX - (paddleX + PADDLE_W / 2.0)) / (PADDLE_W / 2.0);
    hit = constrain(hit, -1.0, 1.0);
    float speed = sqrt(ballVX * ballVX + ballVY * ballVY);
    float angle = hit * 60.0 * PI / 180.0;
    ballVX = speed * sin(angle);
    ballVY = -speed * cos(angle);
    ballY = PADDLE_Y - BALL_R - 1;
  }

  // Bricks
  int col = ((int)ballX - BRICK_OFFSET_X) / (BRICK_W + BRICK_GAP);
  int row = ((int)ballY - BRICK_OFFSET_Y) / (BRICK_H + BRICK_GAP);
  if (ballY >= BRICK_OFFSET_Y &&
      row >= 0 && row < BRICK_ROWS &&
      col >= 0 && col < BRICK_COLS &&
      bricks[row][col]) {
    bricks[row][col] = false;
    drawBrick(row, col);
    ballVY = -ballVY;
    score += 10;
    bricksLeft--;
    drawHUD();

    // Slight speed-up
    float speed = sqrt(ballVX * ballVX + ballVY * ballVY);
    if (speed < 6.0) { ballVX *= 1.01; ballVY *= 1.01; }

    if (bricksLeft <= 0) {
      gameWon = true;
      showMessage("YOU WIN!", TFT_GREEN);
    }
  }

  // Missed ball
  if (ballY - BALL_R > SCR_H) {
    lives--;
    drawHUD();
    // erase old ball
    tft.fillCircle(prevBallX, prevBallY, BALL_R + 1, TFT_BLACK);
    if (lives <= 0) {
      gameOver = true;
      showMessage("GAME OVER", TFT_RED);
    } else {
      resetBall();
      delay(500);
    }
  }
}

void loop() {
  if (millis() - lastFrame < 16) return;   // ~60 FPS
  lastFrame = millis();

  // Restart on tap after game end
  if ((gameOver || gameWon)) {
    if (ts.tirqTouched() && ts.touched()) {
      delay(300);
      resetGame();
    }
    return;
  }

  updateTouch();
  updateBall();

  // Erase old paddle area, draw new paddle
  int px = (int)paddleX;
  if (px != prevPaddleX) {
    tft.fillRect(prevPaddleX, PADDLE_Y, PADDLE_W, PADDLE_H, TFT_BLACK);
    prevPaddleX = px;
  }
  tft.fillRect(px, PADDLE_Y, PADDLE_W, PADDLE_H, TFT_WHITE);

  // Erase old ball, draw new ball
  tft.fillCircle(prevBallX, prevBallY, BALL_R + 1, TFT_BLACK);
  tft.fillCircle((int)ballX, (int)ballY, BALL_R, TFT_WHITE);
  prevBallX = (int)ballX;
  prevBallY = (int)ballY;

  // Redraw bricks near ball in case erase clipped them
  if (ballY < BRICK_OFFSET_Y + BRICK_ROWS * (BRICK_H + BRICK_GAP) + BALL_R + 2) {
    for (int r = 0; r < BRICK_ROWS; r++)
      for (int c = 0; c < BRICK_COLS; c++)
        if (bricks[r][c]) drawBrick(r, c);
  }
}
