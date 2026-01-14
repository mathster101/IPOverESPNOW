import select
import argparse
from cobs import cobs
import TUNDevice
import SerialDevice
import time
import config

def handle_tun_data(tunInterface, serialInterface):
    tunData = tunInterface.read()
    cobsEncoded = cobs.encode(tunData)
    serialInterface.write(cobsEncoded)
    print(f"{time.time()}  {len(tunData)}({len(cobsEncoded)}) Bytes: TUN-->Serial")
    print("#" * 40)


def handle_serial_data(serialInterface, tunInterface):
    serialData = serialInterface.read()
    try:
        decodedMessage = serialData.decode().strip()
    except:
        decodedMessage = ""
    
    if "<DEBUG>" in decodedMessage:
        print(decodedMessage)
        return
    
    if "<PONG>" in decodedMessage:
        try:
            sendTime = float(decodedMessage.split(',')[1])
            latency = ((time.time() - sendTime) / 2) * 1000
            print(f"LINK LATENCY = {latency:.1f}ms")
        except (IndexError, ValueError) as e:
            print(f"Error parsing PONG message: {e}")
        return
    
    if "<PING>" in decodedMessage:
        try:
            sendTime = decodedMessage.split(',')[1]
            pongPacket = f"<PONG>,{sendTime}"
            serialInterface.write(cobs.encode(pongPacket.encode()))
            #print(f"PING received, sending PONG with timestamp {sendTime}")
        except IndexError as e:
            print(f"Error parsing PING message: {e}")
        return
    
    try:
        cobsDecoded = cobs.decode(serialData)
        tunInterface.write(cobsDecoded)
        print(f"{time.time()}  ({len(serialData)}){len(cobsDecoded)} Bytes: Serial-->TUN")
    except Exception as e:
        print(f"error decoding packet-\n{e}")
    print("#" * 40)

def ping(serialInterface):
    pingPacket = f"<PING>,{time.time()}".encode()
    serialInterface.write(cobs.encode(pingPacket))

def main():
    parser = argparse.ArgumentParser(description='Create IP Tunnel over ESP-NOW')
    parser.add_argument('--tun_ip', required=True, help='IP address for the interface')
    parser.add_argument('--serial_port', required=True, help='Serial port the esp32 is connected on. eg: COM3 (WINDOWS), /dev/ttyUSB0 (LINUX)')
    args = parser.parse_args()

    tunInterface = TUNDevice.TUNDevice('nowNet0', args.tun_ip)
    tun_fd = tunInterface.getfd()
    serialInterface = SerialDevice.SerialDevice(args.serial_port, config.BAUDRATE_SERIAL)
    serial_fd = serialInterface.getfd()

    fileDescriptorsToMonitor = [tun_fd, serial_fd]
    last_packet_time = time.time()
    try:
        while True:
            outputs = []
            inputs, _, _ = select.select(fileDescriptorsToMonitor, outputs, fileDescriptorsToMonitor, config.SELECT_TIMEOUT)
            for fd in inputs:
                if fd == tun_fd:
                    handle_tun_data(tunInterface, serialInterface)
                if fd == serial_fd:
                    handle_serial_data(serialInterface, tunInterface)
            if time.time() - last_packet_time > 0.5:
                ping(serialInterface)
                last_packet_time = time.time()

    except KeyboardInterrupt:
        print("shutting down")
    finally:
        tunInterface.closeInterface()
        serialInterface.closeInterface()

if __name__ == '__main__':
    main()