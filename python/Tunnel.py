import select
from cobs import cobs
import TUNDevice
import SerialDevice
import config
import time
import logging
import os
import lz4.block

class Tunnel:
    def __init__(self, tun_ip: str, serial_port: str):
        # Delete log file if it exists
        if os.path.exists('tunnel_errors.log'):
            os.remove('tunnel_errors.log')
        
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

    def tun_to_radio_serial(self):
        """Handle data from TUN interface and forward to serial."""
        try:
            tun_data_raw = self.tun_interface.read()
            tun_data_compressed = lz4.block.compress(tun_data_raw)
            if len(tun_data_compressed) < len(tun_data_raw):
                tun_data_packed = b'\x01' + tun_data_compressed
            else:
                tun_data_packed = b'\x00' + tun_data_raw
            cobs_encoded = cobs.encode(tun_data_packed)
            self.serial_interface.write(cobs_encoded)
            #print(f"{time.time():.3f}  {len(tun_data_raw)}({len(cobs_encoded)}) Bytes: TUN-->Serial")
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
                if len(cobs_decoded) > 0:
                    compression_flag = cobs_decoded[0]
                    data_payload = cobs_decoded[1:]
                    
                    if compression_flag == 0x01:
                        try:
                            decompressed_data = lz4.block.decompress(data_payload)
                            self.tun_interface.write(decompressed_data)
                        except Exception as e:
                            logging.error(f"Error decompressing data: {e}")
                    elif compression_flag == 0x00:  # Raw data
                        self.tun_interface.write(data_payload)
                    else:
                        logging.error(f"Invalid compression flag: {compression_flag}")
                else:
                    logging.error("Empty packet received")
                #print(f"{time.time():.3f}  ({len(serial_data)}){len(cobs_decoded)} Bytes: Serial-->TUN")
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
