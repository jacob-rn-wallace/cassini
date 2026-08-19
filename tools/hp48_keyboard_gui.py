#!/usr/bin/env python3
"""Clickable HP48GX keyboard GUI for the Cassini replica.

Lets you drive the emulator with a mouse instead of hand-typing wire
protocol bytes over serial. Displays HP48GXkeyboard.jpg - a real photo
of an HP48GX keyboard, cropped to just the keyboard - and overlays an
invisible clickable rectangle over each of the 49 physical keys.
Structurally modeled on soynut's own tools/hp41_keyboard_gui.py (this
project's structural template - see the root CLAUDE.md), but the
protocol here is simpler: saturn_core's KeybPress()/KeybRelease() (see
firmware/main.c's PollKeyboardInput()) persist real state in the
emulator's own keyboard matrix between calls, so a mouse-down/mouse-up
pair maps directly to a press/release pair with no threshold/hold
detection needed - unlike soynut's HP-41 bridge, which had to build
that because of how HP-41 `dokey()` polling works.

KEY_MAP's pixel coordinates were derived the same diligent way
soynut's own image-based hit-boxes were: a gridded overlay of the
photo (50px minor lines, 200px major lines with coordinate labels) was
used to read off each key's real edges directly, then every computed
hit-box was rendered back onto the full photo and visually confirmed
to land tightly on its own key with no overlaps across all 49 keys -
not eyeballed guesses. The keyboard has two real column pitches: a
tighter 280px pitch across the top 5 rows (the A-F softkeys through
the ENTER row, 6 columns), and a wider ~350px pitch across the bottom
4 rows (the digit/operator block, 5 columns) - both pitches, and the
row centers, were read directly off the gridded photo, then used to
compute every key's hit-box arithmetically.

Keycodes are saturn_core's own real HP48SX/GX hardware keycodes,
copied directly from saturn_core/src/emulator_api.c's keyboard48[]
table (the vendored core's reference embedder, itself never included
by this project - see firmware/saturn_lcd.c's LCD decode for the same
"copy the reference values, don't include the file" precedent). The ON
key's real code, 0x8000, doesn't fit the wire protocol's one-byte
keycode field - see WIRE_ON_BYTE below for how that's handled.

KEYBOARD_IMAGE's license: a photograph by Clemens Pfeiffer (uploaded
by Panoramafotos.net), via Wikimedia Commons -
https://commons.wikimedia.org/wiki/File:HP48GX.jpg - licensed CC BY-SA
3.0. The copy in this repo is further cropped to just the keyboard
region by this project's own user; redistributing it (or this repo)
must keep this attribution and the CC BY-SA 3.0 license per its terms.

Usage:
    python3 tools/hp48_keyboard_gui.py [--port /dev/cu.usbmodemXXXX]

If --port is omitted, tries to auto-detect a single plausible USB
serial port.

The same serial connection carries both directions: keys sent here,
and main.c's own debug output (boot log, keypress echoes) read back
and shown in the log pane - so this doubles as the debug console you'd
otherwise use `screen`/`pyserial` directly for.
"""
from __future__ import annotations

import argparse
import queue
import sys
import threading
import tkinter as tk
from pathlib import Path

import serial
import serial.tools.list_ports
from PIL import Image, ImageTk

REPO_ROOT = Path(__file__).resolve().parent.parent
KEYBOARD_IMAGE = REPO_ROOT / "HP48GXkeyboard.jpg"

BAUD_RATE = 115200  # matches firmware/main.c's stdio_init_all() USB CDC.

# Power of 10 (Python adaptation), Rule 3: the serial log pane below is
# the one place this GUI could grow without bound during a long
# session (every line main.c prints gets appended, forever) - capped
# rather than excepted, same posture soynut's own GUI takes.
MAX_LOG_LINES = 2000

# firmware/main.c's PollKeyboardInput() reserves this one-byte wire
# value to mean the ON key, whose real saturn_core keycode (0x8000)
# doesn't fit a single byte - see that function's own doc comment.
WIRE_ON_BYTE = 0xFF
ON_KEYCODE = 0x8000


def check(condition: bool, message: str) -> None:
    """Power of 10 (Python adaptation), Rule 5 assertion helper - never compiled out."""
    if not condition:
        raise AssertionError(message)


