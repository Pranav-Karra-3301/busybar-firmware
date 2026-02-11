#!/usr/bin/env python3

import argparse
import json
import os
import re
import sys
import tarfile
import tempfile
import urllib.request
from pathlib import Path

from flipper.cli import Cli
from flipper.storage_socket import FlipperStorage

PORT_NAME = ("10.0.4.20", 23)
BASE_URL = "https://update.flipperzero.one/builds/busybar-firmware"


class FactoryFirmwareUpdater:
    """Manage firmware update process."""

    def __init__(self, hardware: str, tar_path: str | None = None):
        """Initialize updater.

        Args:
            hardware: Hardware type (f21, f22, etc.)
            tar_path: Optional path to local tar file
        """
        self.hardware = hardware
        self.tar_path = tar_path
        self.target_device_path = "/bkp/recovery"
        self.file_manifest = {}  # {filename: size}

    def run(self) -> bool:
        """Execute update process.

        Returns:
            True if successful, False otherwise
        """
        try:
            print(f"=== BusyBar Firmware Update ===")
            print(f"Hardware: {self.hardware}")
            print(f"Target: {self.target_device_path}\n")

            # Get or use provided tar file
            if self.tar_path:
                extracted_path = self._extract_local_tar()
            else:
                extracted_path = self._download_and_extract_tar()

            if not extracted_path:
                return False

            # Upload to device
            if not self._upload_to_device(extracted_path):
                return False

            # Verify uploaded files
            if not self._verify_uploaded_files():
                return False

            print("\n✓ Update completed successfully!")
            return True

        except Exception as e:
            print(f"\n✗ Error: {e}", file=sys.stderr)
            return False

    def _download_and_extract_tar(self) -> str | None:
        """Download firmware from update server and extract.

        Returns:
            Path to extracted directory or None
        """
        # Get latest version
        print("Searching for latest firmware version...")
        version = self._get_latest_version()
        if not version:
            print("✗ No versions found")
            return None

        print(f"Found version: {version}")

        # Find tar file
        tar_file = self._find_tar_file(version)
        if not tar_file:
            print(f"✗ No busybar-{self.hardware}-*.tar file found")
            return None

        print(f"Found tar file: {tar_file}")

        # Download and extract
        tar_url = f"{BASE_URL}/{version}/{tar_file}"
        return self._download_tar(tar_url)

    def _extract_local_tar(self) -> str | None:
        """Extract local tar file.

        Returns:
            Path to extracted directory or None
        """
        if not os.path.exists(self.tar_path):
            print(f"✗ File not found: {self.tar_path}")
            return None

        print(f"Using local tar file: {self.tar_path}")
        return self._extract_tar(self.tar_path)

    def _get_latest_version(self) -> str | None:
        """Get latest version from remote server."""
        try:
            url = f"{BASE_URL}/"
            html = self._get_html(url)
            versions = re.findall(r'href="([0-9]+\.[0-9]+\.[0-9]+)/"', html)

            if not versions:
                return None

            # Sort versions and return latest
            versions.sort(key=lambda v: tuple(map(int, v.split("."))))
            return versions[-1]

        except Exception as e:
            print(f"✗ Failed to get version: {e}")
            return None

    def _find_tar_file(self, version: str) -> str | None:
        """Find tar file matching hardware type."""
        try:
            url = f"{BASE_URL}/{version}/"
            html = self._get_html(url)
            files = re.findall(r'href="([^"]+\.tar)"', html)

            # Find file matching hardware type
            hw_pattern = f"busybar-{self.hardware}".lower()
            for f in files:
                if hw_pattern in f.lower():
                    return f

            return None

        except Exception as e:
            print(f"✗ Failed to find tar file: {e}")
            return None

    def _download_tar(self, url: str) -> str | None:
        """Download and extract tar file."""
        try:
            temp_dir = tempfile.mkdtemp(prefix="busybar_update_")
            tar_path = os.path.join(temp_dir, "firmware.tar")

            print(f"\nDownloading: {url}")
            urllib.request.urlretrieve(url, tar_path)
            print(f"Downloaded: {tar_path}")

            return self._extract_tar(tar_path)

        except Exception as e:
            print(f"✗ Download failed: {e}")
            return None

    def _extract_tar(self, tar_path: str) -> str | None:
        """Extract tar file and build manifest."""
        try:
            extract_dir = os.path.join(os.path.dirname(tar_path), "extracted")
            os.makedirs(extract_dir, exist_ok=True)

            print(f"Extracting to {extract_dir}...")
            with tarfile.open(tar_path, "r:*") as tar:
                tar.extractall(path=extract_dir)

            # Build file manifest
            self._build_manifest(extract_dir)
            return extract_dir

        except Exception as e:
            print(f"✗ Extraction failed: {e}")
            return None

    def _build_manifest(self, extract_dir: str) -> None:
        """Build manifest of files to upload."""
        self.file_manifest = {}
        for root, _, files in os.walk(extract_dir):
            for file in files:
                file_path = os.path.join(root, file)
                size = os.path.getsize(file_path)
                rel_path = os.path.relpath(file_path, extract_dir)
                self.file_manifest[rel_path] = size

    def _upload_to_device(self, extract_dir: str) -> bool:
        """Upload files to device."""
        try:
            with Cli(PORT_NAME) as cli:
                print("\nUnlock device features...")
                cli.send("sysctl debug 1\r")
                cli.send("sysctl storage_bkp_unlock 1\r")

            with FlipperStorage(PORT_NAME) as storage:
                print("Uploading files to device...")

                # Ensure target dir exists
                if not storage.exist_dir(self.target_device_path):
                    print(f"  Creating {self.target_device_path}")
                    storage.mkdir(self.target_device_path)

                # Upload all files
                for root, dirs, files in os.walk(extract_dir):
                    # Create subdirectories
                    for dir_name in dirs:
                        local_dir = os.path.join(root, dir_name)
                        rel_path = os.path.relpath(local_dir, extract_dir)
                        device_dir = (
                            self.target_device_path
                            + "/"
                            + rel_path.replace(os.sep, "/")
                        )

                        if not storage.exist_dir(device_dir):
                            print(f"  Creating {device_dir}")
                            storage.mkdir(device_dir)

                    # Upload files
                    for file_name in files:
                        local_file = os.path.join(root, file_name)
                        rel_path = os.path.relpath(local_file, extract_dir)
                        device_file = (
                            self.target_device_path
                            + "/"
                            + rel_path.replace(os.sep, "/")
                        )

                        size = os.path.getsize(local_file)
                        print(f"  Uploading {rel_path} ({size} bytes)")
                        storage.send_file(local_file, device_file)

            return True

        except Exception as e:
            print(f"✗ Upload failed: {e}")
            return False
        finally:
            try:
                with Cli(PORT_NAME) as cli:
                    print("\nLocking device features...")
                    cli.send("sysctl debug 0\r")
                    cli.send("sysctl storage_bkp_unlock 0\r")
            except Exception:
                pass

    def _verify_uploaded_files(self) -> bool:
        """Verify uploaded files and their sizes."""
        try:
            print("\nVerifying uploaded files...")

            with FlipperStorage(PORT_NAME) as storage:
                # Collect device file info
                device_files = {}
                for root, _, files in storage.walk(self.target_device_path):
                    for file in files:
                        file_path = os.path.join(root, file).replace(os.sep, "/")
                        rel_path = file_path.replace(
                            self.target_device_path, ""
                        ).lstrip("/")

                        try:
                            size = storage.size(file_path)
                            device_files[rel_path] = size
                        except Exception:
                            print(f"  ⚠ Could not get size of {file_path}")

                # Compare with local manifest
                print("\nFile comparison:")
                all_match = True
                for local_file, local_size in self.file_manifest.items():
                    if local_file in device_files:
                        device_size = device_files[local_file]
                        if local_size == device_size:
                            print(f"  ✓ {local_file}: {device_size} bytes")
                        else:
                            print(
                                f"  ✗ {local_file}: local={local_size}, device={device_size}"
                            )
                            all_match = False
                    else:
                        print(f"  ✗ {local_file}: not found on device")
                        all_match = False

                # Check for extra files on device
                for device_file in device_files:
                    if device_file not in self.file_manifest:
                        print(f"  ⚠ {device_file}: extra file on device")

                if all_match:
                    print("\n✓ All files verified successfully!")
                    return True
                else:
                    print("\n✗ Some files do not match!")
                    return False

        except Exception as e:
            print(f"✗ Verification failed: {e}")
            return False

    @staticmethod
    def _get_html(url: str) -> str:
        """Get HTML content from URL."""
        req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
        with urllib.request.urlopen(req) as response:
            return response.read().decode("utf-8")


def parse_arguments() -> argparse.Namespace:
    """Parse command line arguments."""
    parser = argparse.ArgumentParser(
        description="Update recovery partition on BusyBar device",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Update f21 with latest release firmware
  python update_recovery_fw.py

  # Update f22 (latest available)
  python update_recovery_fw.py --hardware f22
  # Use local tar file
  python update_recovery_fw.py --tar-path /path/to/firmware.tar
        """,
    )

    parser.add_argument(
        "--hardware",
        choices=["f21", "f22", "a21", "a22", "b"],
        default="f21",
        help="Hardware type (default: f21)",
    )

    parser.add_argument(
        "--tar-path",
        type=str,
        help="Path to local tar file (skips download)",
    )

    return parser.parse_args()


def main():
    args = parse_arguments()

    updater = FactoryFirmwareUpdater(
        hardware=args.hardware,
        tar_path=args.tar_path,
    )

    success = updater.run()
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
