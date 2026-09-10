using System.Net.Sockets;
using System.Net;
using System.Runtime.InteropServices;
using System.Text;
using static Debug3dsServerConsole.Program;

namespace Debug3dsServerConsole {

    public class ClientTest {

        async Task readHandler() {
            try {
                string str = "";
                while (runHandler) {
                    int bytesRead = await client.Read();
                    if (bytesRead == 0) break; // Client disconnected
                    if (client.readData.isReading) {
                        Log($"Received partial data, {client.readData.currentBytes}/{client.readData.targetBytes} ({(int)(((float)client.readData.currentBytes / (float)client.readData.targetBytes) * 100f)}%) bytes (read {bytesRead}) [{client.name}]", "Client", ConsoleColor.Black, ConsoleColor.White);
                    }
                    else {
                        Log($"Received total data, {client.readData.currentBytes} bytes (read {bytesRead}) ({client.readData.sectionCount} sections) [{client.name}]", "Client", ConsoleColor.Black, ConsoleColor.White);
                    }
                    // process data
                    if (client.curHeader.type == (byte)TCPServer.MSG_TYPE.REQUEST_MEMORY) {
                        byte[] retData = new byte[0x1000];
                        retData[0] = 1;
                        var buffer = TCPServer.CreateSendData(TCPServer.MSG_TYPE.REQUEST_MEMORY, retData, true);
                        var num = await client.Write(buffer);
                        Log($"Wrote {num} bytes", "Client", ConsoleColor.Black, ConsoleColor.White);
                    }
                    if (client.curHeader.type == (byte)TCPServer.MSG_TYPE.TEST) {
                        var buffer = TCPServer.CreateSendData(TCPServer.MSG_TYPE.REQUEST_MEMORY_SELF, Encoding.ASCII.GetBytes("Unimplemented"), true);
                        var num = await client.Write(buffer);
                        Log($"Wrote {num} bytes", "Client", ConsoleColor.Black, ConsoleColor.White);
                    }
                    if (client.curHeader.type == (byte)TCPServer.MSG_TYPE.REQUEST_MEMORY_SELF) {
                        byte[] retData = new byte[0x1000];
                        retData[0] = 2;
                        var buffer = TCPServer.CreateSendData(TCPServer.MSG_TYPE.REQUEST_MEMORY_SELF, retData, true);
                        var num = await client.Write(buffer);
                        Log($"Wrote {num} bytes", "Client", ConsoleColor.Black, ConsoleColor.White);
                    }
                    if (client.curHeader.type == (byte)TCPServer.MSG_TYPE.REQUEST_DIRECTORY) {
                        var readData = (client.readData.sectionCount == 1 ? client.readBuffer.Skip(16) : client.readBuffer).ToArray();
                        int len = 0;
                        while (readData[len++] != 0) { }
                        len--; // 0 is not wanted
                        str += Encoding.ASCII.GetString(readData, 0, len);
                        if (!client.readData.isReading) {
                            Log($"Received file path \"{str}\"");
                            var dirs = str.Split("/", StringSplitOptions.RemoveEmptyEntries);
                            bool flag = archive.CheckPath(dirs, out var folder);
                            if (!flag) {
                                var buffer = TCPServer.CreateSendData(TCPServer.MSG_TYPE.STRING, Encoding.ASCII.GetBytes("Bad Path"), true);
                                var num = await client.Write(buffer);
                                Log($"Wrote {num} bytes", "Client", ConsoleColor.Black, ConsoleColor.White);
                            }
                            else {
                                len = 0;
                                for (int i = 0; i < folder.content.Count; i++) {
                                    len += 8 + folder.content[i].name.Length + 1; // name + (ulong size) + \0
                                }
                                var buffer = TCPServer.CreateSendHeader(0, 0, TCPServer.MSG_TYPE.REQUEST_DIRECTORY, 1, 0, 16 + (uint)len);
                                var num = await client.Write(buffer);
                                byte[] data = new byte[TCPServer.BUFFER_SIZE];
                                var index = 0;
                                for (int i = 0; i < folder.content.Count; i++) {
                                    if (data.Length < index + folder.content[i].name.Length + 8 + 1) {
                                        num += await client.Write(data, index);
                                        index = 0;
                                    }
                                    data[index + 0] = (byte)((folder.content[i].size >> (8 * 0)) & 0xFF);
                                    data[index + 1] = (byte)((folder.content[i].size >> (8 * 1)) & 0xFF);
                                    data[index + 2] = (byte)((folder.content[i].size >> (8 * 2)) & 0xFF);
                                    data[index + 3] = (byte)((folder.content[i].size >> (8 * 3)) & 0xFF);
                                    data[index + 4] = (byte)((folder.content[i].size >> (8 * 4)) & 0xFF);
                                    data[index + 5] = (byte)((folder.content[i].size >> (8 * 5)) & 0xFF);
                                    data[index + 6] = (byte)((folder.content[i].size >> (8 * 6)) & 0xFF);
                                    data[index + 7] = (byte)((folder.content[i].size >> (8 * 7)) & 0xFF);
                                    index += 8;
                                    Array.Copy(Encoding.ASCII.GetBytes(folder.content[i].name), 0, data, index, folder.content[i].name.Length);
                                    index += folder.content[i].name.Length;
                                    data[index] = (folder.content[i].GetType() == typeof(Archive.ArchiveFolder)) ? (byte)1 : (byte)0;
                                    index++;
                                }
                                num += await client.Write(data, index);
                                Log($"Wrote {num} bytes", "Client", ConsoleColor.Black, ConsoleColor.White);
                            }
                            str = "";
                        }
                    }
                    if (client.curHeader.type == (byte)TCPServer.MSG_TYPE.UPLOAD_FILE) {
                        var readData = (client.readData.sectionCount == 1 ? client.readBuffer.Skip(16) : client.readBuffer).ToArray();
                        int len = 0;
                        while (readData[len++] != 0) { }
                        len--; // 0 is not wanted
                        str += Encoding.ASCII.GetString(readData, 0, len);
                        if (!client.readData.isReading) {
                            Log($"Received file name \"{str}\"");
                            var dirs = str.Split("/", StringSplitOptions.RemoveEmptyEntries);
                            bool flag = archive.CheckFile(dirs, out var file);
                            if (!flag) {
                                var buffer = TCPServer.CreateSendData(TCPServer.MSG_TYPE.STRING, Encoding.ASCII.GetBytes("Bad Path"), true);
                                var num = await client.Write(buffer);
                                Log($"Wrote {num} bytes", "Client", ConsoleColor.Black, ConsoleColor.White);
                            }
                            else {
                                var fileSize = file.size;
                                var buffer = TCPServer.CreateSendHeader(0, 0, TCPServer.MSG_TYPE.UPLOAD_FILE, 1, 0, 16 + (uint)fileSize);
                                var num = await client.Write(buffer);
                                byte[] testEntireFileBuffer = archive.GetFile(dirs, file);

                                byte[] fileTestBuffer = new byte[TCPServer.BUFFER_SIZE];
                                ulong index = 0;
                                while (index < fileSize) {
                                    len = (int)Math.Min(TCPServer.BUFFER_SIZE, fileSize - index);
                                    Array.Copy(testEntireFileBuffer, (int)index, fileTestBuffer, 0, len);
                                    num += await client.Write(fileTestBuffer, len);
                                    index += TCPServer.BUFFER_SIZE;
                                }
                                Log($"Wrote {num} bytes", "Client", ConsoleColor.Black, ConsoleColor.White);
                            }
                            str = "";
                        }
                    }
                    if (client.curHeader.type == (byte)TCPServer.MSG_TYPE.DOWNLOAD_FILE) {
                        var buffer = TCPServer.CreateSendData(TCPServer.MSG_TYPE.REQUEST_MEMORY_SELF, Encoding.ASCII.GetBytes("Unimplemented"), true);
                        var num = await client.Write(buffer);
                        Log($"Wrote {num} bytes", "Client", ConsoleColor.Black, ConsoleColor.White);
                    }
                    await Task.Delay(5);
                }
            }
            catch (Exception e) {
                
            }
        }

