// MovieTerminal -- "computer stuff" for the background of a movie scene.
//
// The screen is split into four panes:
//
//   top left      a network map -- nodes join, drop out, and pass packets
//   bottom left   made-up source code, scrolling forever
//   top right     a hex dump, scrolling forever
//   bottom right  log lines and numbers, scrolling forever
//
// None of it means anything. It just has to look busy.
//
// Press the BOOT button to change the color scheme.
//
// Needs only TFT_eSPI (set up per docs/CYD-SETUP.md). No touch, no fonts:
// the tiny font lives in TinyFont.h.

#include <TFT_eSPI.h>
#include <esp_system.h>

#include "TinyFont.h"

// ---------------------------------------------------------------- pins ----

constexpr int PIN_BACKLIGHT = 21;
constexpr int PIN_BOOT      = 0;     // BOOT button, reads LOW when pressed

// -------------------------------------------------------------- layout ----
// The screen is 320 x 240 in landscape. Text cells are 4 x 6 pixels.

constexpr int MAX_COLS = 40;                   // widest pane, in characters
constexpr int MAX_ROWS = 240 / CELL_H + 2;     // tallest pane, plus spares

constexpr int STRIP_H = 16;                    // scanlines drawn per chunk
constexpr int MAX_PANE_W = 160;

// ---------------------------------------------------------- line styles ----
// Each line of text has one style, which picks its color.

enum Style : uint8_t { NORMAL, DIM, BRIGHT, ALERT, STYLE_COUNT };

// The network map has its own short list of colors, and each node on it is
// in one of four states.
//
// (These live up here, rather than beside the network code further down,
// because Arduino needs to know about a type before the first function in
// the sketch if any function takes that type as a parameter.)
enum NetColor : uint8_t { NET_BLACK, NET_LINK, NET_NODE, NET_BRIGHT, NET_ALERT,
                          NET_COLOR_COUNT };

enum NodeState : uint8_t { EMPTY, JOINING, ONLINE, DROPPING };

// -------------------------------------------------------------- colors ----

struct RGB {
  uint8_t r, g, b;
};

struct Theme {
  RGB code, hex, data, net;     // base color of each pane
};

const Theme THEMES[] = {
  // code            hex              data             network
  {{60, 230, 110},  {70, 200, 235},  {240, 180, 60},  {190, 140, 255}},  // mixed
  {{60, 230, 110},  {60, 230, 110},  {60, 230, 110},  {60, 230, 110}},   // green
  {{240, 180, 60},  {240, 180, 60},  {240, 180, 60},  {240, 180, 60}},   // amber
  {{120, 190, 255}, {120, 190, 255}, {120, 190, 255}, {120, 190, 255}},  // ice blue
};
constexpr int THEME_COUNT = sizeof(THEMES) / sizeof(THEMES[0]);

// --------------------------------------------------------------- panes ----

// A generator writes one new line of fake text into `out` and says how it
// should be styled.
typedef Style (*Generator)(char* out, size_t size);

struct Pane {
  int x, y, w, h;           // where the pane is on screen, in pixels
  float baseSpeed;          // usual scroll speed, pixels per second
  Generator generate;

  int cols, rows;           // size of the text grid
  char text[MAX_ROWS][MAX_COLS + 1];
  Style style[MAX_ROWS];
  uint16_t colors[STYLE_COUNT];

  int top;                  // which row of text[] is currently the top line
  int offset;               // how many pixels of the top line have scrolled off
  float carry;              // leftover fraction of a pixel still to scroll
  float speed;              // current speed (changes now and then)
  uint32_t nextSpeedChange;
};

Style generateCode(char* out, size_t size);
Style generateHex(char* out, size_t size);
Style generateData(char* out, size_t size);

Pane panes[] = {
  //  x    y    w    h   speed  generator
  {   2, 121, 156, 119,  26,   generateCode},
  { 163,   0, 156, 118,  44,   generateHex},
  { 163, 121, 156, 119,  15,   generateData},
};
constexpr int PANE_COUNT = sizeof(panes) / sizeof(panes[0]);