# Displayed at this fraction of the source image's native resolution
# (1727x2474) - keeps the window a reasonable size on a laptop screen.
# Click coordinates are scaled by the same factor at hit-test time.
DISPLAY_SCALE = 0.3

# (label, center_x, center_y, half_width, half_height, keycode) - all
# in NATIVE image pixel coordinates (pre-DISPLAY_SCALE). keycode is
# saturn_core's own real HP48SX/GX keycode (keyboard48[] in
# emulator_api.c), except ON_KEYCODE which PollKeyboardInput()
# recognizes via WIRE_ON_BYTE instead of a literal byte - see this
# module's own docstring for how these coordinates were derived.
KEY_MAP: list[tuple[str, int, int, int, int, int]] = [
    # Row 1 - softkeys under the LCD's menu labels
    ("A", 155, 95, 90, 68, 0x14),
    ("B", 435, 95, 90, 68, 0x84),
    ("C", 715, 95, 90, 68, 0x83),
    ("D", 995, 95, 90, 68, 0x82),
    ("E", 1275, 95, 90, 68, 0x81),
    ("F", 1555, 95, 90, 68, 0x80),

    # Row 2
    ("MTH", 155, 395, 90, 68, 0x24),
    ("PRG", 435, 395, 90, 68, 0x74),
    ("CST", 715, 395, 90, 68, 0x73),
    ("VAR", 995, 395, 90, 68, 0x72),
    ("UP", 1275, 395, 90, 68, 0x71),
    ("NXT", 1555, 395, 90, 68, 0x70),

    # Row 3
    ("HOME", 155, 663, 90, 68, 0x04),
    ("STO", 435, 663, 90, 68, 0x64),
    ("EVAL", 715, 663, 90, 68, 0x63),
    ("LEFT", 995, 663, 90, 68, 0x62),
    ("DOWN", 1275, 663, 90, 68, 0x61),
    ("RIGHT", 1555, 663, 90, 68, 0x60),

    # Row 4
    ("SIN", 155, 955, 90, 68, 0x34),
    ("COS", 435, 955, 90, 68, 0x54),
    ("TAN", 715, 955, 90, 68, 0x53),
    ("SQRT", 995, 955, 90, 68, 0x52),
    ("YX", 1275, 955, 90, 68, 0x51),
    ("INV", 1555, 955, 90, 68, 0x50),

    # Row 5 - ENTER spans the physical width of columns 1-2
    ("ENTER", 295, 1235, 200, 68, 0x44),
    ("PLUSMINUS", 715, 1235, 90, 68, 0x43),
    ("EEX", 995, 1235, 90, 68, 0x42),
    ("DEL", 1275, 1235, 90, 68, 0x41),
    ("BACKSPACE", 1555, 1235, 90, 68, 0x40),

    # Row 6
    ("ALPHA", 155, 1488, 130, 72, 0x35),
    ("N7", 505, 1488, 130, 72, 0x33),
    ("N8", 855, 1488, 130, 72, 0x32),
    ("N9", 1205, 1488, 130, 72, 0x31),
    ("DIV", 1555, 1488, 130, 72, 0x30),

    # Row 7
    ("LSHIFT", 155, 1781, 130, 72, 0x25),
    ("N4", 505, 1781, 130, 72, 0x23),
    ("N5", 855, 1781, 130, 72, 0x22),
    ("N6", 1205, 1781, 130, 72, 0x21),
    ("MUL", 1555, 1781, 130, 72, 0x20),

    # Row 8
    ("RSHIFT", 155, 2074, 130, 72, 0x15),
    ("N1", 505, 2074, 130, 72, 0x13),
    ("N2", 855, 2074, 130, 72, 0x12),
    ("N3", 1205, 2074, 130, 72, 0x11),
    ("MINUS", 1555, 2074, 130, 72, 0x10),

    # Row 9
    ("ON", 155, 2367, 130, 72, ON_KEYCODE),
    ("N0", 505, 2367, 130, 72, 0x03),
    ("DECIMAL", 855, 2367, 130, 72, 0x02),
    ("SPC", 1205, 2367, 130, 72, 0x01),
    ("PLUS", 1555, 2367, 130, 72, 0x00),
]
check(len(KEY_MAP) == 49, f"expected 49 HP48SX/GX keys, got {len(KEY_MAP)}")