        public Client client;
        public string IP;
        public int port;
        public Task ConnectionTask;
        public bool connected = false;
        public bool runHandler = false;
        public bool autoConnect = false;
        public Archive archive;

        public bool open = false;

        public ClientTest(string IP, int port, bool autoConnect = false) {
            this.open = true;
            this.IP = IP;
            this.port = port;
            this.autoConnect = autoConnect;
            try {
                archive = new Archive(@"C:\Users\crawm\source\repos\Debug3dsServer\Debug3dsServer\3dsFolder");
            } catch (Exception) {
                archive = new Archive();
                // create similar 3ds stuff
                archive.archive.AddFile("boot.firm", 300_000);
                var luma = archive.archive.AddFolder("luma");
                var cdad = luma.AddFolder("cdad");
                cdad.AddFile("pluginTest.cplg", 3_000);
                var memory = luma.AddFolder("dumps").AddFolder("memory");
                memory.AddFile("game_dump_1.bin", 8_000_000);
            }

        }

        ~ClientTest(){
            Disconnect();
        }

        public async Task Connect() {
            try {
                var _client = new TcpClient();
                await _client.ConnectAsync(IP, port);
                client = new Client(_client);
                Log($"Socket connected [{client.name}]", "Client", ConsoleColor.Black, ConsoleColor.White);
                runHandler = true;
                readHandler();
                connected = true;
            }
            catch (Exception e) {
                
            }
        }
        public void Disconnect() {
            runHandler = false;
            client?.Dispose();
            Log($"Socket disconnected [{client.name}]", "Client", ConsoleColor.Black, ConsoleColor.White);
            connected = false;
        }
    }
    public class Archive {

