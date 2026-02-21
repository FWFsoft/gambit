#!/usr/bin/env python3
"""Playwright-based visual test runner for Gambit WASM client.

Controls the WASM client in headless Chromium, reads commands from test_input.txt,
dispatches keyboard/mouse input via Playwright, and captures periodic screenshots.
"""

import argparse
import asyncio
import os
import signal
import sys
import time

from playwright.async_api import async_playwright

KEY_MAP = {
    "W": "w",
    "A": "a",
    "S": "s",
    "D": "d",
    "E": "e",
    "I": "i",
    "F1": "F1",
    "F2": "F2",
    "F3": "F3",
    "Up": "ArrowUp",
    "Down": "ArrowDown",
    "Left": "ArrowLeft",
    "Right": "ArrowRight",
    "Space": " ",
    "Enter": "Enter",
    "Escape": "Escape",
    "Tab": "Tab",
}


class CommandReader:
    """Reads commands from test_input.txt, tracking position across polls."""

    def __init__(self, path: str):
        self.path = path
        self.line_pos = 0

    def poll(self) -> list[str]:
        """Return new commands since last poll."""
        if not os.path.exists(self.path):
            return []
        with open(self.path, "r") as f:
            lines = f.readlines()
        new_lines = lines[self.line_pos :]
        self.line_pos = len(lines)
        commands = []
        for line in new_lines:
            stripped = line.strip()
            if stripped and not stripped.startswith("#"):
                commands.append(stripped)
        return commands


async def wait_for_game_load(page, timeout_s: int = 60):
    """Wait for the WASM module to finish loading.

    The #status element goes from loading text to empty string when ready.
    """
    print("Waiting for canvas element...")
    await page.wait_for_selector("#canvas", timeout=timeout_s * 1000)
    print("Canvas found. Waiting for WASM module to load...")

    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        status_el = await page.query_selector("#status")
        if status_el:
            text = (await status_el.text_content()) or ""
            text = text.strip()
            if text:
                print(f"  Status: {text}")
            else:
                print("WASM module loaded.")
                return
        await asyncio.sleep(0.5)

    raise TimeoutError(f"WASM module did not load within {timeout_s}s")


async def capture_screenshot(canvas, path: str):
    """Capture a screenshot of the canvas element with atomic write."""
    # Use .png temp suffix so Playwright can detect the mime type
    dir_name = os.path.dirname(path) or "."
    tmp_path = os.path.join(dir_name, f".tmp_screenshot_{os.getpid()}.png")
    await canvas.screenshot(path=tmp_path)
    os.rename(tmp_path, path)


async def periodic_screenshot_loop(canvas, output_dir: str, stop_event: asyncio.Event):
    """Capture screenshots every 1s until stop_event is set."""
    path = os.path.join(output_dir, "frame_latest.png")
    while not stop_event.is_set():
        try:
            await capture_screenshot(canvas, path)
        except Exception as e:
            print(f"Screenshot error: {e}")
        try:
            await asyncio.wait_for(stop_event.wait(), timeout=1.0)
        except asyncio.TimeoutError:
            pass


async def execute_command(page, canvas, cmd_line: str, output_dir: str):
    """Execute a single command line."""
    parts = cmd_line.split()
    if not parts:
        return
    cmd = parts[0].upper()
    args = parts[1:]

    if cmd == "WAIT":
        ms = int(args[0]) if args else 1000
        await asyncio.sleep(ms / 1000.0)

    elif cmd == "KEY_DOWN":
        key_name = args[0] if args else ""
        playwright_key = KEY_MAP.get(key_name, key_name.lower())
        await page.keyboard.down(playwright_key)

    elif cmd == "KEY_UP":
        key_name = args[0] if args else ""
        playwright_key = KEY_MAP.get(key_name, key_name.lower())
        await page.keyboard.up(playwright_key)

    elif cmd == "KEY_PRESS":
        key_name = args[0] if args else ""
        playwright_key = KEY_MAP.get(key_name, key_name.lower())
        await page.keyboard.press(playwright_key)

    elif cmd == "SCREENSHOT":
        name = args[0] if args else f"screenshot_{int(time.time())}"
        if not name.endswith(".png"):
            name += ".png"
        path = os.path.join(output_dir, name)
        await capture_screenshot(canvas, path)
        print(f"Screenshot saved: {path}")

    elif cmd == "MOUSE_MOVE":
        if len(args) >= 2:
            x, y = float(args[0]), float(args[1])
            box = await canvas.bounding_box()
            if box:
                await page.mouse.move(box["x"] + x, box["y"] + y)

    elif cmd == "MOUSE_CLICK":
        if len(args) >= 2:
            x, y = float(args[0]), float(args[1])
        else:
            box = await canvas.bounding_box()
            x = box["width"] / 2 if box else 400
            y = box["height"] / 2 if box else 300
        box = await canvas.bounding_box()
        if box:
            await page.mouse.click(box["x"] + x, box["y"] + y)

    else:
        print(f"Unknown command: {cmd_line}")


