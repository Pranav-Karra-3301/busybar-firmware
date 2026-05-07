"""
Device Events Service tests.

Tests reading the event flags and subscribing to indications.
"""

from __future__ import annotations

import asyncio

import allure
import pytest

from clients.ble.client import BleDeviceClient
from clients.ble.constants import CHAR_DEVICE_EVENTS_FLAGS, TIMEOUT_NOTIFICATION_WAIT
from clients.ble.models import DeviceEventsFlags


@allure.feature("BLE")
@allure.story("Device Events")
@pytest.mark.ble
class TestBleDeviceEvents:
    """Device Events flags read and indication tests."""

    @allure.title("Read event flags (uint32)")
    async def test_read_event_flags(
        self, connected_ble_client: BleDeviceClient
    ) -> None:
        """Flags characteristic should return parseable bytes."""
        result = await connected_ble_client.read_event_flags()
        assert isinstance(result, DeviceEventsFlags)
        assert isinstance(result.flags_value, int)

    @allure.title("Event flags indication on device name change")
    async def test_event_flags_indication(
        self,
        connected_ble_client: BleDeviceClient,
        settings_api,
    ) -> None:
        """Changing the device name via HTTP should trigger an indication."""
        received: asyncio.Future[bytearray] = asyncio.get_event_loop().create_future()

        def _callback(sender: int, data: bytearray) -> None:
            if not received.done():
                received.set_result(data)

        await connected_ble_client.start_notify(
            CHAR_DEVICE_EVENTS_FLAGS, _callback
        )

        try:
            # Read current name to restore later
            original_name = settings_api.get_name()

            # Change name to trigger an event flag indication
            with allure.step("Change device name via HTTP API"):
                settings_api.set_name("BLE_TEST_TMP")

            try:
                data = await asyncio.wait_for(
                    received, timeout=TIMEOUT_NOTIFICATION_WAIT
                )
                assert len(data) > 0, "Received empty indication"
            except asyncio.TimeoutError:
                pytest.fail(
                    "No Device Events indication received within timeout"
                )
            finally:
                # Restore original name
                with allure.step("Restore device name"):
                    if hasattr(original_name, "name"):
                        settings_api.set_name(original_name.name)
        finally:
            await connected_ble_client.stop_notify(CHAR_DEVICE_EVENTS_FLAGS)