        public Archive() {
            archive = new ArchiveFolder() {
                name = "SD",
                size = 0
            };
            realPath = null;
        }
        public Archive(string populatePath) {
            archive = new ArchiveFolder() {
                name = "SD",
                size = 0
            };
            realPath = populatePath;
            PopulateFromFolder(populatePath);
        }

        public class ArchiveElement {
            public string name;
            public ulong size;
        }
        public class ArchiveFile : ArchiveElement { }
        public class ArchiveFolder : ArchiveElement {
            public List<ArchiveElement> content = new List<ArchiveElement>();
            public void PopulateFromFolder(string realPath) {
                var dirs = Directory.GetDirectories(realPath);
                for (int i = 0; i < dirs.Length; i++) {
                    var n = dirs[i].Substring(dirs[i].LastIndexOf("\\") + 1);
                    var folder = new ArchiveFolder() {
                        name = n,
                        size = 0
                    };
                    folder.PopulateFromFolder(dirs[i]);
                    content.Add(folder);
                }
                var files = Directory.GetFiles(realPath);
                for (int i = 0; i < files.Length; i++) {
                    var n = files[i].Substring(files[i].LastIndexOf("\\") + 1);
                    content.Add(new ArchiveFile() {
                        name = n,
                        size = (ulong)(new FileInfo(files[i]).Length)
                    });
                }
            }
            public bool CheckPath(string[] path, int depth, out ArchiveFolder folder) {
                folder = null;
                if (path.Length == depth) {
                    folder = this;
                    return path[depth - 1] == this.name;
                }
                var num = 0;
                for (int i = 0; i < content.Count; i++) {
                    if ((content[i] is ArchiveFolder) && content[i].name == path[depth]) {
                        if ((content[i] as ArchiveFolder).CheckPath(path, depth + 1, out folder))
                            return true;
                    }
                }
                return false;
            }
            public bool CheckFile(string[] path, int depth, out ArchiveFile file) {
                file = null;
                var num = 0;
                for (int i = 0; i < content.Count; i++) {
                    if (path.Length == depth + 1) {
                        if ((content[i] is ArchiveFile) && content[i].name == path[depth]) {
                            file = content[i] as ArchiveFile;
                            return true;
                        }
                    }
                    else {
                        if ((content[i] is ArchiveFolder) && content[i].name == path[depth]) {
                            if ((content[i] as ArchiveFolder).CheckFile(path, depth + 1, out file))
                                return true;
                        }
                    }
                }
                return false;
            }