// ------------------------------------------------------------- globals ----

TFT_eSPI tft = TFT_eSPI();

uint16_t stripBuffer[MAX_PANE_W * STRIP_H];
int themeIndex = 0;

// ------------------------------------------------- fake text: the code ----

const char* const NAME_START[] = {
  "buf", "node", "sys", "mem", "net", "key", "idx", "ptr", "reg",
  "pkt", "crc", "io", "tx", "rx", "cfg", "hash", "sig", "seq",
};
const char* const NAME_END[] = {
  "", "", "Len", "Ptr", "Id", "Mask", "Val", "Init", "Read",
  "Sync", "Lock", "Map", "Tab", "Ctl",
};

template <typename T, size_t N>
const T& pick(const T (&list)[N]) {
  return list[random(N)];
}

// Invent a plausible variable or function name, like "bufLen" or "netSync".
void makeName(char* out, size_t size) {
  snprintf(out, size, "%s%s", pick(NAME_START), pick(NAME_END));
}

Style generateCode(char* out, size_t size) {
  static int indent = 0;        // how deep inside { } we are

  char a[16], b[16], line[64];
  makeName(a, sizeof(a));
  makeName(b, sizeof(b));

  Style style = NORMAL;
  int depth = indent;           // indentation for THIS line
  int roll = random(100);

  if (indent == 0) {
    // Outside any function: start one, or write a top-level line.
    switch (random(6)) {
      case 0:
        snprintf(line, sizeof(line), "#define %s 0x%04X", a, (int)random(0x10000));
        break;
      case 1:
        snprintf(line, sizeof(line), "// %s %s", a, b);
        style = DIM;
        break;
      case 2:
        snprintf(line, sizeof(line), "static uint8_t %s[%d];", a, 8 << random(5));
        break;
      default:
        snprintf(line, sizeof(line), "%s %s(int %s) {",
                 random(2) ? "void" : "int", a, b);
        style = BRIGHT;
        indent = 1;
        break;
    }
  } else if (roll < 20) {
    // Close a block.
    indent--;
    depth = indent;
    snprintf(line, sizeof(line), "}");
  } else if (roll < 38 && indent < 4) {
    // Open a new block.
    switch (random(4)) {
      case 0:
        snprintf(line, sizeof(line), "for (i = 0; i < %s; i++) {", a);
        break;
      case 1:
        snprintf(line, sizeof(line), "if (%s(%s) != 0) {", a, b);
        break;
      case 2:
        snprintf(line, sizeof(line), "while (%s & 0x%02X) {", a, (int)random(256));
        break;
      default:
        snprintf(line, sizeof(line), "if (!%s) {", a);
        break;
    }
    indent++;
  } else {
    // An ordinary statement.
    switch (random(9)) {
      case 0:
        snprintf(line, sizeof(line), "%s = %s(%s, 0x%04X);", a, b, a, (int)random(0x10000));
        break;
      case 1:
        snprintf(line, sizeof(line), "%s[%d] ^= %s;", a, (int)random(64), b);
        break;
      case 2:
        snprintf(line, sizeof(line), "return %s;", a);
        break;
      case 3:
        snprintf(line, sizeof(line), "%s->%s = %d;", a, b, (int)random(1024));
        break;
      case 4:
        snprintf(line, sizeof(line), "// %s %s", a, b);
        style = DIM;
        break;
      case 5:
        snprintf(line, sizeof(line), "%s(&%s);", a, b);
        break;
      case 6:
        snprintf(line, sizeof(line), "%s += %s >> %d;", a, b, (int)random(1, 8));
        break;
      case 7:
        snprintf(line, sizeof(line), "uint8_t %s = 0x%02X;", a, (int)random(256));
        break;
      default:
        snprintf(line, sizeof(line), "%s |= (1 << %d);", a, (int)random(16));
        break;
    }
  }

  snprintf(out, size, "%*s%s", depth * 2, "", line);
  return style;
}

// --------------------------------------------- fake text: the hex dump ----

