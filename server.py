import socket
import json
from datetime import datetime
import threading

HOST = '192.168.0.156'
PORT = 1234
json_file = "data.json"

def handle_user_input(conn, running_event):
    while running_event.is_set():
        command = input("Ingrese 'start' para iniciar, 'stop' para detener o 'exit' para salir: ")
        if command.lower() == 'start':
            conn.sendall(b"start")
            print("Transmision de datos iniciada.")
        elif command.lower() == 'stop':
            conn.sendall(b"stop")
            print("Transmision de datos detenida.")
        elif command.lower() == 'exit':
            conn.sendall(b"stop")
            print("Finalizando conexion con ESP32.")
            running_event.clear()
            conn.close()
            break
        else:
            print("Comando no reconocido.")

def receive_data(conn, running_event):
    with open(json_file, "a") as f:
        while running_event.is_set():
            try:
                data = conn.recv(1024)
                if not data:
                    print("Conexion cerrada por el ESP32.")
                    running_event.clear()
                    break

                data_json = json.loads(data.decode('utf-8'))
                data_json["timestamp"] = datetime.now().isoformat()
                json.dump(data_json, f)
                f.write("\n")
                print(f"Datos recibidos y almacenados: {data_json}")
            except json.JSONDecodeError:
                print("Error al decodificar JSON.")
            except Exception as e:
                print(f"Error al recibir datos: {e}")
                running_event.clear()
                break

def main():
    while True:
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                s.bind((HOST, PORT))
                s.listen()
                print(f"Servidor escuchando en {HOST}:{PORT}")

                conn, addr = s.accept()
                print(f"Conectado por {addr}")

                running_event = threading.Event()
                running_event.set()

                # Inicia los hilos
                input_thread = threading.Thread(target=handle_user_input, args=(conn, running_event))
                receive_thread = threading.Thread(target=receive_data, args=(conn, running_event))

                input_thread.start()
                receive_thread.start()

                # Espera a que los hilos terminen
                input_thread.join()
                receive_thread.join()

                print("Conexion finalizada, esperando nueva conexion.")
        except Exception as e:
            print(f"Error en el servidor: {e}")
            break

if __name__ == "__main__":
    main()