async def command_loop(
    page, canvas, reader: CommandReader, output_dir: str, stop_event: asyncio.Event, idle_timeout_s: float = 5.0
):
    """Poll for new commands and execute them. Stop after idle_timeout_s with no new commands."""
    last_command_time = time.monotonic()

    while not stop_event.is_set():
        commands = reader.poll()
        if commands:
            last_command_time = time.monotonic()
            for cmd in commands:
                print(f"Executing: {cmd}")
                await execute_command(page, canvas, cmd, output_dir)
        else:
            idle = time.monotonic() - last_command_time
            if idle > idle_timeout_s:
                print(f"No new commands for {idle_timeout_s}s. Finishing.")
                stop_event.set()
                return

        await asyncio.sleep(0.2)


async def main():
    parser = argparse.ArgumentParser(description="Playwright visual test runner for Gambit WASM")
    parser.add_argument("--url", default="http://localhost:8080/WasmClient.html", help="URL of the WASM client")
    parser.add_argument("--timeout", type=int, default=180, help="Max test duration in seconds")
    parser.add_argument("--idle-timeout", type=float, default=5.0, help="Seconds of no new commands before stopping")
    parser.add_argument("--output-dir", default="test_output", help="Directory for screenshots and logs")
    parser.add_argument("--input-file", default="test_input.txt", help="Path to command input file")
    args = parser.parse_args()

    os.makedirs(args.output_dir, exist_ok=True)

    console_log_path = os.path.join(args.output_dir, "console.log")
    console_log = open(console_log_path, "w")

    stop_event = asyncio.Event()

    # Handle SIGINT gracefully
    loop = asyncio.get_event_loop()
    for sig in (signal.SIGINT, signal.SIGTERM):
        loop.add_signal_handler(sig, lambda: stop_event.set())

    # Overall timeout
    async def timeout_watcher():
        try:
            await asyncio.wait_for(stop_event.wait(), timeout=args.timeout)
        except asyncio.TimeoutError:
            print(f"Test timeout reached ({args.timeout}s)")
            stop_event.set()

    async with async_playwright() as p:
        browser = await p.chromium.launch(headless=True)
        context = await browser.new_context(viewport={"width": 1024, "height": 768})
        page = await context.new_page()

        # Capture console output
        def on_console(msg):
            text = f"[{msg.type}] {msg.text}\n"
            console_log.write(text)
            console_log.flush()

        page.on("console", on_console)

        print(f"Navigating to {args.url}...")
        try:
            await page.goto(args.url, wait_until="domcontentloaded", timeout=30000)
        except Exception as e:
            print(f"Failed to navigate to {args.url}: {e}")
            print("Is the HTTP server running? (cd build-wasm6 && python3 -m http.server 8080)")
            await browser.close()
            console_log.close()
            sys.exit(1)

        try:
            await wait_for_game_load(page)
        except TimeoutError as e:
            print(f"Load timeout: {e}")
            await browser.close()
            console_log.close()
            sys.exit(1)

        # Click canvas to focus it for keyboard input
        canvas = await page.query_selector("#canvas")
        if not canvas:
            print("Error: #canvas element not found")
            await browser.close()
            console_log.close()
            sys.exit(1)

        await canvas.click()
        print("Canvas focused. Starting test...")

        reader = CommandReader(args.input_file)

        # Run screenshot loop, command loop, and timeout watcher concurrently
        await asyncio.gather(
            periodic_screenshot_loop(canvas, args.output_dir, stop_event),
            command_loop(page, canvas, reader, args.output_dir, stop_event, args.idle_timeout),
            timeout_watcher(),
        )

        # Final screenshot
        final_path = os.path.join(args.output_dir, "final_state.png")
        try:
            await capture_screenshot(canvas, final_path)
            print(f"Final screenshot: {final_path}")
        except Exception:
            pass

        await browser.close()

    console_log.close()
    print(f"Console log: {console_log_path}")
    print("Test complete.")


if __name__ == "__main__":
    asyncio.run(main())
