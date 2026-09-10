using System.Text;
using System.Globalization;
using System.Formats.Tar;

namespace Debug3dsServerConsole
{

    public class Program
    {
        static string GetInput() {
            DoInputText();
            return Console.ReadLine();
        }
        public static void LogClear() {
            Console.Clear();
            DoInputText();
        }
        public static void Log(string str, ConsoleColor c = ConsoleColor.White, ConsoleColor c2 = ConsoleColor.Black) => Log(str, "", c, c2);
        public static void Log(string str, string level, ConsoleColor c = ConsoleColor.White, ConsoleColor c2 = ConsoleColor.Black) {
            Console.ForegroundColor = c;
            Console.BackgroundColor = c2;
            Console.WriteLine("\r" + (level != null && level.Length > 0 ? $"[{level}] " : "") + str);
            Console.ForegroundColor = ConsoleColor.White;
            Console.BackgroundColor = ConsoleColor.Black;
            DoInputText();
        }
        public static void DoInputText(string c = ">") {
            Console.ForegroundColor = ConsoleColor.Magenta;
            Console.BackgroundColor = ConsoleColor.Black;
            Console.Write("\r" + c + " ");
            Console.ForegroundColor = ConsoleColor.White;
        }

        public static async Task ProcessCommand(string cmd) {
            var args = new List<string>();
            var curStr = "";
            bool isStr = false;
            for (int i = 0; i < cmd.Length; i++) {
                if (cmd[i] == '"') {
                    isStr = !isStr;
                }
                else if (!isStr && cmd[i] == ' ') {
                    if (curStr.Length > 0) args.Add(curStr);
                    curStr = "";
                }
                else {
                    curStr += cmd[i];
                }
            }
            if (curStr.Length > 0) args.Add(curStr);

            if (curClient != null) {
                bool foundClient = false;
                for (int i = 0; i < server.connected.Count; i++) {
                    if (server.connected[i].name == curClient.name) {
                        foundClient = true;
                        break;
                    }
                }
                if (!foundClient) curClient = null;
            }

            bool Check(bool _server = true, bool _clients = true, bool _client = true) {
                if (_server && server == null) {
                    Log("Server was not started");
                    return true;
                }
                else if (_clients && server.connected.Count == 0) {
                    Log($"No connected clients");
                    return true;
                }
                else if (_client && curClient == null) {
                    Log($"No client selected");
                    return true;
                }
                return false;
            }

            if (args.Count > 0) {
                if (args[0] == "help") {
                    Log("=== Commands ===");
                    Log("help\n - this message");
                    Log("clear\n - Clears the console");
                    Log("start [optional port]\n - Starts the server");
                    Log("stop\n - Stops the server");
                    Log("debug\n - Toggles some debug stuff");
                    Log("tc\n - Creates a test client, connects it to the server and selects it");
                    Log("clients\n - Prints all connected clients");
                    Log("client\n - Prints current client");
                    Log("client [name or index]\n - Selects a client");
                    Log("test\n - Sends test information");
                    Log("mem [address]\n - Request 4096 bytes of memory from the debugging process (\"0x\" optional, must be hex)");
                    Log("memself [address]\n - Request 4096 bytes of memory from the self process (\"0x\" optional, must be hex)");
                    Log("ls\n - Lists the contents of the base folder");
                    Log("ls [3ds path]\n - Lists the contents of a folder");
                    Log("take [source 3ds path] [destination non-3ds path]\n - Takes a file from the client (Downloads a file)");
                    Log("give [destination 3ds path] [source non-3ds path]\n - Gives a file to the client (Uploads a file)");
                    return;
                }
                if (args[0] == "clear") {
                    if (args.Count > 1) {
                        Log("Too many arguments");
                    }
                    else {
                        LogClear();
                    }
                    return;
                }
                if (args[0] == "start") {
                    if (args.Count == 2) {
                        if (int.TryParse(args[1], out int port) && port <= 0xFFFF) {
                            StartServer(port);
                        }
                        else {
                            Log($"Invalid port \"{args[1]}\"");
                        }
                    }
                    else if (args.Count > 2) {
                        Log("Too many arguments");
                    }
                    else {
                        StartServer(8001);
                    }
                    return;
                }
                if (args[0] == "stop") {
                    if (args.Count > 1) {
                        Log("Too many arguments");
                    }
                    else {
                        StopServer();
                    }
                    return;
                }
                if (args[0] == "debug") {
                    if (args.Count > 1) {
                        Log("Too many arguments");
                    }
                    else {
                        if (server == null) {
                            Log("Server was not started");
                            return;
                        }
                        server.debug = !server.debug;
                        Log($"Debug {(server.debug ? "On" : "Off")}");
                        if (server.debug) {
                            Log($"Features");
                            Log($" - Displays all data sent and received as bytes");
                        }
                    }
                    return;
                }
                if (args[0] == "tc") {
                    if (args.Count > 1) {
                        Log("Too many arguments");
                    }
                    else {
                        if (server == null) {
                            Log("Server was not started");
                            return;
                        }
                        var c = new ClientTest(server.IP, server.PORT);
                        await c.Connect();
                    }
                    return;
                }

                if (args[0] == "clients") {
                    if (args.Count > 1) {
                        Log("Too many arguments");
                    }
                    else {
                        if (server == null) {
                            Log("Server was not started");
                            return;
                        }
                        if (server.connected.Count == 0) {
                            Log($"No connected clients");
                        }
                        else {
                            for (int i = 0; i < server.connected.Count; i++) {
                                Log($"{i}: {server.connected[i]}");
                            }
                        }
                    }
                    return;
                }
                if (args[0] == "client") {
                    if (server == null) {
                        Log("Server was not started");
                        return;
                    }
                    else if (server.connected.Count == 0) {
                        Log($"No connected clients");
                    }
                    else if (args.Count > 2) {
                        Log("Too many arguments");
                    }
                    else if (args.Count == 2) {
                        if (int.TryParse(args[1], out int num)) {
                            if (num >= server.connected.Count) {
                                Log($"Client index too large");
                            }
                            else {
                                curClient = server.connected[num];
                                Log($"Selected client {curClient}");
                                return;
                            }
                        }
                        else {
                            for (int i = 0; i < server.connected.Count; i++) {
                                if (server.connected[i].name == args[1]) {
                                    curClient = server.connected[i];
                                    Log($"Selected client {curClient}");
                                    return;
                                }
                            }
                            Log($"No client with name {args[1]} exists");
                            return;
                        }
                        Log($"Invalid parameter \"{args[1]}\"");
                    }
                    else {
                        if (curClient == null) {
                            Log($"No client selected");
                        }
                        else {
                            Log($"Selected client {curClient}");
                        }
                    }
                    return;
                }

                if (args[0] == "test") {
                    if (Check()) return;
                    if (args.Count == 2) {
                        if (ulong.TryParse(args[1], out ulong num)) {
                            if (num <= 0xFFFFFFFF) {
                                SendTest((uint)num);
                            }
                            else {
                                Log($"Value too large, must be 32 bit");
                            }
                        }
                        else {
                            Log($"Invalid address \"{args[1]}\"");
                        }
                    }
                    else if (args.Count > 2) {
                        Log("Too many arguments");
                    }
                    else {
                        Log("To read memory include a memory address");
                    }
                    return;
                }
                if (args[0] == "mem") {
                    if (Check()) return;
                    if (args.Count == 2) {
                        var raw = Encoding.ASCII.GetBytes(args[1].StartsWith("0x") ? args[1].Substring(2) : args[1]);
                        if (ulong.TryParse(raw, NumberStyles.HexNumber, CultureInfo.InvariantCulture, out ulong num)) {
                            if (num <= 0xFFFFFFFF) {
                                RequestMemory((uint)num, false);
                            }
                            else {
                                Log($"Value too large, must be 32 bit");
                            }
                        }
                        else {
                            Log($"Invalid address \"{args[1]}\"");
                        }
                    }
                    else if (args.Count > 2) {
                        Log("Too many arguments");
                    }
                    else {
                        Log("To read memory include a memory address");
                    }
                    return;
                }
                if (args[0] == "memself") {
                    if (Check()) return;
                    if (args.Count == 2) {
                        var raw = Encoding.ASCII.GetBytes(args[1].StartsWith("0x") ? args[1].Substring(2) : args[1]);
                        if (ulong.TryParse(raw, NumberStyles.HexNumber, CultureInfo.InvariantCulture, out ulong num)) {
                            if (num <= 0xFFFFFFFF) {
                                RequestMemory((uint)num, true);
                            }
                            else {
                                Log($"Value too large, must be 32 bit");
                            }
                        }
                        else {
                            Log($"Invalid address \"{args[1]}\"");
                        }
                    }
                    else if (args.Count > 2) {
                        Log("Too many arguments");
                    }
                    else {
                        Log("To read memory include a memory address");
                    }
                    return;
                }

                if (args[0] == "ls") {
                    if (Check()) return;
                    if (args.Count > 2) {
                        Log("Too many arguments");
                    }
                    if (args.Count == 2) {
                        RequestPath(args[1]);
                    }
                    else {
                        RequestPath();
                    }
                    return;
                }
                if (args[0] == "give") {
                    if (Check()) return;
                    if (args.Count > 3) {
                        Log("Too many arguments");
                    }
                    else if (args.Count < 3) {
                        Log("Too few arguments");
                    }
                    else if (args.Count == 3) {
                        byte[] arr;
                        try {
                            arr = File.ReadAllBytes(args[2]);
                        } catch (Exception e) {
                            Log("File read error\n" + e);
                            return;
                        }
                        DownloadFile(args[1], arr);
                    }
                    return;
                }
                if (args[0] == "take") {
                    if (Check()) return;
                    if (args.Count > 3) {
                        Log("Too many arguments");
                    }
                    else if (args.Count < 3) {
                        Log("Too few arguments");
                    }
                    else if (args.Count == 3) {
                        server.fileOutput = args[2];
                        UploadFile(args[1]);
                    }
                    return;
                }
            }
            if (args.Count == 0) {
                return;
            }
            Log($"Unknown command \"{args[0]}\"");
        }
        
