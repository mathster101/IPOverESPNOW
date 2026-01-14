import argparse
from python import Tunnel


def main():
    parser = argparse.ArgumentParser(description='Create IP Tunnel over ESP-NOW')
    parser.add_argument('--tun_ip', required=True, 
                       help='IP address for the interface')
    parser.add_argument('--serial_port', required=True, 
                       help='Serial port the esp32 is connected on. eg: COM3 (WINDOWS), /dev/ttyUSB0 (LINUX)')
    args = parser.parse_args()

    tunnel = Tunnel.Tunnel(args.tun_ip, args.serial_port)
    
    try:
        tunnel.run()
    finally:
        tunnel.close()


if __name__ == '__main__':
    main()