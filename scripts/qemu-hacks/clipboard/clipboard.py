import struct
import socket
import asyncio
import uvloop
import os
import subprocess
import threading
import logging
import time
import traceback
import signal

# Set uvloop as the event loop
asyncio.set_event_loop_policy(uvloop.EventLoopPolicy())

logging.basicConfig(level=logging.DEBUG)

logging.debug("Starting clipboard server")

HOST = 'localhost'
PORT = 4444
INTERNAL_PORT=4445
INTERNAL_SOCKET_PATH = '/tmp/virtio-vdagent-clipboard'

# Global variable to store the last clipboard data
clipboard_data = ""

stop_event = threading.Event()
async_stop_event = asyncio.Event()

def clipboard_watcher():
    while not stop_event.is_set():
        # wait until internal socket is created
        while not os.path.exists(INTERNAL_SOCKET_PATH):
            time.sleep(1)

            if stop_event.is_set():
                return

        try:
            logging.debug("Starting clipboard watcher")
            # Run wl-paste with `-w` to monitor clipboard changes
            process = subprocess.Popen(
                ["wl-paste", "-t", "text", "-w", "nc", "-q", "1", "-U", INTERNAL_SOCKET_PATH]
            )

            # Wait for clipboard content; stop if stop_event is set
            while not stop_event.is_set():
                if process.poll() is not None:  # Process finished
                    break
                stop_event.wait(0.1)  # Check every 100 ms
            # Terminate subprocess if still running
            if process.poll() is None:
                process.terminate()
                process.wait()

        except Exception as e:
            logging.error(f"Exception in clipboard watcher: {e}")
            continue

    logging.debug("Clipboard watcher stopped")

# Function to handle graceful shutdown on app exit
def shutdown(signum, frame):
    logging.info("Shutting down...")
    stop_event.set()  # Signal thread to stop
    async_stop_event.set()  # Signal async tasks to stop

# Set up signal handling for graceful shutdown
signal.signal(signal.SIGINT, shutdown)
signal.signal(signal.SIGTERM, shutdown)

# Start the clipboard watcher in a separate daemon thread
watcher_thread = threading.Thread(target=clipboard_watcher, daemon=True)
watcher_thread.start()


VDP_CLIENT_PORT = 1
VD_AGENT_PROTOCOL = 1
VD_AGENT_MAX_DATA_SIZE = 2048

VD_AGENT_CLIPBOARD = 4
VD_AGENT_ANNOUNCE_CAPABILITIES = 6
VD_AGENT_CLIPBOARD_GRAB = 7
VD_AGENT_CLIPBOARD_REQUEST = 8
VD_AGENT_CLIPBOARD_RELEASE = 9
VD_AGENT_MAX_CLIPBOARD = 14


VD_AGENT_CAP_CLIPBOARD = 1 << 3
VD_AGENT_CAP_CLIPBOARD_BY_DEMAND = 1<< 5
VD_AGENT_CAP_CLIPBOARD_SELECTION = 1 << 6
VD_AGENT_CAP_GUEST_LINEEND_LF = 1 << 8
VD_AGENT_CAP_MAX_CLIPBOARD = 1 << 10
VD_AGENT_CAP_CLIPBOARD_NO_RELEASE_ON_REGRAB = 1 << 16
VD_AGENT_CAP_CLIPBOARD_GRAB_SERIAL = 1 << 17

class PackabeleData:
    def pack(self):
        pass

class VDAgentMessage(PackabeleData):
    def __init__(self, msg_type, content: PackabeleData, opaque=0):
        self.protocol = VD_AGENT_PROTOCOL
        self.msg_type = msg_type
        self.opaque = opaque
        self.data = content.pack()

    def pack(self):
        return struct.pack("<IIQI", self.protocol, self.msg_type, self.opaque, len(self.data)) + self.data

    async def send(self, writer):
        msg = self.pack()
        msg_len = len(msg)

        logging.debug(f"Sending message of {msg_len} bytes")

        while msg_len > 0:
            if msg_len > (VD_AGENT_MAX_DATA_SIZE - 8):
                chunk_len = VD_AGENT_MAX_DATA_SIZE - 8
            else:
                chunk_len = msg_len

            writer.write(struct.pack("<II", VDP_CLIENT_PORT, chunk_len) + msg[:chunk_len])
            await writer.drain()
            msg = msg[chunk_len:]
            msg_len -= chunk_len
            logging.debug(f"Sent chunk of {chunk_len} bytes")

        logging.debug(f"Messsage sent successfully")

class VDAgentAnnounceCapabilities(PackabeleData):
    def __init__(self, request, caps):
        self.request = request
        self.caps = caps

    def pack(self):
        return struct.pack("<II", self.request, self.caps)

class VDAgentRequestClipboard(PackabeleData):
    def __init__(self, selection, cb_type):
        self.selection = selection
        self.cb_type = cb_type

    def pack(self):
        return struct.pack("<II", self.selection, self.cb_type)