// One line of a memory dump: an address, eight bytes in hex, and the same
// eight bytes as characters (a dot for anything unprintable).
Style generateHex(char* out, size_t size) {
  static uint16_t address = 0x4000;

  bool empty = (random(9) == 0);      // now and then, a row of zeros
  char bytes[25], chars[9];

  for (int i = 0; i < 8; i++) {
    int value = empty ? 0 : random(256);
    snprintf(&bytes[i * 3], 4, "%02X ", value);
    chars[i] = (value >= 33 && value <= 126) ? (char)value : '.';
  }
  chars[8] = '\0';

  snprintf(out, size, "%04X: %s%s", address, bytes, chars);
  address += 8;
  return empty ? DIM : NORMAL;
}

// ------------------------------------------------ fake text: the data ----

const char* const UNIT[]  = {"NODE", "LINK", "CORE", "PORT", "GATE", "CELL"};
const char* const EVENT[] = {"SYNC OK", "ACK", "HANDSHAKE", "AUTH VALID",
                             "ROUTE SET", "CACHE HIT", "KEY ROTATE", "IDLE"};
const char* const ALARM[] = {"TEMP HIGH", "CRC FAIL", "TIMEOUT", "RETRY"};

Style generateData(char* out, size_t size) {
  static uint32_t clock = 43200;      // a pretend time of day, in seconds
  clock += random(1, 4);

  int hours = (clock / 3600) % 24;
  int minutes = (clock / 60) % 60;
  int seconds = clock % 60;

  switch (random(10)) {
    case 0: {                         // four columns of decimals
      int n = 0;
      for (int i = 0; i < 4 && n < (int)size; i++) {
        n += snprintf(out + n, size - n, "%3d.%03d  ",
                      (int)random(-99, 1000), (int)random(1000));
      }
      return NORMAL;
    }
    case 1: {                         // a row of binary
      int n = 0;
      for (int i = 0; i < 4 && n < (int)size; i++) {
        int value = random(256);
        for (int bit = 7; bit >= 0; bit--) {
          out[n++] = (value >> bit) & 1 ? '1' : '0';
        }
        out[n++] = ' ';
      }
      out[n] = '\0';
      return DIM;
    }
    case 2:                           // a position
      snprintf(out, size, "LAT %2d.%04d LON -%d.%04d ALT %d",
               (int)random(10, 60), (int)random(10000),
               (int)random(70, 125), (int)random(10000), (int)random(50, 900));
      return NORMAL;
    case 3: {                         // a progress bar
      int percent = random(101);
      char bar[21];
      for (int i = 0; i < 20; i++) bar[i] = (i * 5 < percent) ? '#' : '.';
      bar[20] = '\0';
      snprintf(out, size, "LOAD [%s] %3d%%", bar, percent);
      return BRIGHT;
    }
    case 4:                           // something going wrong
      snprintf(out, size, "[%02d:%02d:%02d] WARN %s %02d %s",
               hours, minutes, seconds, pick(UNIT), (int)random(32), pick(ALARM));
      return ALERT;
    default:                          // an ordinary log line
      snprintf(out, size, "[%02d:%02d:%02d] %s %02d %s 0x%04X",
               hours, minutes, seconds, pick(UNIT), (int)random(32),
               pick(EVENT), (int)random(0x10000));
      return NORMAL;
  }
}

// ------------------------------------------------------ the network map ----
//
// A pretend network. Nodes are dots, links are lines between them, and
// packets are bright specks that travel along the links. At random moments a
// new node joins (it flashes and links to its nearest neighbors) or an
// existing node drops out (it blinks red, then vanishes with its links).

constexpr int NET_X = 2;              // where the map is on screen
constexpr int NET_Y = 0;
constexpr int NET_W = 156;
constexpr int NET_H = 118;

constexpr int MAX_NODES   = 14;
constexpr int MIN_NODES   = 4;
constexpr int MAX_LINKS   = 32;
constexpr int MAX_PACKETS = 10;

