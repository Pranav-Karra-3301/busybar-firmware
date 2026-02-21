#!/usr/bin/env python3
"""
BusyBar Recovery Firmware Updater

This script downloads the latest firmware for a specified hardware variant,
uploads it to the device's recovery partition, and verifies the transfer.
It includes a safety check to ensure the firmware matches the device hardware.
"""

import argparse
import logging
import re
import sys
import tarfile
import tempfile
import time
import urllib.error
import urllib.request
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Optional, Tuple

from flipper.cli import Cli
from flipper.storage_socket import FlipperStorage

# ----------------------------------------------------------------------
# Configuration
# ----------------------------------------------------------------------
DEFAULT_PORT = ("10.0.4.20", 23)
BASE_URL = "https://update.flipperzero.one/builds/busybar-firmware"
RETRY_ATTEMPTS = 3
RETRY_DELAY = 2  # seconds

# Setup logging
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(levelname)s - %(message)s",
    datefmt="%H:%M:%S",
)
logger = logging.getLogger("busybar_updater")


# ----------------------------------------------------------------------
# Data classes
# ----------------------------------------------------------------------
@dataclass
class FirmwareManifest:
    """Represents a set of files with their sizes."""
    files: Dict[str, int]  # relative path -> size in bytes


# ----------------------------------------------------------------------
# Core updater class
# ----------------------------------------------------------------------
class FactoryFirmwareUpdater:
    """Manages the firmware update process."""

    def __init__(
        self,
        hardware: str,
        port: Tuple[str, int] = DEFAULT_PORT,
        tar_path: Optional[Path] = None,
    ) -> None:
        """
        Args:
            hardware: Hardware type (e.g., 'f21', 'f22').
            port: (host, port) tuple for the device connection.
            tar_path: Optional local path to a firmware tarball.
        """
        self.hardware = hardware.lower()
        self.port = port
        self.tar_path = Path(tar_path) if tar_path else None
        self.target_device_path = "/bkp/recovery"
        self.manifest: Optional[FirmwareManifest] = None

    def run(self) -> bool:
        """Execute the full update workflow."""
        logger.info("=== BusyBar Firmware Update ===")
        logger.info(f"Hardware : {self.hardware}")
        logger.info(f"Target   : {self.target_device_path}")

        try:
            # Step 1: Verify device compatibility before doing anything else
            if not self._check_device_compatibility():
                return False

            # Step 2: Obtain firmware (download or local)
            if self.tar_path:
                firmware_dir = self._extract_local_tar()
            else:
                firmware_dir = self._download_and_extract()

            if firmware_dir is None:
                return False

            # Step 3: Upload to device
            if not self._upload_to_device(firmware_dir):
                return False

            # Step 4: Verify uploaded files
            if not self._verify_upload():
                return False

            logger.info("✓ Update completed successfully!")
            return True

        except Exception as e:
            logger.exception("Unhandled exception")
            return False

    # ------------------------------------------------------------------
    # Hardware compatibility check
    # ------------------------------------------------------------------
    def _check_device_compatibility(self) -> bool:
        """
        Query the device for its target (u5_firmware_target) and compare
        with the expected value derived from the hardware argument.
        Returns True if compatible, False otherwise.
        """
        # Extract numeric target from hardware string (e.g., "f21" -> 21)
        m = re.search(r"\d+$", self.hardware)
        if not m:
            logger.warning(
                f"Hardware '{self.hardware}' does not specify a numeric target; "
                "skipping compatibility check."
            )
            return True

        expected_target = int(m.group())
        logger.info(
            f"Checking device compatibility: expecting target {expected_target}"
        )

        actual_target = self._get_device_target()
        if actual_target is None:
            logger.error("Could not determine device target from device_info")
            return False

        if actual_target != expected_target:
            logger.error(
                f"Device target mismatch: expected {expected_target}, got {actual_target}. "
                "Aborting to prevent firmware corruption."
            )
            return False

        logger.info(f"Device target matches: {actual_target}")
        return True

    def _get_device_target(self) -> Optional[int]:
        """Connect via CLI, run 'device_info', and parse u5_firmware_target."""
        try:
            with Cli(self.port) as cli:
                data = cli.send_and_wait_prompt("device_info\r")
                output = data.decode("utf-8", errors="ignore")
                logger.debug(f"device_info output:\n{output}")

                # Parse for the target line
                for line in output.splitlines():
                    if "u5_firmware_target" in line:
                        match = re.search(r":\s*(\d+)", line)
                        if match:
                            return int(match.group(1))

                logger.error("'u5_firmware_target' not found in device_info output")
                return None

        except Exception as e:
            logger.error(f"Failed to communicate with device: {e}")
            return None

    # ------------------------------------------------------------------
    # Firmware acquisition
    # ------------------------------------------------------------------
    def _download_and_extract(self) -> Optional[Path]:
        """Download the latest firmware tarball and extract it."""
        logger.info("Searching for latest firmware version...")
        version = self._get_latest_version()
        if not version:
            logger.error("No versions found on server")
            return None

        logger.info(f"Latest version: {version}")

        tar_filename = self._find_tar_file(version)
        if not tar_filename:
            logger.error(f"No tarball for hardware '{self.hardware}' found")
            return None

        logger.info(f"Found tarball: {tar_filename}")
        tar_url = f"{BASE_URL}/{version}/{tar_filename}"

        return self._download_and_extract_tar(tar_url)

    def _extract_local_tar(self) -> Optional[Path]:
        """Extract a local tarball."""
        if not self.tar_path or not self.tar_path.exists():
            logger.error(f"Local file not found: {self.tar_path}")
            return None

        logger.info(f"Using local tarball: {self.tar_path}")
        return self._extract_tar(self.tar_path)

    def _get_latest_version(self) -> Optional[str]:
        """Parse the version directory listing and return the newest version."""
        url = f"{BASE_URL}/"
        html = self._fetch_url(url)
        if not html:
            return None

        # Find all version strings like "1.2.3/"
        version_pattern = r'href="([0-9]+(?:\.[0-9]+)*)/"'
        versions = re.findall(version_pattern, html)
        if not versions:
            return None

        # Sort versions correctly (e.g., 1.10.0 > 1.9.0)
        def version_key(v: str):
            return tuple(int(part) for part in v.split("."))

        versions.sort(key=version_key)
        return versions[-1]

    def _find_tar_file(self, version: str) -> Optional[str]:
        """Find the tarball matching the hardware type in the version directory."""
        url = f"{BASE_URL}/{version}/"
        html = self._fetch_url(url)
        if not html:
            return None

        # Look for .tar files
        tar_pattern = r'href="([^"]+\.tar)"'
        tarballs = re.findall(tar_pattern, html)

        # Filter by hardware name (case‑insensitive)
        hw_pattern = f"busybar-{self.hardware}".lower()
        for tb in tarballs:
            if hw_pattern in tb.lower():
                return tb
        return None

    def _fetch_url(self, url: str, retries: int = RETRY_ATTEMPTS) -> Optional[str]:
        """Fetch URL content with retries."""
        for attempt in range(1, retries + 1):
            try:
                req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
                with urllib.request.urlopen(req, timeout=10) as resp:
                    return resp.read().decode("utf-8")
            except (urllib.error.URLError, TimeoutError) as e:
                logger.warning(f"Attempt {attempt}/{retries} failed for {url}: {e}")
                if attempt < retries:
                    time.sleep(RETRY_DELAY)
                else:
                    logger.error(f"All {retries} attempts failed for {url}")
        return None

    def _download_and_extract_tar(self, url: str) -> Optional[Path]:
        """Download a tarball and extract it into a temporary directory."""
        # Create a temporary directory
        tmp_dir = Path(tempfile.mkdtemp(prefix="busybar_update_"))
        tar_path = tmp_dir / "firmware.tar"

        logger.info(f"Downloading {url} ...")
        try:
            urllib.request.urlretrieve(url, tar_path)
            logger.info(f"Downloaded to {tar_path}")
        except Exception as e:
            logger.error(f"Download failed: {e}")
            self._cleanup_temp_dir(tmp_dir)
            return None

        extract_dir = self._extract_tar(tar_path, base_dir=tmp_dir)
        if extract_dir is None:
            self._cleanup_temp_dir(tmp_dir)
            return None

        return extract_dir

    def _extract_tar(
        self, tar_path: Path, base_dir: Optional[Path] = None
    ) -> Optional[Path]:
        """Extract a tarball and build the file manifest."""
        if base_dir is None:
            base_dir = tar_path.parent

        extract_dir = base_dir / "extracted"
        extract_dir.mkdir(exist_ok=True)

        logger.info(f"Extracting to {extract_dir} ...")
        try:
            with tarfile.open(tar_path, "r:*") as tar:
                tar.extractall(path=extract_dir)
        except Exception as e:
            logger.error(f"Extraction failed: {e}")
            return None

        self._build_manifest(extract_dir)
        return extract_dir

    def _build_manifest(self, extract_dir: Path) -> None:
        """Walk the extracted directory and record all file sizes."""
        files = {}
        for file_path in extract_dir.rglob("*"):
            if file_path.is_file():
                rel_path = file_path.relative_to(extract_dir).as_posix()
                files[rel_path] = file_path.stat().st_size
        self.manifest = FirmwareManifest(files=files)
        logger.info(f"Manifest built: {len(files)} files to upload")

    @staticmethod
    def _cleanup_temp_dir(path: Path) -> None:
        """Remove a temporary directory (and all its contents)."""
        import shutil
        try:
            shutil.rmtree(path)
            logger.debug(f"Removed temporary directory: {path}")
        except Exception as e:
            logger.warning(f"Failed to remove temporary directory {path}: {e}")

    # ------------------------------------------------------------------
    # Device communication
    # ------------------------------------------------------------------
    def _unlock_device(self) -> bool:
        """Send unlock commands via CLI."""
        try:
            with Cli(self.port) as cli:
                logger.info("Unlocking device features...")
                cli.send("sysctl debug 1\r")
                cli.send("sysctl storage_bkp_unlock 1\r")
            return True
        except Exception as e:
            logger.error(f"Failed to unlock device: {e}")
            return False

    def _lock_device(self) -> None:
        """Send lock commands (best effort)."""
        try:
            with Cli(self.port) as cli:
                logger.info("Locking device features...")
                cli.send("sysctl debug 0\r")
                cli.send("sysctl storage_bkp_unlock 0\r")
        except Exception:
            pass

    def _upload_to_device(self, extract_dir: Path) -> bool:
        """Upload all files from extract_dir to the device's recovery partition."""
        if not self._unlock_device():
            return False

        try:
            with FlipperStorage(self.port) as storage:
                # Ensure target directory exists
                if not storage.exist_dir(self.target_device_path):
                    logger.info(f"Creating {self.target_device_path}")
                    storage.mkdir(self.target_device_path)

                # Walk the local directory and upload files
                for local_path in sorted(extract_dir.rglob("*")):
                    rel_path = local_path.relative_to(extract_dir).as_posix()

                    if local_path.is_dir():
                        # Create directory on device
                        device_dir = f"{self.target_device_path}/{rel_path}"
                        if not storage.exist_dir(device_dir):
                            logger.debug(f"Creating directory {device_dir}")
                            storage.mkdir(device_dir)

                    elif local_path.is_file():
                        device_file = f"{self.target_device_path}/{rel_path}"
                        size = local_path.stat().st_size
                        logger.info(f"Uploading {rel_path} ({size} bytes)")
                        storage.send_file(str(local_path), device_file)

            return True

        except Exception as e:
            logger.error(f"Upload failed: {e}")
            return False
        finally:
            self._lock_device()

    def _verify_upload(self) -> bool:
        """Compare files on device with the local manifest."""
        if not self.manifest:
            logger.error("No local manifest to verify against")
            return False

        logger.info("Verifying uploaded files...")

        try:
            with FlipperStorage(self.port) as storage:
                # Get all files on device under the target path
                device_files = self._list_device_files(storage)

                # Compare
                all_match = True
                logger.info("File comparison:")

                for rel_path, local_size in self.manifest.files.items():
                    device_size = device_files.get(rel_path)
                    if device_size is None:
                        logger.error(f"  ✗ {rel_path}: missing on device")
                        all_match = False
                    elif local_size != device_size:
                        logger.error(
                            f"  ✗ {rel_path}: size mismatch (local={local_size}, device={device_size})"
                        )
                        all_match = False
                    else:
                        logger.info(f"  ✓ {rel_path}: {device_size} bytes")

                # Warn about extra files on device
                for dev_path in device_files:
                    if dev_path not in self.manifest.files:
                        logger.warning(f"  ⚠ {dev_path}: extra file on device")

                if all_match:
                    logger.info("✓ All files verified successfully!")
                    return True
                else:
                    logger.error("✗ Some files do not match!")
                    return False

        except Exception as e:
            logger.error(f"Verification failed: {e}")
            return False

    def _list_device_files(self, storage) -> Dict[str, int]:
        """Walk the device's target directory and return {relative_path: size}."""
        result = {}
        target = self.target_device_path.rstrip("/")
        for root, dirs, files in storage.walk(target):
            for file in files:
                full_path = f"{root}/{file}"
                # Get relative path (strip target prefix and leading slash)
                rel_path = full_path.replace(target, "").lstrip("/")
                try:
                    size = storage.size(full_path)
                    result[rel_path] = size
                except Exception as e:
                    logger.warning(f"Could not get size of {full_path}: {e}")
        return result