            public ArchiveFile AddFile(string name, ulong size) {
                var file = new ArchiveFile() {
                    name = name,
                    size = size
                };
                content.Add(file);
                return file;
            }
            public ArchiveFolder AddFolder(string name) {
                var folder = new ArchiveFolder() {
                    name = name,
                    size = 0
                };
                content.Add(folder);
                return folder;
            }
        }
        public ArchiveFolder archive;
        public string realPath;
        public void PopulateFromFolder(string realPath) {
            archive.PopulateFromFolder(realPath);
        }
        public bool CheckPath(string[] path, out ArchiveFolder folder) {
            folder = null;
            if (path.Length == 0) {
                folder = archive;
                return true;
            }
            return archive.CheckPath(path, 0, out folder);
        }
        public bool CheckFile(string[] path, out ArchiveFile file) {
            file = null;
            return archive.CheckFile(path, 0, out file);
        }
        public byte[] GetFile(string[] path, ArchiveFile file) {
            if (realPath == null) {
                byte[] arr = new byte[file.size];
                string str = "Test Default Path Fail File Data";
                Array.Copy(Encoding.ASCII.GetBytes(str), arr, str.Length);
                return arr;
            }
            return File.ReadAllBytes(realPath + "\\" + (string.Join("\\", path)));
        }
    }
    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct Header {
        public byte FF;
        public byte type;
        public byte response;
        public byte _reserved;
        public uint size;
        public uint num2;
        public uint num3;
    }
    public struct ReadData {
        public bool isReading;
        public uint sectionCount;
        public uint targetBytes;
        public uint currentBytes;
    }
    public struct WriteData {
        public bool isWriting;
        public uint size;
    }
    public class Client : IDisposable {

        /// <summary>
        /// Connect socket before calling
        /// </summary>
        public Client(TcpClient client) {
            rawClient = client;
            if (client.Client.RemoteEndPoint != null && client.Client.RemoteEndPoint is IPEndPoint loc1) {
                name = loc1.Address + ":" + loc1.Port;
            }
            else if (client.Client.LocalEndPoint != null && client.Client.LocalEndPoint is IPEndPoint loc2) {
                name = loc2.Address + ":" + loc2.Port;
            }
            stream = client.GetStream();
        }
        ~Client() {
            Dispose();
        }

        public byte[] sendBuffer = new byte[TCPServer.BUFFER_SIZE];
        public byte[] readBuffer = new byte[TCPServer.BUFFER_SIZE];

        public string name;
        public TcpClient rawClient;
        public NetworkStream stream;

        public Header curHeader;
        public ReadData readData;
        public WriteData writeData;

        public async Task<int> Read() {
            var readCount = !readData.isReading ? 16 : (int)Math.Min(readData.targetBytes - readData.currentBytes, TCPServer.BUFFER_SIZE);
            int bytesRead = await stream.ReadAsync(readBuffer, 0, readCount);
            if (bytesRead == 0) return 0; // client disconnected
            if (!readData.isReading) { // bufferOut[0] == 0xFF
                curHeader = MemoryMarshal.Read<Header>(readBuffer);
                readData.isReading = true;
                readData.currentBytes = 0;
                readData.sectionCount = 0;
                readData.targetBytes = curHeader.size;
            }
            readData.currentBytes += (uint)bytesRead;
            if (readData.currentBytes >= curHeader.size) {
                readData.isReading = false;
            }
            else {
                readData.isReading = true;
            }
            readData.sectionCount++;
            return bytesRead;
        }
        public async Task<int> Write(byte[] buffer) {
            return await Write(buffer, buffer.Length);
        }
        public async Task<int> Write(byte[] buffer, int length) {
            writeData.isWriting = true;
            int writenBytes = 0;
            uint size = (uint)length;
            writeData.size = size;
            while (size > 0) {
                int len = (int)Math.Min(size, TCPServer.BUFFER_SIZE);
                Array.Copy(buffer, writenBytes, sendBuffer, 0, len);
                await stream.WriteAsync(sendBuffer, 0, len);
                size -= (uint)len;
                writenBytes += len;
            }
            writeData.isWriting = false;
            return writenBytes;
        }

        public bool IsConnected() => rawClient.Connected;

        public override string ToString() {
            return name;
        }

        public void Dispose() {
            stream.Dispose();
            rawClient.Dispose();
        }

    }
    public class TCPServer {

        public const int BUFFER_SIZE = 0x1000;

        public enum MSG_TYPE {
            TEST,
            REQUEST_MEMORY,
            REQUEST_MEMORY_SELF,
            REQUEST_DIRECTORY,
            /// <summary> Take file from 3ds </summary>
            UPLOAD_FILE,
            /// <summary> Give file to 3ds </summary>
            DOWNLOAD_FILE,
            STRING
        };

