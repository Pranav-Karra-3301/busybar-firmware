"""Shared fixtures for the CLI test suite (integration/cli/).

Plain helpers and constants live in utils/cli_helpers.py.
"""

import re
import threading

import pytest

from clients.cli import SimpleCLIConnection
from utils.cli_helpers import resync
from utils.fetch_http_server import FetchHTTPServer, FetchRequestHandler
from utils.js_test_runner import run_js_case


@pytest.fixture(scope="module", autouse=True)
def cli_debug(persistent_cli_connection):
    """Debug mode on for the whole CLI module, and left on afterwards.

    The flag lives in NVM and survives reboots, so a suite that turned it off would
    leave the bench without the debug-gated commands (`gpio`, `otp`, `factory_reset`).
    Re-enable on teardown as well: `test_sysctl_debug_toggle` flips it off on purpose.
    """
    persistent_cli_connection.execute_command("sysctl debug 1")
    yield
    persistent_cli_connection.execute_command("sysctl debug 1")


@pytest.fixture(scope="class")
def sl_cli():
    """CLI in 917 (sl_cli) mode, shared by the whole 917 class.

    Its own connection, not `persistent_cli_connection`: `sl_cli` is exclusive and
    a test failing inside 917 mode must not leave the shared CLI at the `917>:`
    prompt. Entering 917 mode is slow, so do it once per class, not per test — the
    commands in there are read-only and cannot interfere with each other.
    """
    cli = SimpleCLIConnection()
    if not cli.connect():
        pytest.skip("Could not connect to CLI")
    try:
        cli.enter_sl_cli()
        yield cli
    finally:
        if cli._in_sl_cli:
            cli.exit_sl_cli()
        cli.disconnect()


@pytest.fixture
def storage_dir(persistent_cli_connection):
    """Empty `/ext/cli_test`, wiped again afterwards — whatever the test did or
    left half-done. Cleanup runs even when the test fails, and starts with a
    resync so it still works if a raw-protocol test died mid-command."""
    cli = persistent_cli_connection
    path = "/ext/cli_test"

    def rm_rf(target):
        # `storage remove` only unlinks files and *empty* dirs, so walk the tree
        # depth-first (`extract` leaves a whole subtree behind in out/)
        listing = cli.execute_command(f"storage list {target}")
        for kind, name in re.findall(r"\[([DF])\]\s+(\S+)", listing):
            child = f"{target}/{name}"
            rm_rf(child) if kind == "D" else cli.execute_command(
                f"storage remove {child}"
            )
        cli.execute_command(f"storage remove {target}")

    rm_rf(path)  # a previous run may have died before its own cleanup
    cli.execute_command(f"storage mkdir {path}")
    try:
        yield path
    finally:
        resync(cli)
        rm_rf(path)


@pytest.fixture
def http_server(persistent_cli_connection):
    """HTTP server on the pytest host, reachable from the device under test."""
    host_ip = persistent_cli_connection.tn.sock.getsockname()[0]
    server = FetchHTTPServer((host_ip, 0), FetchRequestHandler)
    server_thread = threading.Thread(target=server.serve_forever, daemon=True)
    server_thread.start()
    try:
        yield server
    finally:
        server.release_stall.set()
        server.shutdown()
        server_thread.join(timeout=2)
        server.server_close()


@pytest.fixture
def js_case_runner(persistent_cli_connection, storage_api, storage_dir):
    """Upload and run an isolated JavaScript assertion case on the device."""

    def runner(case_name, body, timeout=25):
        return run_js_case(
            persistent_cli_connection,
            storage_api,
            storage_dir,
            case_name,
            body,
            timeout,
        )

    return runner