constexpr uint32_t NET_FRAME_MS = 50;     // redraw the map 20 times a second
constexpr uint32_t JOIN_MS      = 600;    // how long a joining node flashes
constexpr uint32_t DROP_MS      = 800;    // how long a dropping node blinks
constexpr uint32_t LIT_MS       = 180;    // how long a node glows after a packet
constexpr float PACKET_SPEED    = 70;     // pixels per second
constexpr int NODE_SPACING      = 22;     // nodes keep at least this far apart

// The map is drawn into a small off-screen picture first, then sent to the
// screen all at once, so you never see it half-drawn. Each pixel of that
// picture is a NetColor number (defined near the top) rather than a full color.

struct Node {
  NodeState state;
  int x, y;
  uint8_t id;               // the two-digit label drawn beside it
  uint32_t stateStart;      // when it started joining or dropping
  uint32_t litUntil;        // glows until this time after receiving a packet
};

struct Link {
  bool used;
  uint8_t a, b;             // the two nodes it joins
};

struct Packet {
  bool used;
  uint8_t link;
  bool forward;             // true: travelling a -> b.  false: b -> a
  float progress;           // 0 at the start of the link, 1 at the end
};

Node nodes[MAX_NODES];
Link links[MAX_LINKS];
Packet packets[MAX_PACKETS];

uint8_t netCanvas[NET_W * NET_H];
uint16_t netColors[NET_COLOR_COUNT];
uint32_t packetsDelivered = 0;

// ---- the network: who is connected to whom ----

bool nodeActive(int i) {
  return nodes[i].state == JOINING || nodes[i].state == ONLINE;
}

int countNodes() {
  int count = 0;
  for (int i = 0; i < MAX_NODES; i++) {
    if (nodes[i].state != EMPTY) count++;
  }
  return count;
}

int countLinks() {
  int count = 0;
  for (int i = 0; i < MAX_LINKS; i++) {
    if (links[i].used) count++;
  }
  return count;
}

bool linked(int a, int b) {
  for (int i = 0; i < MAX_LINKS; i++) {
    if (!links[i].used) continue;
    if ((links[i].a == a && links[i].b == b) || (links[i].a == b && links[i].b == a)) {
      return true;
    }
  }
  return false;
}

int linkCount(int node) {
  int count = 0;
  for (int i = 0; i < MAX_LINKS; i++) {
    if (links[i].used && (links[i].a == node || links[i].b == node)) count++;
  }
  return count;
}

void addLink(int a, int b) {
  if (a < 0 || b < 0 || a == b || linked(a, b)) return;
  for (int i = 0; i < MAX_LINKS; i++) {
    if (!links[i].used) {
      links[i] = {true, (uint8_t)a, (uint8_t)b};
      return;
    }
  }
}

// The closest active node to `from` that it is not already linked to,
// or -1 if there isn't one.
int nearestUnlinked(int from) {
  int best = -1;
  long bestDistance = 0;
  for (int i = 0; i < MAX_NODES; i++) {
    if (i == from || !nodeActive(i) || linked(from, i)) continue;
    long dx = nodes[i].x - nodes[from].x;
    long dy = nodes[i].y - nodes[from].y;
    long distance = dx * dx + dy * dy;
    if (best < 0 || distance < bestDistance) {
      best = i;
      bestDistance = distance;
    }
  }
  return best;
}

// A new node joins the network. Returns its index, or -1 if there was no
// free slot or no free space on the map.
int addNode(uint32_t now) {
  int slot = -1;
  for (int i = 0; i < MAX_NODES; i++) {
    if (nodes[i].state == EMPTY) {
      slot = i;
      break;
    }
  }
  if (slot < 0) return -1;

  // Try a few random spots until one is clear of the other nodes.
  for (int attempt = 0; attempt < 30; attempt++) {
    int x = random(8, NET_W - 16);        // leave room for the label
    int y = random(14, NET_H - 8);        // and for the header line
    bool clear = true;
    for (int i = 0; i < MAX_NODES && clear; i++) {
      if (nodes[i].state == EMPTY) continue;
      long dx = nodes[i].x - x;
      long dy = nodes[i].y - y;
      if (dx * dx + dy * dy < NODE_SPACING * NODE_SPACING) clear = false;
    }
    if (!clear) continue;

    nodes[slot] = {JOINING, x, y, (uint8_t)random(256), now, 0};
    addLink(slot, nearestUnlinked(slot));
    if (random(2)) addLink(slot, nearestUnlinked(slot));   // sometimes two links
    return slot;
  }
  return -1;
}