def _wire_bytes( command: bytes, keycode: int ) -> bytes:
    """Build a 3-byte wire message: command ('P'/'R') + 2 hex digits.

    Args:
        command: b"P" or b"R".
        keycode: A KEY_MAP entry's keycode (ON_KEYCODE or a real 0x00-0xBF code).

    Returns:
        The 3 raw bytes firmware/main.c's PollKeyboardInput() expects.
    """
    wire_byte = WIRE_ON_BYTE if keycode == ON_KEYCODE else keycode
    check(0 <= wire_byte <= 0xFF, f"keycode {keycode:#x} doesn't fit one wire byte")
    return command + f"{wire_byte:02X}".encode("ascii")


def find_port() -> str:
    """Auto-detect the Pico's USB serial port.

    Returns:
        The single plausible port's device path.

    Raises:
        SystemExit: if zero or more than one plausible port is found;
            prints the available ports first so the user can pass
            --port explicitly.
    """
    ports = list(serial.tools.list_ports.comports())
    candidates = [p for p in ports if "usbmodem" in p.device.lower()]
    if len(candidates) == 1:
        return candidates[0].device
    print("Could not auto-detect the Pico's serial port.")
    if ports:
        print("Available ports:")
        for p in ports:
            print(f"  {p.device}  ({p.description})")
    print("Pass one explicitly with --port.")
    sys.exit(1)


class SerialLink:
    """Owns the serial connection.

    Writes keys from the GUI thread, reads lines in a background
    thread and hands them to the GUI via a queue (Tkinter isn't
    thread-safe - the main loop polls the queue instead of touching
    widgets from this thread directly).
    """

    def __init__(self, port: str) -> None:
        """Open the serial port and start the background read thread.

        Args:
            port: Device path for the Pico's USB serial port.
        """
        self.ser = serial.Serial(port, BAUD_RATE, timeout=0.2)
        self.line_queue: queue.Queue[str] = queue.Queue()
        self._stop = threading.Event()
        self._thread = threading.Thread(target=self._read_loop, daemon=True)
        self._thread.start()

    def _read_loop(self) -> None:
        """Background thread body: read bytes, split on newlines, queue lines."""
        buf = b""
        while not self._stop.is_set():
            try:
                chunk = self.ser.read(256)
            except serial.SerialException:
                break
            if not chunk:
                continue
            buf += chunk
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                self.line_queue.put(line.decode(errors="replace").rstrip("\r"))

    def send(self, data: bytes) -> None:
        """Write bytes to the serial port and flush immediately.

        Args:
            data: Raw wire protocol bytes, e.g. b"P14".
        """
        check(len(data) > 0, "SerialLink.send() called with empty data")
        self.ser.write(data)
        self.ser.flush()

    def close(self) -> None:
        """Stop the read thread and close the serial port."""
        self._stop.set()
        self.ser.close()