async def handle_announce_capabilities(data, writer):
    request, caps = struct.unpack("<II", data)

    if request == 1:
        request = True

    logging.debug(f"Announce capabilities: {caps}, need resopnse: {request}")

    if caps & VD_AGENT_CAP_CLIPBOARD:
        print("Clipboard supported")
    if caps & VD_AGENT_CAP_CLIPBOARD_BY_DEMAND:
        print("Clipboard by demand supported")
    if caps & VD_AGENT_CAP_CLIPBOARD_SELECTION:
        print("Clipboard selection supported")
    if caps & VD_AGENT_CAP_GUEST_LINEEND_LF:
        print("Guest lineend LF supported")
    if caps & VD_AGENT_CAP_MAX_CLIPBOARD:
        print("Max clipboard supported")
    if caps & VD_AGENT_CAP_CLIPBOARD_NO_RELEASE_ON_REGRAB:
        print("Clipboard no release on regrab supported")
    if caps & VD_AGENT_CAP_CLIPBOARD_GRAB_SERIAL:
        print("Clipboard grab serial supported")

    if request:
        resp_cap = VDAgentAnnounceCapabilities(0, VD_AGENT_CAP_CLIPBOARD_BY_DEMAND | VD_AGENT_CAP_CLIPBOARD_SELECTION | VD_AGENT_CAP_CLIPBOARD_GRAB_SERIAL)
        resp_msg = VDAgentMessage(VD_AGENT_ANNOUNCE_CAPABILITIES, resp_cap)
        await resp_msg.send(writer)


VD_AGENT_CLIPBOARD_NONE = 0,
VD_AGENT_CLIPBOARD_UTF8_TEXT = 1
VD_AGENT_CLIPBOARD_IMAGE_PNG = 2
VD_AGENT_CLIPBOARD_IMAGE_BMP = 3
VD_AGENT_CLIPBOARD_IMAGE_TIFF = 4
VD_AGENT_CLIPBOARD_IMAGE_JPG = 5
VD_AGENT_CLIPBOARD_FILE_LIST = 6

async def handle_clipboard(data, writer):
    global clipboard_data
    # first 8 bytes header, remaining data is the content
    header, data = data[:8], data[8:]
    selection, content_type = struct.unpack("<II", header)

    logging.debug(f"Clipboard data received. selection: {selection}, content type: {content_type}")

    if content_type == VD_AGENT_CLIPBOARD_UTF8_TEXT:
        # send data to wl-copy with content type text/plain;charset=utf-8
        # wl-copy -t text/plain;charset=utf-8 <payload>
        if data == clipboard_data:
            logging.debug("Clipboard data is same as previous, skipping")
            return

        clipboard_data = data
        os.system(f"echo -n {data.decode('utf-8')} | wl-copy -t text/plain;charset=utf-8")
        logging.debug(f"Clipboard data posted to wl-copy")
    else:
        logging.warning(f"Unsupported content type: {content_type}")

async def handle_clipboard_request(data, writer):
    logging.warning("Clipboard request not implemented")