        public string IP = "0.0.0.0";
        public int PORT = 8001;
        public TcpListener server;
        public Thread serverThread;
        public bool isServerRunning;
        public string fileOutput = "";

        public bool debug = false;

        public delegate void ClientFunc(Client client);
        public delegate void DirReceiveFunc((string, int, bool)[] file);
        public event ClientFunc OnClientConnect;
        public event ClientFunc OnClientDisconnect;
        public List<Client> connected = new List<Client>();

        public string TargetName = "";
        public byte[] TargetData = new byte[16];

        public void SendTo(string name, byte[] buffer) {
            TargetName = name;
            TargetData = buffer;
        }

        public static byte[] CreateSendData(MSG_TYPE type, byte[] data, bool response = false) {
            List<byte> arr = CreateSendHeader(0, 0, type, (byte)(response ? 1 : 0), 0, (uint)(16 + (data == null ? 0 : data.Length))).ToList();
            arr.AddRange(data);
            return arr.ToArray();
        }
        public static byte[] CreateSendHeader(uint b, uint c, MSG_TYPE type, byte response = 0x00, byte _reserved = 0x00, uint size = 16) {
            return new byte[] {
                0xFF, (byte)type, response, _reserved,
                (byte)((size & 0xFF) >> 0), (byte)((size & 0xFF00) >> 8), (byte)((size & 0xFF0000) >> 16), (byte)((size & 0xFF000000) >> 24),
                (byte)((b & 0xFF) >> 0), (byte)((b & 0xFF00) >> 8), (byte)((b & 0xFF0000) >> 16), (byte)((b & 0xFF000000) >> 24),
                (byte)((c & 0xFF) >> 0), (byte)((c & 0xFF00) >> 8), (byte)((c & 0xFF0000) >> 16), (byte)((c & 0xFF000000) >> 24)
            };
        }
        public static bool GetIP(out string IP) {
            IP = "";
            using (Socket socket = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, 0)) {
                socket.Connect("8.8.8.8", 65530);
                IPEndPoint endPoint = socket.LocalEndPoint as IPEndPoint;
                if (endPoint == null || endPoint.Address == null) {
                    return false;
                }
                IP = endPoint?.Address.ToString();
            }
            return true;
        }
        public static TCPServer CreateServer(string IP, int port) {
            var server = new TCPServer();
            server.IP = IP;
            server.PORT = port;
            return server;
        }


        public void StartHost() {
            if (!isServerRunning) {
                serverThread = new Thread(ServerMain);
                serverThread.IsBackground = true; // Make it a background thread
                isServerRunning = true;
                serverThread.Start();
            }
            else {
                Log("Server Already Running!", "Server", ConsoleColor.Green);
            }
        }
        public void StopHost() {
            if (isServerRunning) {
                isServerRunning = false;
                server.Stop();
            }
            else {
                Log("Server already stopped!", "Server", ConsoleColor.Green);
            }
        }
        private void ServerMain() {
            server = new TcpListener(IPAddress.Parse(IP), PORT);
            server.Start();
            Log($"Server Started! ({IP}:{PORT})", "Server", ConsoleColor.Green);
            while (isServerRunning) {
                try {
                    TcpClient client = server.AcceptTcpClient();
                    HandleClient(new Client(client));
                }
                catch (Exception e) {
                    Log($"Server Error\n{e.Message}", "Server", ConsoleColor.Red);
                    if (!isServerRunning) break;
                    throw;
                }
            }
            Log("Server Stopped", "Server", ConsoleColor.Green);
        }