# ----------------------------------------------------------------------
# Command line interface
# ----------------------------------------------------------------------
def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Update recovery partition on BusyBar device",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Update f21 with latest release firmware
  python update_recovery_fw.py

  # Update f22
  python update_recovery_fw.py --hardware f22

  # Use local tar file
  python update_recovery_fw.py --tar-path /path/to/firmware.tar

  # Specify device IP and port
  python update_recovery_fw.py --port 192.168.1.100:23
        """,
    )
    parser.add_argument(
        "--hardware",
        choices=["f21", "f22"],
        default="f21",
        help="Hardware type (default: f21)",
    )
    parser.add_argument(
        "--tar-path",
        type=Path,
        help="Path to local tar file (skips download)",
    )
    parser.add_argument(
        "--port",
        type=str,
        default="10.0.4.20:23",
        help="Device connection string as host:port (default: 10.0.4.20:23)",
    )
    parser.add_argument(
        "--verbose", "-v", action="store_true", help="Enable debug logging"
    )
    return parser.parse_args()


def main() -> None:
    args = parse_arguments()

    if args.verbose:
        logger.setLevel(logging.DEBUG)

    # Parse port argument
    try:
        host, port_str = args.port.split(":")
        port = (host, int(port_str))
    except ValueError:
        logger.error("Invalid port format. Use host:port (e.g., 10.0.4.20:23)")
        sys.exit(1)

    updater = FactoryFirmwareUpdater(
        hardware=args.hardware,
        port=port,
        tar_path=args.tar_path,
    )

    success = updater.run()
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()