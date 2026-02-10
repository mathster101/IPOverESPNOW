import select
from cobs import cobs
import TUNDevice
import SerialDevice
import config
import time
import logging

class Tunnel:
    def __init__(self, tun_ip: str, serial_port: str):
        # Setup logging
        logging.basicConfig(
            filename='tunnel_errors.log',
            level=logging.ERROR,
            format='%(asctime)s - %(levelname)s - %(message)s'
        )
        
        self.tun_interface = TUNDevice.TUNDevice('nowNet0', tun_ip)
        self.tun_fd = self.tun_interface.getfd()
        
        self.serial_interface = SerialDevice.SerialDevice(
            serial_port, 
            config.BAUDRATE_SERIAL)
        self.serial_fd = self.serial_interface.getfd()
        
        self.file_descriptors_to_monitor = [self.tun_fd, self.serial_fd]
        self.last_packet_time = time.time()

    def tun_to_radio_serial(self):
        """Handle data from TUN interface and forward to serial."""
        try:
            tun_data = self.tun_interface.read()
            cobs_encoded = cobs.encode(tun_data)
            self.serial_interface.write(cobs_encoded)
            print(f"{time.time():.3f}  {len(tun_data)}({len(cobs_encoded)}) Bytes: TUN-->Serial")
        except Exception as e:
            logging.error(f"Error handling TUN data: {e}")

    def handle_control_message(self, message: str) -> bool:
        if "<DEBUG>" in message:
            print(message)
            return True        
        return False

    def radio_serial_to_tun(self):
        try:
            serial_data = self.serial_interface.read()
            try:
                decoded_message = serial_data.decode().strip()
                if self.handle_control_message(decoded_message):
                    return
            except UnicodeDecodeError:
                pass
            
            try:
                cobs_decoded = cobs.decode(serial_data)
                self.tun_interface.write(cobs_decoded)
                print(f"{time.time():.3f}  ({len(serial_data)}){len(cobs_decoded)} Bytes: Serial-->TUN")
            except Exception as e:
                logging.error(f"Error decoding packet: {e}")
        except Exception as e:
            logging.error(f"Error handling serial data: {e}")

    def process_events(self):
        outputs = []
        inputs, _, _ = select.select(
            self.file_descriptors_to_monitor, 
            outputs, 
            self.file_descriptors_to_monitor, 
            config.SELECT_TIMEOUT)
        
        for fd in inputs:
            if fd == self.tun_fd:
                self.tun_to_radio_serial()
            elif fd == self.serial_fd:
                self.radio_serial_to_tun()

    def run(self):
        try:
            while True:
                self.process_events()
                    
        except KeyboardInterrupt:
            print("\nShutting down...")
        except Exception as e:
            print(f"Unexpected error: {e}")

    def close(self):
        try:
            self.tun_interface.closeInterface()
        except Exception as e:
            print(f"Error closing TUN interface: {e}")
            
        try:
            self.serial_interface.closeInterface()
        except Exception as e:
            print(f"Error closing serial interface: {e}")
