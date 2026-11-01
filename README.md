# ESP32-C3 BLE OTA updater

This project builds a 4 MB ESP32-C3 Arduino firmware with two OTA app slots and the BLE service/characteristics used by the prototype uploader.

## Build on GitHub from a phone
1. Create a new **public or private** GitHub repository.
2. Upload the contents of this folder to the repository root (including `.github/workflows/build.yml`).
3. Open the repository's **Actions** tab and select **Build ESP32-C3 BLE OTA firmware**.
4. If prompted, enable GitHub Actions. Run workflow, or push to `main` to trigger it.
5. Open the successful run and download the artifact named `esp32-c3-ble-ota-firmware`.
6. Extract the ZIP; it contains `firmware.bin`.

## Important
- This is a prototype BLE OTA updater, not a production-secure updater. It has no authentication, signature verification, or cryptographic integrity check. Use only in a controlled test environment.
- The OTA app slot is 0x140000 bytes (1,310,720 bytes); the updater rejects larger firmware images.
- The BLE protocol uses a 4-byte little-endian firmware length, then sequential raw firmware chunks, then any non-empty write to the finish characteristic.
- A BLE client must write chunks in order and wait for each write to complete. Negotiate a suitable MTU and respect the negotiated ATT payload limit.
- This firmware is only the BLE updater. It does not yet include OLED animation playback or animation file storage.
