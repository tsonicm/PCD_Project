import socket
import os

def sendFile(filename, socket):
    # Send the filename
    socket.send(filename.encode())
    # Send the file size
    socket.send(str(os.path.getsize(filename)).encode())
    # Open the file
    with open(filename, 'r') as f:
        # Send the file in chunks of 1024
        while True:
            data = f.read(1024).encode()
            if not data:
                break
            socket.send(data)
    # Close the file
    f.close()

def receiveFile(filename, socket):
    # Receive the length of the file which is not really needed in this case
    length = int(socket.recv(1024).decode())
    # Send a message to the server to indicate that the client is ready to receive the file
    socket.send('SIGOKRECV'.encode())
    # Create a new file
    with open(filename, 'w') as f:
        # Receive the file in chunks of 1024
        while True:
            data = socket.recv(1024).decode()
            if not data:
                break
            f.write(data)
    # Close the file
    f.close()

def main():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(('localhost', 8080))
    
    # Wait for the server to send a message
    message = s.recv(1024).decode()
    if message == 'SIGFULL':
        print('Server is full')
        s.close()
        return
    elif message == 'SIGOK':
        print('[+]Connected to server.\n')

        print('Enter the name of the file you want to send')
        filename = input()

        # send the file to the server
        sendFile(filename, s)
        print('File sent.')

        # Receive the file from the server
        receiveFile('return.enc', s)
        print('File received.')

        # Close the connection
        s.close()
    else:
        print('Unknown message received')
        s.close()

if __name__ == '__main__':
    main()