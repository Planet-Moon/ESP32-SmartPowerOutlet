import socket

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((socket.gethostname(), 8885))

full_message = ""
while True:
    msg = s.recv(2)
    if len(msg) <= 0:
        break
    str_ = msg.decode("utf-8")
    print(str_)
    full_message += str_

print(full_message.decode("utf-8"))
s.close()
