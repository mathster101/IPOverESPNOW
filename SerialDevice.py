import serial
import time
import config

class SerialDevice:
    def __init__(self, name, baudrate):
        self.serial_fd = serial.Serial(
            port=name,
            baudrate=baudrate,
            timeout=config.TIMEOUT_SERIAL,
            write_timeout=config.TIMEOUT_SERIAL,
            rtscts=False,  
            dsrdtr=False,  
            xonxoff=False,
        )
        self.reset()
        
    def reset(self):
        try:
            self.serial_fd.setDTR(False)
            time.sleep(0.001)
            self.serial_fd.setDTR(True)

            print("Reset signal sent to ESP32")
            time.sleep(1)
        except Exception as e:
            print(f"Failed to reset ESP32: {e}")

    def write(self, data):
        if data[-1:] != b"\0":
            data = data + b"\0"
        if len(data) > config.MAX_MESSAGE_SIZE_SERIAL:
            print(
                f"Error: message is too large {len(data)}>{config.MAX_MESSAGE_SIZE_SERIAL}!!!"
            )
            return
        self.serial_fd.write(data)

    def read(self):
        data = self.serial_fd.read_until(b"\0")
        return data.rstrip(b"\0")

    def getfd(self):
        return self.serial_fd

    def closeInterface(self):
        self.serial_fd.close()
