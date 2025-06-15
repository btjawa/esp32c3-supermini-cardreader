import asyncio
from bleak import BleakClient, BleakScanner, BleakError

DEVICE_NAME = "ESP32C3_Debug"
CHARACTERISTIC_UUID = "e8825349-bb14-4a68-aab4-925fd6288652"
RETRY_INTERVAL = 1  # seconds
HEARTBEAT_INTERVAL = 5  # seconds
MAX_MISSED_SCANS = 3  # 连续漏扫次数阈值

async def listen_to_device(address):
    missed_count = 0
    while True:
        try:
            async with BleakClient(address) as client:
                print(f"✅ Connected to {DEVICE_NAME} at {address}")

                def handle_notification(sender, data):
                    try:
                        print(f"{sender}:")
                        print(f"{data.decode(errors='ignore')}")
                    except Exception as e:
                        print(f"⚠️ Decode error: {e}")

                await client.start_notify(CHARACTERISTIC_UUID, handle_notification)
                print("📡 Listening for notifications...")

                while True:
                    if not client.is_connected:
                        raise BleakError("Device disconnected.")

                    devices = await BleakScanner.discover()
                    found = any(d.name == DEVICE_NAME for d in devices)
                    if found:
                        missed_count = 0  # reset missed count
                    else:
                        missed_count += 1
                        print(f"⚠️ Missed scan {missed_count}/{MAX_MISSED_SCANS}")
                        if missed_count >= MAX_MISSED_SCANS:
                            print(f"⚠️ Device {DEVICE_NAME} disappeared from scan.")
                            raise BleakError("Device not found in scan.")

                    await asyncio.sleep(HEARTBEAT_INTERVAL)

        except BleakError as e:
            print(f"⚠️ Disconnected or error occurred: {e}")
            print(f"🔄 Will retry in {RETRY_INTERVAL} seconds...")
            await asyncio.sleep(RETRY_INTERVAL)

        except Exception as e:
            print(f"🔥 Unexpected error: {e}")
            break  # On unknown error, exit

async def main_loop():
    while True:
        print("🔍 Scanning for devices...")
        devices = await BleakScanner.discover()
        target = None
        for d in devices:
            print(f"Found: {d.name} [{d.address}]")
            if d.name == DEVICE_NAME:
                target = d
                break

        if target:
            await listen_to_device(target.address)
        else:
            print(f"❌ Device {DEVICE_NAME} not found. Retrying in {RETRY_INTERVAL} seconds...")
            await asyncio.sleep(RETRY_INTERVAL)

def main():
    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)
    task = loop.create_task(main_loop())
    try:
        loop.run_until_complete(task)
    except KeyboardInterrupt:
        print("\n👋 Received Ctrl+C! Stopping...")
        task.cancel()
        try:
            loop.run_until_complete(task)
        except asyncio.CancelledError:
            pass
    finally:
        loop.close()

if __name__ == "__main__":
    main()
