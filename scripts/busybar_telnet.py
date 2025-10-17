#!/usr/bin/env python3
"""Interactive BusyBar Telnet client.

Usage:
    python busybar_telnet.py [--host HOST] [--port PORT] [--timeout SECONDS]

Connects to the BusyBar telnet service, forwarding all input/output while keeping
control sequences (Ctrl+C, Ctrl+Z, etc.) intact. By default it targets
``10.0.4.20:23`` and you can override the address via CLI flags or the
``BUSYBAR_IP`` / ``BUSYBAR_TELNET_PORT`` environment variables.
"""


from __future__ import annotations

import warnings

warnings.filterwarnings("ignore", category=DeprecationWarning)

import errno
import os
import select
import socket
import sys
import telnetlib  # type: ignore[import]
import termios
import time
import tty
from contextlib import contextmanager
from typing import Iterator, Optional

from flipper.app import App

DEFAULT_HOST = os.getenv("BUSYBAR_IP", "10.0.4.20")
DEFAULT_PORT = int(os.getenv("BUSYBAR_TELNET_PORT", "23"))
DEFAULT_TIMEOUT = float(os.getenv("BUSYBAR_TELNET_TIMEOUT", "10"))


@contextmanager
def _raw_terminal(fd: int) -> Iterator[None]:
    """Put the provided file descriptor into raw, non-blocking mode."""

    original_attrs = termios.tcgetattr(fd)
    was_blocking = os.get_blocking(fd)
    try:
        # Switch to raw mode to forward control sequences untouched
        tty.setraw(fd)
        # Ensure reads don't block indefinitely
        os.set_blocking(fd, False)
        yield
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, original_attrs)
        os.set_blocking(fd, was_blocking)


class BusyBarTelnetClient:
    def __init__(self, host: str, port: int, timeout: float, logger):
        self.host = host
        self.port = port
        self.timeout = timeout
        self.logger = logger

    def run(self) -> int:
        try:
            self._ensure_reachable(self.host, self.port, self.timeout)
        except TimeoutError as exc:
            self.logger.error(str(exc))
            return 2
        except OSError as exc:
            self.logger.error(
                "Connectivity check failed for %s:%s: %s", self.host, self.port, exc
            )
            return 2

        try:
            with telnetlib.Telnet(self.host, self.port, timeout=self.timeout) as tn:
                self.logger.info("Connected to %s:%d", self.host, self.port)
                self.logger.info(
                    "Press Ctrl+] followed by Enter to terminate the session."
                )
                self._interactive_loop(tn)
        except (socket.timeout, TimeoutError):
            self.logger.error(
                "Connection to %s:%d timed out after %.1f seconds",
                self.host,
                self.port,
                self.timeout,
            )
            return 1
        except ConnectionRefusedError:
            self.logger.error("Connection refused by %s:%d", self.host, self.port)
            return 1
        except OSError as exc:
            self.logger.error("Telnet error: %s", exc)
            return 1

        return 0

    def _ensure_reachable(self, host: str, port: int, timeout: float) -> None:
        deadline = time.monotonic() + timeout
        last_error: Optional[OSError] = None
        self.logger.info(
            "Checking reachability of %s:%d (timeout %.1fs)", host, port, timeout
        )

        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                break
            try:
                connect_timeout = max(0.5, min(remaining, 3.0))
                with socket.create_connection((host, port), timeout=connect_timeout):
                    self.logger.debug("Reachability confirmed for %s:%d", host, port)
                    return
            except OSError as exc:
                last_error = exc
                time.sleep(min(0.5, max(0.0, deadline - time.monotonic())))

        raise TimeoutError(
            f"Unable to reach {host}:{port} within {timeout:.1f}s"
            + (f" (last error: {last_error})" if last_error else "")
        )

    def _interactive_loop(self, tn: telnetlib.Telnet) -> None:
        stdin_fd = sys.stdin.fileno()
        stdout_fd = sys.stdout.fileno()

        with _raw_terminal(stdin_fd):
            should_run = True
            while should_run:
                read_set = [tn.fileno(), stdin_fd]
                readable, _, _ = select.select(read_set, [], [])

                if tn.fileno() in readable:
                    try:
                        buffer = tn.read_very_eager()
                    except EOFError:
                        self.logger.info("Remote host closed the connection.")
                        break

                    if buffer:
                        os.write(stdout_fd, buffer)
                    elif tn.eof:
                        self.logger.info("Remote host closed the connection.")
                        break

                if stdin_fd in readable:
                    try:
                        chunk = os.read(stdin_fd, 1024)
                    except BlockingIOError:
                        chunk = b""
                    except OSError as exc:
                        if exc.errno in (errno.EAGAIN, errno.EWOULDBLOCK):
                            chunk = b""
                        else:
                            raise

                    if not chunk:
                        continue

                    # Telnet escape sequence: Ctrl+]
                    if chunk == b"\x1d":
                        self.logger.info("Escape key detected, closing session.")
                        should_run = False
                        continue

                    tn.write(chunk)


class Main(App):
    def init(self):  # type: ignore[override]
        self.parser.description = "BusyBar Telnet client"
        self.parser.add_argument(
            "--host",
            default=DEFAULT_HOST,
            help=f"BusyBar hostname or IP (default: {DEFAULT_HOST})",
        )
        self.parser.add_argument(
            "--port",
            type=int,
            default=DEFAULT_PORT,
            help=f"Telnet port (default: {DEFAULT_PORT})",
        )
        self.parser.add_argument(
            "--timeout",
            type=float,
            default=DEFAULT_TIMEOUT,
            help=f"Reachability and connection timeout in seconds (default: {DEFAULT_TIMEOUT})",
        )
        self.parser.set_defaults(func=self.main)

    def main(self) -> int:
        args = self.args
        client = BusyBarTelnetClient(args.host, args.port, args.timeout, self.logger)
        return client.run()


if __name__ == "__main__":
    Main()()