        async Task HandleClient(Client client) {
            connected.Add(client);
            Log($"Client Connected! [{client.name}] (bf_size:{client.rawClient.Client.ReceiveBufferSize})", "Server", ConsoleColor.Cyan);
            OnClientConnect?.Invoke(client);

            try {
                var writeTask = Task.Run(async () => {
                    try {
                        while (client.IsConnected()) {
                            if (TargetName == client.name) {
                                int bytes = await client.Write(TargetData);
                                if (bytes > 0) {
                                    if (debug) Log($"WRITE: {string.Join(' ', TargetData.Select(x => $"{x:X2}"))}", "debug", ConsoleColor.Yellow);
                                    Log($"Sent data, {bytes} bytes [{client.name}]", "Server", ConsoleColor.Green);
                                }
                                TargetName = "";
                            }
                            await Task.Delay(5);
                        }
                    }
                    catch (Exception e) {
                        Log($"Write Error [{client.name}]:\n{e}", "Server", ConsoleColor.Red);
                    }
                });
                List<byte> serverSideBuffer = new List<byte>();
                while (client.IsConnected()) {
                    int bytesRead = await client.Read();
                    if (bytesRead == 0) break; // Client disconnected
                    if (debug) Log($"READ: {string.Join(' ', client.readBuffer.Take(bytesRead).Select(x => $"{x:X2}"))}", "debug", ConsoleColor.Yellow);
                    if (client.readData.sectionCount == 1)
                        serverSideBuffer.Clear();
                    serverSideBuffer.AddRange(client.readBuffer.Take(bytesRead));
                    if (client.readData.isReading) {
                        Log($"Received partial data, {client.readData.currentBytes}/{client.readData.targetBytes} ({(int)(((float)client.readData.currentBytes / (float)client.readData.targetBytes) * 100f)}%) bytes (read {bytesRead}) [{client.name}]", "Server", ConsoleColor.Green);
                    }
                    else {
                        Log($"Received total data, {client.readData.currentBytes} bytes (read {bytesRead}) ({client.readData.sectionCount} sections) [{client.name}]", "Server", ConsoleColor.Green);

                        if (client.curHeader.type == (byte)MSG_TYPE.STRING) {
                            if (serverSideBuffer.Count > 0x10000) {
                                Log($"Data too large to show", "");
                            }
                            else {
                                Log($"{Encoding.ASCII.GetString(serverSideBuffer.Skip(16).ToArray())}", "");
                            }
                        }
                        else if (client.curHeader.type == (byte)MSG_TYPE.REQUEST_DIRECTORY) {
                            List<ulong> size = new List<ulong>();
                            List<string> name = new List<string>();
                            List<bool> isFolder = new List<bool>();
                            for (int i = 16; i < serverSideBuffer.Count; i++) {
                                ulong s = 0;
                                s += (ulong)(serverSideBuffer[i + 0] << (8 * 0));
                                s += (ulong)(serverSideBuffer[i + 1] << (8 * 1));
                                s += (ulong)(serverSideBuffer[i + 2] << (8 * 2));
                                s += (ulong)(serverSideBuffer[i + 3] << (8 * 3));
                                s += (ulong)(serverSideBuffer[i + 4] << (8 * 4));
                                s += (ulong)(serverSideBuffer[i + 5] << (8 * 5));
                                s += (ulong)(serverSideBuffer[i + 6] << (8 * 6));
                                s += (ulong)(serverSideBuffer[i + 7] << (8 * 7));
                                size.Add(s);
                                i += 8;
                                List<byte> strBytes = new List<byte>();
                                while (true) {
                                    if (serverSideBuffer[i] != 0 && serverSideBuffer[i] != 1) {
                                        strBytes.Add(serverSideBuffer[i]);
                                        i++;
                                    }
                                    else break;
                                }
                                name.Add(Encoding.ASCII.GetString(strBytes.ToArray()));
                                isFolder.Add(serverSideBuffer[i] == 1);
                            }
                            for (int i = 0; i < name.Count; i++) {
                                Log($"{name[i]} ({(isFolder[i] ? "folder" : $"{size[i]} bytes")})", "");
                            }
                        }
                        else if (client.curHeader.type == (byte)MSG_TYPE.UPLOAD_FILE) {
                            try {
                                File.WriteAllBytes(fileOutput, serverSideBuffer.Skip(16).ToArray());
                            } catch (Exception e) {
                                Log("File save error\n" + e);
                            }
                        }
                        else {
                            if (serverSideBuffer.Count > 0x10000) {
                                Log($"Data too large to show", "");
                            }
                            else {
                                Log($"{string.Join(' ', serverSideBuffer.Select(x => $"{x:X2}"))}", "");
                            }
                        }

                    }
                    await Task.Delay(5);
                }
            }
            catch (Exception e) {
                Log($"Client Error\n{e.Message}", "Server", ConsoleColor.Red);
            }

            OnClientDisconnect?.Invoke(client);
            connected.Remove(client);
            Log($"Client Disconnected [{client.name}]", "Server", ConsoleColor.Cyan);
        }
    }

}