// A node has finished dropping out: remove it, its links, and any packets
// that were on those links. Anyone left with no links reconnects.
void removeNode(int node) {
  nodes[node].state = EMPTY;

  for (int i = 0; i < MAX_LINKS; i++) {
    if (links[i].used && (links[i].a == node || links[i].b == node)) {
      links[i].used = false;
      for (int p = 0; p < MAX_PACKETS; p++) {
        if (packets[p].used && packets[p].link == i) packets[p].used = false;
      }
    }
  }

  for (int i = 0; i < MAX_NODES; i++) {
    if (nodeActive(i) && linkCount(i) == 0) {
      addLink(i, nearestUnlinked(i));     // the network "re-routes"
    }
  }
}

// Send a packet from `node` along one of its links, chosen at random.
void sendPacket(int node) {
  int chosen = -1, seen = 0;
  for (int i = 0; i < MAX_LINKS; i++) {
    if (links[i].used && (links[i].a == node || links[i].b == node)) {
      seen++;
      if (random(seen) == 0) chosen = i;  // each link equally likely
    }
  }
  if (chosen < 0) return;

  for (int p = 0; p < MAX_PACKETS; p++) {
    if (!packets[p].used) {
      packets[p] = {true, (uint8_t)chosen, links[chosen].a == node, 0};
      return;
    }
  }
}

// Move the whole network forward in time by `seconds`.
void updateNetwork(uint32_t now, float seconds) {
  static uint32_t nextEvent = 0;
  static uint32_t nextTraffic = 0;

  // Nodes finish joining or dropping.
  for (int i = 0; i < MAX_NODES; i++) {
    if (nodes[i].state == JOINING && now - nodes[i].stateStart >= JOIN_MS) {
      nodes[i].state = ONLINE;
    } else if (nodes[i].state == DROPPING && now - nodes[i].stateStart >= DROP_MS) {
      removeNode(i);
    }
  }

  // Every so often, a node joins or drops.
  if (now >= nextEvent) {
    nextEvent = now + random(500, 1800);
    int count = countNodes();
    bool join = (count <= MIN_NODES) || (count < MAX_NODES && random(100) < 55);
    if (join) {
      addNode(now);
    } else {
      int victim = random(MAX_NODES);
      if (nodes[victim].state == ONLINE) {
        nodes[victim].state = DROPPING;
        nodes[victim].stateStart = now;
      }
    }
  }

  // A steady trickle of new packets.
  if (now >= nextTraffic) {
    nextTraffic = now + random(120, 500);
    int sender = random(MAX_NODES);
    if (nodes[sender].state == ONLINE) sendPacket(sender);
  }

  // Packets travel along their links.
  for (int p = 0; p < MAX_PACKETS; p++) {
    if (!packets[p].used) continue;
    const Link& link = links[packets[p].link];
    float dx = nodes[link.b].x - nodes[link.a].x;
    float dy = nodes[link.b].y - nodes[link.a].y;
    float length = sqrtf(dx * dx + dy * dy);
    if (length < 1) length = 1;

    packets[p].progress += PACKET_SPEED * seconds / length;
    if (packets[p].progress >= 1) {
      int arrivedAt = packets[p].forward ? link.b : link.a;
      packets[p].used = false;
      packetsDelivered++;
      nodes[arrivedAt].litUntil = now + LIT_MS;
      if (nodes[arrivedAt].state == ONLINE && random(100) < 55) {
        sendPacket(arrivedAt);            // pass it on
      }
    }
  }
}

// ---- the network: drawing ----

void plot(int x, int y, NetColor color) {
  if (x < 0 || y < 0 || x >= NET_W || y >= NET_H) return;
  netCanvas[y * NET_W + x] = color;
}

