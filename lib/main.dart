import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:permission_handler/permission_handler.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Painel Caminhão',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(seedColor: Colors.orange),
      ),
      home: const HomeScreen(),
    );
  }
}

class HomeScreen extends StatefulWidget {
  const HomeScreen({super.key});
  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> {
  // --- BLE ---
  BluetoothDevice? device;
  BluetoothCharacteristic? ledCharacteristic;
  BluetoothCharacteristic? distanciaCharacteristic;
  bool conectado = false;
  bool escaneando = false;
  List<ScanResult> dispositivosEncontrados = [];

  // --- LEDs ---
  bool ledVermelho = false;
  bool ledVerde = false;
  bool ledAzul = false;

  // --- Distância ---
  double distancia = 999;

  // UUIDs
  static const String UUID_SERVICE   = '12345678-1234-1234-1234-123456789abc';
  static const String UUID_LED       = 'abcdefab-1234-1234-1234-abcdefabcdef';
  static const String UUID_DISTANCIA = 'abcdefef-1234-1234-1234-abcdefabcdef';

  // --- Cor do sensor de ré ---
  Color get corDistancia {
    if (distancia < 20) return Colors.red;
    if (distancia < 50) return Colors.orange;
    if (distancia < 100) return Colors.yellow;
    return Colors.green;
  }

  String get statusDistancia {
    if (distancia < 20) return 'ATENÇÃO!';
    if (distancia < 50) return 'Perto';
    if (distancia < 100) return 'Médio';
    if (distancia >= 999) return '---';
    return 'Livre';
  }

  // --- BLE: Escanear ---
  Future<void> escanear() async {
    await Permission.bluetoothScan.request();
    await Permission.bluetoothConnect.request();
    await Permission.location.request();

    setState(() {
      dispositivosEncontrados.clear();
      escaneando = true;
    });

    FlutterBluePlus.startScan(timeout: const Duration(seconds: 5));
    FlutterBluePlus.scanResults.listen((results) {
      setState(() => dispositivosEncontrados = results);
    });

    await Future.delayed(const Duration(seconds: 5));
    setState(() => escaneando = false);
  }

  // --- BLE: Conectar ---
  Future<void> conectar(BluetoothDevice d) async {
    await d.connect(autoConnect: false, mtu: null);
    List<BluetoothService> services = await d.discoverServices();

    for (BluetoothService service in services) {
      if (service.uuid.toString() == UUID_SERVICE) {
        for (BluetoothCharacteristic c in service.characteristics) {
          if (c.uuid.toString() == UUID_LED) ledCharacteristic = c;
          if (c.uuid.toString() == UUID_DISTANCIA) {
            distanciaCharacteristic = c;
            // Ativa notify — ESP32 começa a enviar distância
            await c.setNotifyValue(true);
            c.lastValueStream.listen((value) {
              if (value.isNotEmpty) {
                String texto = String.fromCharCodes(value);
                setState(() {
                  distancia = double.tryParse(texto) ?? 999;
                });
              }
            });
          }
        }
      }
    }

    setState(() {
      device = d;
      conectado = true;
    });
  }

  // --- BLE: Desconectar ---
  Future<void> desconectar() async {
    await device?.disconnect();
    setState(() {
      device = null;
      conectado = false;
      ledCharacteristic = null;
      distanciaCharacteristic = null;
      distancia = 999;
    });
  }

  // --- BLE: Enviar comando LED ---
  Future<void> enviarLed(String cor, bool estado) async {
    if (ledCharacteristic == null) return;
    String comando = 'LED:$cor:${estado ? 1 : 0}';
    await ledCharacteristic!.write(comando.codeUnits);
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: const Color(0xFF1A1A2E),
      appBar: AppBar(
        backgroundColor: const Color(0xFF16213E),
        title: const Text(
          '🚛 Painel Caminhão',
          style: TextStyle(color: Colors.orange, fontWeight: FontWeight.bold),
        ),
        centerTitle: true,
        actions: [
          if (conectado)
            IconButton(
              icon: const Icon(Icons.bluetooth_connected, color: Colors.green),
              onPressed: desconectar,
              tooltip: 'Desconectar',
            ),
        ],
      ),
      body: SingleChildScrollView(
        padding: const EdgeInsets.all(20),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            _secaoBLE(),
            const SizedBox(height: 24),
            _secaoSensorRe(),
            const SizedBox(height: 24),
            _secaoLeds(),
          ],
        ),
      ),
    );
  }

  // --- Widget: BLE ---
  Widget _secaoBLE() {
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: const Color(0xFF16213E),
        borderRadius: BorderRadius.circular(12),
        border: Border.all(
          color: conectado
              ? Colors.green.withOpacity(0.5)
              : Colors.grey.withOpacity(0.3),
        ),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Icon(
                conectado ? Icons.bluetooth_connected : Icons.bluetooth_disabled,
                color: conectado ? Colors.green : Colors.grey,
              ),
              const SizedBox(width: 8),
              Text(
                conectado
                    ? 'ESP32: ${device!.platformName}'
                    : 'ESP32: Desconectado',
                style: TextStyle(
                    color: conectado ? Colors.green : Colors.grey),
              ),
            ],
          ),
          if (!conectado) ...[
            const SizedBox(height: 12),
            ElevatedButton.icon(
              onPressed: escaneando ? null : escanear,
              icon: escaneando
                  ? const SizedBox(
                      width: 16,
                      height: 16,
                      child: CircularProgressIndicator(strokeWidth: 2))
                  : const Icon(Icons.search),
              label: Text(escaneando ? 'Escaneando...' : 'Buscar ESP32'),
              style:
                  ElevatedButton.styleFrom(backgroundColor: Colors.orange),
            ),
            if (dispositivosEncontrados.isNotEmpty) ...[
              const SizedBox(height: 12),
              const Text('Dispositivos encontrados:',
                  style: TextStyle(color: Colors.white70, fontSize: 13)),
              const SizedBox(height: 6),
              ...dispositivosEncontrados.map((r) => ListTile(
                    contentPadding: EdgeInsets.zero,
                    leading:
                        const Icon(Icons.bluetooth, color: Colors.orange),
                    title: Text(
                      r.device.platformName.isEmpty
                          ? 'Sem nome'
                          : r.device.platformName,
                      style: const TextStyle(color: Colors.white),
                    ),
                    subtitle: Text(r.device.remoteId.toString(),
                        style: const TextStyle(
                            color: Colors.white38, fontSize: 11)),
                    onTap: () => conectar(r.device),
                  )),
            ],
          ],
        ],
      ),
    );
  }

  // --- Widget: Sensor de Ré ---
  Widget _secaoSensorRe() {
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.all(20),
      decoration: BoxDecoration(
        color: const Color(0xFF16213E),
        borderRadius: BorderRadius.circular(16),
        border: Border.all(color: corDistancia.withOpacity(0.5)),
      ),
      child: Column(
        children: [
          const Text(
            '📡 Sensor de Ré',
            style: TextStyle(
                color: Colors.white,
                fontSize: 18,
                fontWeight: FontWeight.bold),
          ),
          const SizedBox(height: 20),
          // Círculo de distância
          AnimatedContainer(
            duration: const Duration(milliseconds: 300),
            width: 160,
            height: 160,
            decoration: BoxDecoration(
              shape: BoxShape.circle,
              color: corDistancia.withOpacity(0.15),
              border: Border.all(color: corDistancia, width: 3),
              boxShadow: [
                BoxShadow(
                    color: corDistancia.withOpacity(0.4), blurRadius: 20)
              ],
            ),
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                Text(
                  distancia >= 999
                      ? '---'
                      : '${distancia.toStringAsFixed(0)} cm',
                  style: TextStyle(
                    color: corDistancia,
                    fontSize: 32,
                    fontWeight: FontWeight.bold,
                  ),
                ),
                Text(
                  statusDistancia,
                  style: TextStyle(
                      color: corDistancia.withOpacity(0.8), fontSize: 14),
                ),
              ],
            ),
          ),
          const SizedBox(height: 16),
          // Barra de proximidade
          if (distancia < 999) ...[
            ClipRRect(
              borderRadius: BorderRadius.circular(8),
              child: LinearProgressIndicator(
                value: (1 - (distancia.clamp(0, 200) / 200)),
                backgroundColor: Colors.grey[800],
                valueColor: AlwaysStoppedAnimation<Color>(corDistancia),
                minHeight: 12,
              ),
            ),
            const SizedBox(height: 8),
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: const [
                Text('Longe', style: TextStyle(color: Colors.white38, fontSize: 11)),
                Text('Perto', style: TextStyle(color: Colors.white38, fontSize: 11)),
              ],
            ),
          ],
        ],
      ),
    );
  }

  // --- Widget: LEDs ---
  Widget _secaoLeds() {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        const Text(
          '💡 Indicadores',
          style: TextStyle(
              color: Colors.white,
              fontSize: 18,
              fontWeight: FontWeight.bold),
        ),
        const SizedBox(height: 12),
        Row(
          mainAxisAlignment: MainAxisAlignment.spaceEvenly,
          children: [
            _botaoLed('Alerta', ledVermelho, Colors.red, () async {
              setState(() => ledVermelho = !ledVermelho);
              await enviarLed('VERMELHO', ledVermelho);
            }),
            _botaoLed('Livre', ledVerde, Colors.green, () async {
              setState(() => ledVerde = !ledVerde);
              await enviarLed('VERDE', ledVerde);
            }),
            _botaoLed('Freio', ledAzul, Colors.blue, () async {
              setState(() => ledAzul = !ledAzul);
              await enviarLed('AZUL', ledAzul);
            }),
          ],
        ),
      ],
    );
  }

  // --- Widget: Botão LED ---
  Widget _botaoLed(
      String label, bool ligado, Color cor, VoidCallback onTap) {
    return GestureDetector(
      onTap: onTap,
      child: Column(
        children: [
          AnimatedContainer(
            duration: const Duration(milliseconds: 300),
            width: 70,
            height: 70,
            decoration: BoxDecoration(
              shape: BoxShape.circle,
              color: ligado ? cor : Colors.grey[800],
              boxShadow: ligado
                  ? [BoxShadow(color: cor.withOpacity(0.6), blurRadius: 20)]
                  : [],
            ),
          ),
          const SizedBox(height: 8),
          Text(label,
              style:
                  const TextStyle(color: Colors.white70, fontSize: 12)),
        ],
      ),
    );
  }
}