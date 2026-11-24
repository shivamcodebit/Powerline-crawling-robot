import serial
import time

print("Opening COM9 at 115200 baud...")
try:
    ser = serial.Serial('COM9', 115200, timeout=1.0)
    print("Opened COM9 successfully!")
    time.sleep(1.0)
    
    # Read anything for 2 seconds
    start = time.time()
    received = b""
    while time.time() - start < 2.0:
        if ser.in_waiting > 0:
            received += ser.read(ser.in_waiting)
        time.sleep(0.05)
        
    print(f"Received {len(received)} bytes: {received}")
    
    if len(received) == 0:
        print("Sending ping...")
        ser.write(b'{"type":"ping","seq":1}\n')
        time.sleep(0.5)
        resp = ser.read(ser.in_waiting)
        print(f"Response to ping: {resp}")
        
    ser.close()
except Exception as e:
    print(f"Exception: {e}")
