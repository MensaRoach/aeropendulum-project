import serial
import struct
import sys
import argparse
import time

def read_telemetry(port, baudrate):
    try:
        # Configuration matches USART2: 115200, 8-bit data, 1 bit even parity, 1 stop bit
        ser = serial.Serial(
            port=port, 
            baudrate=baudrate, 
            bytesize=serial.EIGHTBITS, 
            parity=serial.PARITY_EVEN, 
            stopbits=serial.STOPBITS_ONE,
            timeout=1
        )
    except Exception as e:
        print(f"Error opening serial port: {e}")
        return

    print(f"Listening on {port} at {baudrate} baud (8E1)...")
    
    buffer = bytearray()
    
    # Define struct format: 7 int16 (little-endian)
    # < = little-endian
    # h = short (2 bytes)
    # 7h = 7 shorts = 14 bytes
    fmt = "<7h"
    struct_size = struct.calcsize(fmt)
    
    while True:
        try:
            # Read bytes until we hit 0x00 (COBS frame delimiter)
            b = ser.read(1)
            if b:
                if b == b'\x00':
                    if len(buffer) > 0:
                        try:
                            # Decode COBS
                            decoded = cobs.decode(bytes(buffer))
                            if len(decoded) == struct_size:
                                unpacked = struct.unpack(fmt, decoded)
                                accel_x, accel_y, accel_z, temp, gyro_x, gyro_y, gyro_z = unpacked
                                
                                # Convert temp to celsius according to MPU6050 datasheet
                                # Temp in C = (TEMP_OUT / 340.0) + 36.53
                                temp_c = (temp / 340.0) + 36.53
                                
                                print(f"Accel: X={accel_x:6d} Y={accel_y:6d} Z={accel_z:6d} | "
                                      f"Gyro: X={gyro_x:6d} Y={gyro_y:6d} Z={gyro_z:6d} | "
                                      f"Temp: {temp_c:5.1f} °C")
                            else:
                                print(f"Warning: Decoded length {len(decoded)} does not match struct size {struct_size}")
                        except cobs.DecodeError:
                            print("COBS Decode Error")
                        except Exception as e:
                            print(f"Unexpected error decoding: {e}")
                            
                        buffer.clear()
                else:
                    buffer.append(b[0])
                    # Protect against buffer overflow if no 0x00 is received
                    if len(buffer) > 1024:
                        print("Buffer overflow, dropping data.")
                        buffer.clear()
        except KeyboardInterrupt:
            print("\nExiting...")
            break
        except Exception as e:
            print(f"Error reading from serial: {e}")
            break

    ser.close()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Read and decode MPU6050 telemetry via serial.")
    parser.add_argument("-p", "--port", required=True, help="Serial port (e.g., COM3 or /dev/ttyUSB0)")
    parser.add_argument("-b", "--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    
    args = parser.parse_args()
    
    try:
        from cobs import cobs
    except ImportError:
        print("Error: The 'cobs' python package is not installed.")
        print("Please install it using: pip install cobs")
        sys.exit(1)
        
    read_telemetry(args.port, args.baud)
