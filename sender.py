import socket
import time
#im.show()

#data = list(im.getdata())
#data = bytes([0x2F, 0x2F, 0x2F])
HOST = "192.168.11.2"  # The server's hostname or IP address
PORT = 5000  # The port used by the server

preamble = bytes([0xAA, 0x55, 0xAA, 0x55])
message_type = bytes([0x01, 0x01])
reserved = bytes([0x00, 0x00])
filler = [0x0]


def socket_action():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((HOST, PORT))
        s.sendall(data)




def constructMessage(frame_number, chunk_number, payload):
    payload_len = len(payload)
    payload_len = hex(payload_len)
    payload_len = formatter(payload_len)
    
    
    frame_len = hex(int(payload_len) + 20)
    frame = preamble + message_type + reserved + frame_number + frame_len + chunk_number + reserved + hex(0) + payload_len + payload
    print(frame + "\n")
    
def formatter(value):
    return [0x0, data]

import socket

# Define the server address and port
server_address = ("192.168.11.2", 5000)

# Create a socket and connect to the server
client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
client_socket.connect(server_address)

# List of integer values
int_values = [170, 85, 170, 85, 0, 1, 3, 1, 0, 0, 255, 0, 255]

try:
    for value in int_values:
        # Convert the integer to bytes (adjust the byte size as needed)
        byte_data = value.to_bytes(1, byteorder='big')  # Assuming 4 bytes per integer (adjust as needed)
        
        # Send the bytes over the socket
        print(byte_data)
        client_socket.send(byte_data)

finally:
    # Close the socket when done
    client_socket.close()


#constructMessage(hex(0), hex(0), data)