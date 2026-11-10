# Movie Terminal

"Computer stuff" for the background of a movie scene. The screen is split into
four panes — an animated network map and three panes of tiny text that scroll
forever:

| Pane | What it shows |
|---|---|
| Top left | A network map: nodes join, drop out, and pass packets to each other |
| Bottom left | Made-up source code, with indentation, braces, and comments |
| Top right | A hex dump: address, eight bytes, and the same bytes as characters |
| Bottom right | Log lines, columns of numbers, binary, coordinates, progress bars, and the occasional red warning |

None of it means anything. It just has to look busy. Each text pane scrolls at
its own speed, and now and then pauses or bursts, the way real output does.

On the network map, a new node flashes white with a spreading ring and links
to its nearest neighbors. A node that drops out blinks red, then disappears
along with its links; any node left stranded reconnects to its nearest
neighbor. White specks are packets travelling along the links, and a node
glows for a moment when one arrives.

Press the **BOOT** button to change the color scheme: green / cyan / amber,
all green, all amber, or ice blue.

---

## Running It

1. Complete [the CYD setup guide](../../docs/CYD-SETUP.md) first.
2. Open `MovieTerminal.ino` in Arduino IDE. `TinyFont.h` opens as a second tab.
   (Pasting only the `.ino` into a new sketch will fail — it needs both files.)
3. Board: **ESP32 Dev Module**, Upload Speed: **115200**, Port: your
   `/dev/cu.usbserial-*` device.
4. Upload.

Only TFT_eSPI is needed. The sketch does not use the touchscreen or any of
TFT_eSPI's fonts.

---

## How It Works

**The font.** `TinyFont.h` holds a complete font where every character is just
3 pixels wide and 5 tall, in a 4 × 6 cell. That fits 80 characters across the
screen and 40 lines down. Each character is written out as five rows of binary
you can read by eye — try redesigning a letter.

**The fake text.** Three *generator* functions each invent one line at a time:

- `generateCode()` keeps track of how deep it is inside `{ }` so the
  indentation always makes sense, and builds names like `bufLen` or `netSync`
  by gluing a random start to a random ending.
- `generateHex()` counts an address upward, eight bytes per line.
- `generateData()` picks one of several line shapes and keeps a pretend clock
  ticking for the timestamps.

**Endless scrolling.** Each pane keeps a grid of text lines. When the top line
has scrolled completely off the screen, its row is reused for a brand-new line
at the bottom. The grid is a *ring*: nothing is ever moved, we just change
which row counts as the top.

**Smooth scrolling.** The sketch does not ask the display to draw letters. It
builds each pane's picture itself, one horizontal line of pixels at a time,
and sends that to the screen. Scrolling up by one pixel is just starting the
picture one pixel further down the text — so the text glides instead of
jumping a whole line at a time.

**The network map.** The network is three small lists: nodes, links (each
joins two nodes), and packets (each rides along one link). `updateNetwork()`
moves time forward — nodes finish joining or dropping, packets advance, and
every second or so a random node joins or leaves. `drawNetwork()` then draws
the whole map into an off-screen picture and sends it to the screen in one go,
20 times a second, so you never see it half-drawn. The lines are drawn with
Bresenham's algorithm, a classic way to draw a straight line using only whole
numbers.

---

## Ideas To Try

- Change `MAX_NODES`, `PACKET_SPEED`, or how often nodes join and drop.
- Make busy nodes (lots of links) draw bigger.

- Add your own words to `NAME_START`, `UNIT`, or `EVENT`.
- Add a new kind of line to `generateData()`.
- Add a color scheme to `THEMES`.
- Change a pane's speed, or its size and position, in the `panes[]` table.
- Write a fourth generator and give it its own pane.