async def handle_clipboard_grab(data, writer):
    # 4 bytes selection 4 bytes serial and n times 4 bytes type
    selection, serial, *types = struct.unpack("<II" + "I" * ((len(data) - 8) // 4), data)
    logging.debug(f"Clipboard grab: selection: {selection}, serial: {serial}, types: {types}")
    # travel types and check if there is utf8 text type
    found = False
    for t in types:
        if t == VD_AGENT_CLIPBOARD_UTF8_TEXT:
            found = True
            break

    if found:
        logging.debug("Supported type found, sending clipboard request")
        reqclip = VDAgentRequestClipboard(selection, VD_AGENT_CLIPBOARD_UTF8_TEXT)
        reqclip_msg = VDAgentMessage(VD_AGENT_CLIPBOARD_REQUEST, reqclip)
        await reqclip_msg.send(writer)
    else:
        logging.warning("No supported type found")

async def handle_clipboard_release(data, writer):
    logging.warning("Clipboard release not implemented")

async def handle_max_clipboard(data, writer):
    logging.warning("Max clipboard not implemented")

async def handle_message(msg_type, opaque, data, writer):
    if msg_type == VD_AGENT_ANNOUNCE_CAPABILITIES:
        await handle_announce_capabilities(data, writer)
    elif msg_type == VD_AGENT_CLIPBOARD:
        await handle_clipboard(data, writer)
    elif msg_type == VD_AGENT_CLIPBOARD_REQUEST:
        await handle_clipboard_request(data, writer)
    elif msg_type == VD_AGENT_CLIPBOARD_GRAB:
        await handle_clipboard_grab(data, writer)
    elif msg_type == VD_AGENT_CLIPBOARD_RELEASE:
        await handle_clipboard_release(data, writer)
    elif msg_type == VD_AGENT_MAX_CLIPBOARD:
        await handle_max_clipboard(data, writer)
    else:
        logging.warning(f"Unknown message type: {msg_type}")


async def read_data(reader,length):
    buffer = b''
    while length > 0:
        data = await reader.read(length)

        if data == b'':
            return None

        buffer += data

        length -= len(data)

    return buffer

async def read_chunk_header(reader):
    header = await read_data(reader, 8)

    if not header:
        return None, -1

    return struct.unpack("<II", header)

async def read_message_header(reader):
    header = await read_data(reader, 20)

    if not header:
        return None, None, None, -1

    # Protocol (4 bytes), Message Type (4 bytes), Opaque (8 bytes), Message Size (4 bytes)
    return struct.unpack("<IIQI", header)


async def handle_client(reader, writer):
    client_address = writer.get_extra_info('peername')
    logging.debug(f"Connection established with {client_address}")

    try:
        buffer = b''
        message_header_read = False
        message_size = 0

        while True:
           port,chunk_size = await read_chunk_header(reader)

           print(f"Port: {port}, Size: {chunk_size}")

           if chunk_size == -1:
               break

           if not message_header_read:
               protocol, msg_type, opaque, message_size = await read_message_header(reader)

               if message_size == -1:
                   break

               message_header_read = True

               if message_size + 20 == chunk_size:
                   remaining_size = 0
                   read_size = message_size
                   single_packet = True
               else:
                   remaining_size = message_size - (chunk_size - 20)
                   read_size = chunk_size - 20
                   single_packet = False

           if message_header_read and not single_packet:
               remaining_size -= chunk_size
               read_size = chunk_size

           logging.debug(f"message size: {message_size}, remaining size: {remaining_size}, read size: {read_size}, single packet: {single_packet}")

           buffer += await read_data(reader, read_size)

           if remaining_size == 0:
               # dump as hex with space and 16 columns
               for i in range(0, len(buffer), 16):
                   print(" ".join(f"{c:02x}" for c in buffer[i:i+16]))

               print("")

               await handle_message(msg_type, opaque, buffer, writer)
               buffer = b''
               message_header_read = False
               message_size = 0

        logging.debug(f"Connection with {client_address} was closed.")

    except asyncio.CancelledError:
        logging.debug(f"Connection with {client_address} was cancelled.")
    finally:
        # Close the writer when done
        writer.close()
        await writer.wait_closed()

async def handle_clipboard_watch(reader, writer):
    global clipboard_data
    logging.debug("new clipboard watch connection")

    buffer = b''

    while True:
        try:
            data = await asyncio.wait_for(reader.read(4096), timeout=60)

            if not data:
                logging.debug("peer closed connection")
                break

            buffer += data

        except asyncio.TimeoutError:
            logging.debug("Clipboard watch timeout")
            break

    if len(buffer) and buffer != clipboard_data:
        clipboard_data = buffer
        logging.debug(f"Clipboard updated:\n{clipboard_data}")

    logging.debug("Closing clipboard watch connection")
    writer.close()
    await writer.wait_closed()
    logging.debug("Clipboard watch connection closed")


async def start_server(server):
    """Start a server and keep it running until async_stop_event is set."""
    async with server:
        try:
            await server.serve_forever()
        except asyncio.CancelledError:
            pass  # Handle server cancellation if needed


async def tcp_server():
    server = await asyncio.start_server(
        handle_client, HOST, PORT)

    addr = server.sockets[0].getsockname()
    logging.info(f'Serving on {addr}')

    await start_server(server) # Start the server and keep it running

async def tcp_internal_server():
    server = await asyncio.start_server(
        handle_clipboard_watch, HOST, INTERNAL_PORT)

    addr = server.sockets[0].getsockname()
    logging.info(f'Serving on {addr}')

    await start_server(server) # Start the server and keep it running 

async def unix_server():
    if os.path.exists(INTERNAL_SOCKET_PATH):
        os.unlink(INTERNAL_SOCKET_PATH)

    server = await asyncio.start_unix_server(
        handle_clipboard_watch, path=INTERNAL_SOCKET_PATH)

    addr = server.sockets[0].getsockname()
    logging.info(f'Serving on {addr}')

    await start_server(server) # Start the server and keep it running

    os.unlink(INTERNAL_SOCKET_PATH)

async def main():
    tcp_task = asyncio.create_task(tcp_server())
    #internal_task = asyncio.create_task(tcp_internal_server())
    internal_task = asyncio.create_task(unix_server())

    await async_stop_event.wait()

    tcp_task.cancel()
    internal_task.cancel()

    await asyncio.gather(
        tcp_task, internal_task,
        return_exceptions=True
    )


try:
    asyncio.run(main())
except KeyboardInterrupt:
    shutdown(None, None)
    logging.info("\nServer shut down.")
