
import asyncio
from datetime import datetime
from bleak import BleakClient, BleakScanner

# ---------- CONFIG ----------
TARGET_NAME = "Austin" # <-- ONLY change this to match BLE.setLocalName(...)
TARGET_SERVICE_UUID = "2AA846A1-29D3-FE8A-59FC-ACD670A1D600" # optional ﬁlter (recommended)
CHAR_UUID = "12345678-1234-5678-1234-56789abcdef1"
CSV_PATH = "punch.csv"
SCAN_TIMEOUT = 8.0
# ---------------------------

def int16_le(b0, b1):
    return int.from_bytes(bytes([b0, b1]), byteorder="little", signed=True)

def handle_notify(sender, data: bytearray):
    # Expect exactly 8 bytes: ctr + ax + ay + az (int16 little-endian)
    if len(data) != 8:
        try:
            txt = data.decode("utf-8", errors="ignore").strip()
            print("Non-8B packet:", len(data), "as text:", txt)
        except Exception:
            print("Non-8B packet:", len(data), "raw:", list(data))
        return

    ctr = int16_le(data[0], data[1])
    ax_mg = int16_le(data[2], data[3])
    ay_mg = int16_le(data[4], data[5])
    az_mg = int16_le(data[6], data[7])

    ax_g = ax_mg / 1000.0
    ay_g = ay_mg / 1000.0
    az_g = az_mg / 1000.0

    ts = datetime.now().isoformat(timespec="milliseconds")
    line = f"{ts},{ctr},{ax_g:.3f},{ay_g:.3f},{az_g:.3f}"

    print(line)
    with open(CSV_PATH, "a", encoding="utf-8") as f:
        f.write(line + "\n")

def ensure_csv_header():
    try:
        with open(CSV_PATH, "r", encoding="utf-8"):
            pass
    except FileNotFoundError:
        with open(CSV_PATH, "w", encoding="utf-8") as f:
            f.write("timestamp,ctr,ax_g,ay_g,az_g\n")

async def find_target_device():
    print(f"Scanning for BLE devices ({SCAN_TIMEOUT}s)...")
    devices = await BleakScanner.discover(timeout=SCAN_TIMEOUT)

    # Print what macOS sees (addresses look like UUIDs on macOS)
    for d in devices:
        print(f"  - name={d.name!r}  address={d.address}")

    # Prefer service-UUID filter when available (more reliable than name)
    if TARGET_SERVICE_UUID:
        svc = TARGET_SERVICE_UUID.lower()
        print(f"Searching for device advertising service UUID {TARGET_SERVICE_UUID} ...")
        device = await BleakScanner.find_device_by_filter(
            lambda d, ad: svc in [s.lower() for s in (ad.service_uuids or [])],
            timeout=SCAN_TIMEOUT,
        )
        if device:
            return device

    # Fallback: match by name substring
    if TARGET_NAME:
        for d in devices:
            if d.name and TARGET_NAME.lower() in d.name.lower():
                return d

    return None

async def main():
    device = await find_target_device()
    if not device:
        raise RuntimeError(
            "Target device not found.\n"
            "- Make sure it is advertising BLE\n"
            "- Disconnect it from Windows/phone/other centrals\n"
            "- Set TARGET_NAME correctly (must match BLE.setLocalName)\n"
            "- Or verify TARGET_SERVICE_UUID matches your firmware"
        )

    print("Connecting to:", device.name, device.address)
    ensure_csv_header()

    async with BleakClient(device) as client:
        print("Connected:", client.is_connected)

        await client.start_notify(CHAR_UUID, handle_notify)
        print("Streaming ACCEL... Ctrl+C to stop.")
        while True:
            await asyncio.sleep(1)

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nStopped.") 