class KeyboardApp:
    """The clickable keyboard window: photo canvas and serial log pane."""

    def __init__(self, root: tk.Tk, link: SerialLink) -> None:
        """Build the keyboard window: canvas, side panel, and serial polling.

        Args:
            root: The Tk root window to build the UI into.
            link: An already-open SerialLink to send keys through and
                read the debug log from.
        """
        self.link = link
        root.title("HP48GX Keyboard (Cassini)")

        image = Image.open(KEYBOARD_IMAGE)
        disp_w = int(image.width * DISPLAY_SCALE)
        disp_h = int(image.height * DISPLAY_SCALE)
        self.tk_image = ImageTk.PhotoImage(image.resize((disp_w, disp_h)))

        main = tk.Frame(root)
        main.pack(fill=tk.BOTH, expand=True)

        self.canvas = self._build_canvas(main, disp_w, disp_h)
        self.status, self.log = self._build_side_panel(main)

        root.after(50, self._poll_serial)

    def _build_canvas(self, main: tk.Frame, disp_w: int, disp_h: int) -> tk.Canvas:
        """Build the keyboard photo canvas and bind every KEY_MAP entry's clickable rectangle.

        Split out of __init__ to keep that one under Rule 4's ~60-line target.
        """
        canvas = tk.Canvas(main, width=disp_w, height=disp_h, highlightthickness=0)
        canvas.pack(side=tk.LEFT, fill=tk.BOTH)
        canvas.create_image(0, 0, anchor=tk.NW, image=self.tk_image)

        # No <Enter>/<Leave> hover-highlight bindings - see soynut's
        # own hp41_keyboard_gui.py for the macOS Aqua Tk feedback-loop
        # quirk this avoids. <Button-1>/<ButtonRelease-1> don't share
        # that failure mode.
        for label, cx, cy, hw, hh, keycode in KEY_MAP:
            x0, y0 = (cx - hw) * DISPLAY_SCALE, (cy - hh) * DISPLAY_SCALE
            x1, y1 = (cx + hw) * DISPLAY_SCALE, (cy + hh) * DISPLAY_SCALE
            rect = canvas.create_rectangle(x0, y0, x1, y1, outline="", fill="", width=2)

            def on_press(
                _event: tk.Event, label: str = label, keycode: int = keycode, rect: int = rect,
            ) -> None:
                self._on_press(label, keycode, rect)

            def on_release(
                _event: tk.Event, label: str = label, keycode: int = keycode, rect: int = rect,
            ) -> None:
                self._on_release(label, keycode, rect)

            canvas.tag_bind(rect, "<Button-1>", on_press)
            canvas.tag_bind(rect, "<ButtonRelease-1>", on_release)
        return canvas

    def _build_side_panel(self, main: tk.Frame) -> tuple[tk.Label, tk.Text]:
        """Build the status line and serial log pane.

        Split out of __init__ alongside _build_canvas() for the same Rule 4 reason.
        """
        side = tk.Frame(main)
        side.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        status = tk.Label(side, text="ready", anchor="w")
        status.pack(fill=tk.X)

        log = tk.Text(side, width=60, bg="black", fg="#33ff33",
                       font=("Menlo", 10), state=tk.DISABLED)
        log.pack(fill=tk.BOTH, expand=True)
        return status, log

    def _on_press(self, label: str, keycode: int, rect_id: int) -> None:
        """Handle mouse-down: flash the key and send an immediate press.

        Args:
            label: Human-readable key name, for the status line.
            keycode: This key's saturn_core keycode.
            rect_id: Canvas item id of this key's clickable rectangle.
        """
        self.canvas.itemconfig(rect_id, outline="#ff3333")
        try:
            self.link.send(_wire_bytes(b"P", keycode))
        except serial.SerialException as e:
            self.status.config(text=f"send failed: {e}")
            return
        self.status.config(text=f"pressed {label} (0x{keycode:04X})")

    def _on_release(self, label: str, keycode: int, rect_id: int) -> None:
        """Handle mouse-up: clear the flash and send an immediate release.

        Args:
            label: Human-readable key name, for the status line.
            keycode: This key's saturn_core keycode.
            rect_id: Canvas item id of this key's clickable rectangle.
        """
        self.canvas.itemconfig(rect_id, outline="")
        try:
            self.link.send(_wire_bytes(b"R", keycode))
        except serial.SerialException as e:
            self.status.config(text=f"send failed: {e}")
            return
        self.status.config(text=f"released {label}")

    def _append_log(self, line: str) -> None:
        """Append one line to the log pane, trimming from the top past MAX_LOG_LINES.

        Args:
            line: The log line to append (no trailing newline needed).
        """
        self.log.config(state=tk.NORMAL)
        self.log.insert(tk.END, line + "\n")
        line_count = int(self.log.index("end-1c").split(".")[0])
        if line_count > MAX_LOG_LINES:
            overflow = line_count - MAX_LOG_LINES
            self.log.delete("1.0", f"{overflow + 1}.0")
        self.log.see(tk.END)
        self.log.config(state=tk.DISABLED)

    def _poll_serial(self) -> None:
        """Drain any queued serial log lines into the log pane, then reschedule itself."""
        try:
            while True:
                line = self.link.line_queue.get_nowait()
                self._append_log(line)
        except queue.Empty:
            pass
        self.canvas.after(50, self._poll_serial)


def main() -> None:
    """Parse args, connect to the Pico, and run the Tk event loop until the window closes."""
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", help="Pico's USB serial port (auto-detected if omitted)")
    args = parser.parse_args()

    port = args.port or find_port()
    print(f"Connecting to {port} @ {BAUD_RATE} baud...")
    link = SerialLink(port)

    root = tk.Tk()
    KeyboardApp(root, link)
    try:
        root.mainloop()
    finally:
        link.close()


if __name__ == "__main__":
    main()