// A straight line, one pixel at a time (Bresenham's algorithm).
void canvasLine(int x0, int y0, int x1, int y1, NetColor color) {
  int dx = abs(x1 - x0), dy = -abs(y1 - y0);
  int stepX = (x0 < x1) ? 1 : -1;
  int stepY = (y0 < y1) ? 1 : -1;
  int error = dx + dy;
  while (true) {
    plot(x0, y0, color);
    if (x0 == x1 && y0 == y1) break;
    int twice = 2 * error;
    if (twice >= dy) { error += dy; x0 += stepX; }
    if (twice <= dx) { error += dx; y0 += stepY; }
  }
}

// A circle. Filled if `inner` is 0; otherwise a ring from `inner` to `radius`.
void canvasCircle(int cx, int cy, int radius, int inner, NetColor color) {
  for (int y = -radius; y <= radius; y++) {
    for (int x = -radius; x <= radius; x++) {
      int distance = x * x + y * y;
      if (distance <= radius * radius && distance >= inner * inner) {
        plot(cx + x, cy + y, color);
      }
    }
  }
}

void canvasText(int x, int y, const char* text, NetColor color) {
  for (; *text; text++, x += CELL_W) {
    for (int row = 0; row < GLYPH_H; row++) {
      int dots = glyphRow(*text, row);
      if (dots & 0b100) plot(x, y + row, color);
      if (dots & 0b010) plot(x + 1, y + row, color);
      if (dots & 0b001) plot(x + 2, y + row, color);
    }
  }
}

void drawNetwork(uint32_t now) {
  memset(netCanvas, NET_BLACK, sizeof(netCanvas));
  bool blink = (now / 100) % 2;

  // Links first, so nodes are drawn on top of them.
  for (int i = 0; i < MAX_LINKS; i++) {
    if (!links[i].used) continue;
    const Node& a = nodes[links[i].a];
    const Node& b = nodes[links[i].b];
    NetColor color = NET_LINK;
    if (a.state == DROPPING || b.state == DROPPING) {
      color = blink ? NET_ALERT : NET_BLACK;
    } else if (a.state == JOINING || b.state == JOINING) {
      color = NET_BRIGHT;
    }
    canvasLine(a.x, a.y, b.x, b.y, color);
  }

  for (int p = 0; p < MAX_PACKETS; p++) {
    if (!packets[p].used) continue;
    const Link& link = links[packets[p].link];
    const Node& from = packets[p].forward ? nodes[link.a] : nodes[link.b];
    const Node& to   = packets[p].forward ? nodes[link.b] : nodes[link.a];
    int x = from.x + (int)((to.x - from.x) * packets[p].progress);
    int y = from.y + (int)((to.y - from.y) * packets[p].progress);
    canvasCircle(x, y, 1, 0, NET_BRIGHT);
  }

  for (int i = 0; i < MAX_NODES; i++) {
    const Node& node = nodes[i];
    if (node.state == EMPTY) continue;

    char label[4];
    snprintf(label, sizeof(label), "%02X", node.id);
    NetColor labelColor = NET_LINK;

    if (node.state == JOINING) {
      // A ring that spreads outward as the node comes online.
      int ring = 3 + (int)((now - node.stateStart) * 6 / JOIN_MS);
      canvasCircle(node.x, node.y, ring, ring - 1, NET_BRIGHT);
      canvasCircle(node.x, node.y, 2, 0, NET_BRIGHT);
      labelColor = NET_BRIGHT;
    } else if (node.state == DROPPING) {
      if (blink) canvasCircle(node.x, node.y, 3, 0, NET_ALERT);
      labelColor = NET_ALERT;
    } else if (now < node.litUntil) {
      canvasCircle(node.x, node.y, 3, 0, NET_BRIGHT);   // just got a packet
    } else {
      canvasCircle(node.x, node.y, 2, 0, NET_NODE);
    }
    canvasText(node.x + 5, node.y - 2, label, labelColor);
  }

  char header[40];
  snprintf(header, sizeof(header), "NODES %02d  LINKS %02d  PKT %05lu",
           countNodes(), countLinks(), (unsigned long)(packetsDelivered % 100000));
  canvasText(1, 1, header, NET_NODE);

  // Turn the color numbers into real colors and send the picture.
  for (int stripY = 0; stripY < NET_H; stripY += STRIP_H) {
    int stripH = NET_H - stripY;
    if (stripH > STRIP_H) stripH = STRIP_H;
    const uint8_t* in = &netCanvas[stripY * NET_W];
    for (int i = 0; i < stripH * NET_W; i++) {
      stripBuffer[i] = netColors[in[i]];
    }
    tft.pushImage(NET_X, NET_Y + stripY, NET_W, stripH, stripBuffer);
  }
}