        public static async Task Main(string[] args) {
            DoInputText();
            Log("Hello World!");
            while (true) {
                string userInput = GetInput();
                var arr = userInput.Split(';');
                for (int i = 0; i < arr.Length; i++) {
                    await ProcessCommand(arr[i]);
                }
            }
        }

        public static TCPServer server;
        public static Client curClient;
        public static void StartServer(int port) {
            if (!TCPServer.GetIP(out string IP)) {
                Log($"Error: Failed to get IP");
                return;
            }

            Log($"Starting server {IP}:{port}");
            if (server == null) {
                server = TCPServer.CreateServer(IP, port);
            }
            server.IP = IP;
            server.PORT = port;
            server.StartHost();
        }
        public static void StopServer() {
            server.StopHost();
        }

        public static void SendTest(uint value) {
            server.SendTo(curClient.name,
                TCPServer.CreateSendHeader(value, 0, TCPServer.MSG_TYPE.TEST)
            );
        }
        public static void RequestMemory(uint address, bool self) {
            server.SendTo(curClient.name,
                TCPServer.CreateSendHeader(address, 0, self ? TCPServer.MSG_TYPE.REQUEST_MEMORY_SELF : TCPServer.MSG_TYPE.REQUEST_MEMORY)
            );
        }
        public static void RequestPath(string path = "/") {
            var data = Encoding.ASCII.GetBytes(path).ToList();
            data.Add(0);
            server.SendTo(curClient.name,
                TCPServer.CreateSendData(TCPServer.MSG_TYPE.REQUEST_DIRECTORY, data.ToArray())
            );
        }
        public static void DownloadFile(string path, byte[] fileData) {
            var data = Encoding.ASCII.GetBytes(path).ToList();
            data.Add(0);
            data.AddRange(fileData);
            server.SendTo(curClient.name,
                TCPServer.CreateSendData(TCPServer.MSG_TYPE.DOWNLOAD_FILE, data.ToArray())
            );
        }
        public static void UploadFile(string path) {
            var data = Encoding.ASCII.GetBytes(path).ToList();
            data.Add(0);
            server.SendTo(curClient.name,
                TCPServer.CreateSendData(TCPServer.MSG_TYPE.UPLOAD_FILE, data.ToArray())
            );
        }

    }
}
