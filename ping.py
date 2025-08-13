import socket
from concurrent.futures import ThreadPoolExecutor

# 目標 IP
target_ip = "119.14.115.106"

# 要掃描的端口範圍
ports = range(1, 1025)  # 1~1024 常用端口

# 掃描單一端口
def scan_port(port):
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(0.5)  # 超時時間
        result = s.connect_ex((target_ip, port))
        s.close()
        if result == 0:
            print(f"[+] Port {port} 開放")
    except Exception as e:
        pass

# 使用多線程加速掃描
with ThreadPoolExecutor(max_workers=100) as executor:
    executor.map(scan_port, ports)

print("掃描完成！")