// ------------------------------------------------------- pane plumbing ----

// Put a freshly generated line into row `row` of the pane's text grid.
// The row is padded with spaces so every row is exactly `cols` long.
void fillRow(Pane& pane, int row) {
  char line[80];
  line[0] = '\0';
  pane.style[row] = pane.generate(line, sizeof(line));

  int length = strlen(line);
  for (int col = 0; col < pane.cols; col++) {
    pane.text[row][col] = (col < length) ? line[col] : ' ';
  }
  pane.text[row][pane.cols] = '\0';
}

// Work out the four style colors for a pane from its base color.
void setPaneColor(Pane& pane, RGB base) {
  pane.colors[NORMAL] = tft.color565(base.r, base.g, base.b);
  pane.colors[DIM]    = tft.color565(base.r * 45 / 100, base.g * 45 / 100, base.b * 45 / 100);
  pane.colors[BRIGHT] = tft.color565((base.r + 255 * 2) / 3, (base.g + 255 * 2) / 3,
                                     (base.b + 255 * 2) / 3);
  pane.colors[ALERT]  = tft.color565(255, 70, 50);
}

// Draw the whole pane.
//
// We do not ask the display to draw letters. Instead we build the picture
// ourselves, one horizontal line of pixels (a "scanline") at a time, and send
// it in chunks. For each scanline we work out which line of text it crosses
// and which of that text's 6 pixel rows it is, then copy the font's dots.
// Scrolling by one pixel is just starting that calculation one pixel later.
void drawPane(const Pane& pane) {
  for (int stripY = 0; stripY < pane.h; stripY += STRIP_H) {
    int stripH = pane.h - stripY;
    if (stripH > STRIP_H) stripH = STRIP_H;

    for (int line = 0; line < stripH; line++) {
      uint16_t* out = &stripBuffer[line * pane.w];
      memset(out, 0, pane.w * sizeof(uint16_t));       // start all black

      int contentY = stripY + line + pane.offset;      // position in the text
      int glyphY = contentY % CELL_H;                  // row within the cell
      if (glyphY >= GLYPH_H) continue;                 // the blank spacing row

      int row = (pane.top + contentY / CELL_H) % pane.rows;
      const char* text = pane.text[row];
      uint16_t color = pane.colors[pane.style[row]];

      for (int col = 0; col < pane.cols; col++, out += CELL_W) {
        int dots = glyphRow(text[col], glyphY);
        if (dots & 0b100) out[0] = color;
        if (dots & 0b010) out[1] = color;
        if (dots & 0b001) out[2] = color;
      }
    }
    tft.pushImage(pane.x, pane.y + stripY, pane.w, stripH, stripBuffer);
  }
}

// Now and then each pane changes pace: usually near its normal speed,
// sometimes a short pause, sometimes a burst. Real output is never steady.
void chooseSpeed(Pane& pane, uint32_t now) {
  int roll = random(100);
  if (roll < 12) {
    pane.speed = 0;
    pane.nextSpeedChange = now + random(300, 900);
  } else if (roll < 30) {
    pane.speed = pane.baseSpeed * 2.5f;
    pane.nextSpeedChange = now + random(500, 1500);
  } else {
    pane.speed = pane.baseSpeed * random(70, 131) / 100.0f;
    pane.nextSpeedChange = now + random(1500, 5000);
  }
}

