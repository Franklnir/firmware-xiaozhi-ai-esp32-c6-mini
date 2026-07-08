import serial, time
try:
    s = serial.Serial('COM24', 115200, timeout=1)
    t = time.time()
    while time.time() - t < 10:
        l = s.readline()
        if l:
            print(l.decode('utf-8', errors='ignore').strip())
except Exception as e:
    print(f"Error: {e}")
