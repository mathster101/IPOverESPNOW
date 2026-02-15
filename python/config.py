SELECT_TIMEOUT = 3#seconds

MAX_MESSAGE_SIZE_SERIAL = 1470 - 2 # to match the firmware size limit
BAUDRATE_SERIAL = 921600
TIMEOUT_SERIAL = 0.5 # seconds
MTU = MAX_MESSAGE_SIZE_SERIAL - 30 # 30 to be on the safer side