// Scroll a pane by however far it should have moved in `seconds`.
void updatePane(Pane& pane, uint32_t now, float seconds) {
  if (now >= pane.nextSpeedChange) {
    chooseSpeed(pane, now);
  }

  pane.carry += pane.speed * seconds;
  int pixels = (int)pane.carry;       // whole pixels to move this time
  if (pixels == 0) return;
  pane.carry -= pixels;

  pane.offset += pixels;
  while (pane.offset >= CELL_H) {
    // The top line has scrolled completely off. Reuse its row for a brand
    // new line at the bottom -- this is what makes the scroll endless.
    pane.offset -= CELL_H;
    fillRow(pane, pane.top);
    pane.top = (pane.top + 1) % pane.rows;
  }
  drawPane(pane);
}

// ------------------------------------------------------------- themes ----

void applyTheme() {
  const Theme& theme = THEMES[themeIndex];
  setPaneColor(panes[0], theme.code);
  setPaneColor(panes[1], theme.hex);
  setPaneColor(panes[2], theme.data);

  netColors[NET_BLACK]  = TFT_BLACK;
  netColors[NET_LINK]   = tft.color565(theme.net.r * 45 / 100, theme.net.g * 45 / 100,
                                       theme.net.b * 45 / 100);
  netColors[NET_NODE]   = tft.color565(theme.net.r, theme.net.g, theme.net.b);
  netColors[NET_BRIGHT] = tft.color565(255, 255, 255);
  netColors[NET_ALERT]  = tft.color565(255, 70, 50);

  // Thin divider lines between the four panes.
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(160, 0, 1, 240, panes[0].colors[DIM]);
  tft.fillRect(0, 119, 320, 1, panes[0].colors[DIM]);

  drawNetwork(millis());

  for (int i = 0; i < PANE_COUNT; i++) {
    drawPane(panes[i]);
  }
}

// True once each time the BOOT button goes down.
bool buttonPressed() {
  static bool wasDown = false;
  static uint32_t lastChange = 0;

  bool down = (digitalRead(PIN_BOOT) == LOW);
  uint32_t now = millis();
  if (down != wasDown && now - lastChange > 40) {   // ignore contact bounce
    wasDown = down;
    lastChange = now;
    return down;
  }
  return false;
}

// ---------------------------------------------------------- setup / loop ----

void setup() {
  Serial.begin(115200);

  pinMode(PIN_BACKLIGHT, OUTPUT);
  digitalWrite(PIN_BACKLIGHT, HIGH);
  pinMode(PIN_BOOT, INPUT_PULLUP);

  randomSeed(esp_random());           // different nonsense every power-up

  tft.init();
  tft.setRotation(1);                 // landscape, 320 x 240
  tft.setSwapBytes(true);             // so pushImage() colors come out right

  for (int i = 0; i < PANE_COUNT; i++) {
    Pane& pane = panes[i];
    pane.cols = pane.w / CELL_W;
    pane.rows = pane.h / CELL_H + 2;
    pane.top = 0;
    pane.offset = 0;
    pane.carry = 0;
    pane.speed = pane.baseSpeed;
    pane.nextSpeedChange = 0;
    for (int row = 0; row < pane.rows; row++) {
      fillRow(pane, row);             // start with a full screen of text
    }
  }

  // Start the network with a handful of nodes already online.
  for (int i = 0; i < 7; i++) {
    int node = addNode(0);
    if (node >= 0) nodes[node].state = ONLINE;
  }

  applyTheme();
  Serial.println("MovieTerminal ready");
}

void loop() {
  static uint32_t lastFrame = millis();
  static uint32_t lastNetworkDraw = 0;

  uint32_t now = millis();
  float seconds = (now - lastFrame) / 1000.0f;
  lastFrame = now;
  if (seconds > 0.1f) seconds = 0.1f;   // never jump a long way in one go

  if (buttonPressed()) {
    themeIndex = (themeIndex + 1) % THEME_COUNT;
    applyTheme();
  }

  for (int i = 0; i < PANE_COUNT; i++) {
    updatePane(panes[i], now, seconds);
  }

  updateNetwork(now, seconds);
  if (now - lastNetworkDraw >= NET_FRAME_MS) {
    lastNetworkDraw = now;
    drawNetwork(now);
  }
}
