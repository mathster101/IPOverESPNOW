import select
from cobs import cobs
from python import TUNDevice
from python import SerialDevice
from python import config
import time

class Tunnel:
    def __init__(self, tun_ip: str, serial_port: str):
        self.tun_interface = TUNDevice.TUNDevice('nowNet0', tun_ip)
        self.tun_fd = self.tun_interface.getfd()
        
        self.serial_interface = SerialDevice.SerialDevice(
            serial_port, 
            config.BAUDRATE_SERIAL)
        self.serial_fd = self.serial_interface.getfd()
        
        self.file_descriptors_to_monitor = [self.tun_fd, self.serial_fd]
        self.last_packet_time = time.time()

    def tun_to_radio(self):
        """Handle data from TUN interface and forward to serial."""
        try:
            tun_data = self.tun_interface.read()
            cobs_encoded = cobs.encode(tun_data)
            self.serial_interface.write(cobs_encoded)
            #print(f"{time.time():.3f}  {len(tun_data)}({len(cobs_encoded)}) Bytes: TUN-->Serial")
            #print("#" * 40)
        except Exception as e:
            print(f"Error handling TUN data: {e}")

    def handle_control_message(self, message: str) -> bool:
        if "<DEBUG>" in message:
            print(message)
            return True
        
        if "<PONG>" in message:
            try:
                send_time = float(message.split(',')[1])
                latency = ((time.time() - send_time) / 2) * 1000
                print(f"LINK LATENCY = {latency:.1f}ms")
            except (IndexError, ValueError) as e:
                print(f"Error parsing PONG message: {e}")
            return True
        
        if "<PING>" in message:
            try:
                send_time = message.split(',')[1]
                pong_packet = f"<PONG>,{send_time}"
                self.serial_interface.write(cobs.encode(pong_packet.encode()))
            except IndexError as e:
                print(f"Error parsing PING message: {e}")
            return True
        
        return False

    def radio_to_tun(self):
        try:
            serial_data = self.serial_interface.read()
            
            # Try to decode as text for control messages
            try:
                decoded_message = serial_data.decode().strip()
                if self.handle_control_message(decoded_message):
                    return
            except UnicodeDecodeError:
                pass
            
            try:
                cobs_decoded = cobs.decode(serial_data)
                self.tun_interface.write(cobs_decoded)
                #print(f"{time.time():.3f}  ({len(serial_data)}){len(cobs_decoded)} Bytes: Serial-->TUN")
            except Exception as e:
                print(f"Error decoding packet:\n{e}")
                
            #print("#" * 40)
        except Exception as e:
            print(f"Error handling serial data: {e}")

    def ping(self):
        try:
            ping_packet = f"<PING>,{time.time()}".encode()
            self.serial_interface.write(ping_packet)
        except Exception as e:
            print(f"Error sending ping: {e}")

    def process_events(self):
        outputs = []
        inputs, _, _ = select.select(
            self.file_descriptors_to_monitor, 
            outputs, 
            self.file_descriptors_to_monitor, 
            config.SELECT_TIMEOUT
        )
        
        for fd in inputs:
            if fd == self.tun_fd:
                self.tun_to_radio()
            elif fd == self.serial_fd:
                self.radio_to_tun()

    def run(self):
        try:
            while True:
                self.process_events()
                
                # Send periodic ping
                current_time = time.time()
                if current_time - self.last_packet_time > config.PING_INTERVAL:
                    self.ping()
                    self.last_packet_time = current_time
                    
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
