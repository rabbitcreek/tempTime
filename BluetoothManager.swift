import Foundation
import CoreBluetooth
import Combine

class BluetoothManager: NSObject, ObservableObject {
    static let shared = BluetoothManager()
    
    // Heart Rate Monitor Service and Characteristic UUIDs (matching ESP32)
    private let heartRateServiceUUID = CBUUID(string: "180D")  // Heart Rate Service
    private let heartRateMeasurementUUID = CBUUID(string: "2A37")  // Heart Rate Measurement
    private let sensorPositionUUID = CBUUID(string: "2A38")  // Body Sensor Location
    
    // Published properties for UI updates
    @Published var isConnected = false
    @Published var isScanning = false
    @Published var currentTemperature: Double = 0.0
    @Published var currentHumidity: Double = 0.0
    @Published var connectionStatus = "Disconnected"
    @Published var lastUpdateTime: Date?
    
    // BLE properties
    private var centralManager: CBCentralManager!
    private var connectedPeripheral: CBPeripheral?
    private var heartRateCharacteristic: CBCharacteristic?
    private var sensorPositionCharacteristic: CBCharacteristic?
    
    // Reconnection handling
    private var reconnectTimer: Timer?
    private let reconnectInterval: TimeInterval = 5.0
    private var shouldReconnect = false
    private var lastKnownPeripheral: CBPeripheral?
    private var reconnectAttempts = 0
    private let maxReconnectAttempts = 100  // Keep trying for a long time
    
    private override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: nil)
    }
    
    // MARK: - Public Methods
    
    func startScanning() {
        guard centralManager.state == .poweredOn else {
            connectionStatus = "Bluetooth not available"
            return
        }
        
        isScanning = true
        connectionStatus = "Scanning for ESP32..."
        
        // Scan for Heart Rate Monitor devices (our ESP32 appears as one)
        // Also scan for all peripherals to catch the device name
        centralManager.scanForPeripherals(withServices: [heartRateServiceUUID], options: [
            CBCentralManagerScanOptionAllowDuplicatesKey: false
        ])
        
        // Also scan without service filter to catch by device name
        DispatchQueue.main.asyncAfter(deadline: .now() + 1.0) {
            if self.isScanning {
                self.centralManager.scanForPeripherals(withServices: nil, options: [
                    CBCentralManagerScanOptionAllowDuplicatesKey: false
                ])
            }
        }
        
        // Stop scanning after 30 seconds if no device found
        DispatchQueue.main.asyncAfter(deadline: .now() + 30) {
            if self.isScanning {
                self.stopScanning()
                self.connectionStatus = "ESP32 not found. Will retry..."
                self.startReconnectionTimer()
            }
        }
    }
    
    func stopScanning() {
        centralManager.stopScan()
        isScanning = false
    }
    
    func disconnect() {
        shouldReconnect = false
        reconnectTimer?.invalidate()
        
        if let peripheral = connectedPeripheral {
            centralManager.cancelPeripheralConnection(peripheral)
        }
    }
    
    // MARK: - Private Methods
    
    private func connectToPeripheral(_ peripheral: CBPeripheral) {
        connectedPeripheral = peripheral
        lastKnownPeripheral = peripheral  // Remember this peripheral for reconnection
        peripheral.delegate = self
        centralManager.connect(peripheral, options: nil)
        connectionStatus = "Connecting to ESP32..."
        reconnectAttempts = 0  // Reset attempts on new connection
    }
    
    private func startReconnectionTimer() {
        guard shouldReconnect else { return }
        
        reconnectTimer?.invalidate()
        reconnectTimer = Timer.scheduledTimer(withTimeInterval: reconnectInterval, repeats: true) { [weak self] _ in
            guard let self = self else { return }
            
            self.reconnectAttempts += 1
            
            if self.reconnectAttempts <= self.maxReconnectAttempts {
                print("Reconnection attempt \(self.reconnectAttempts)/\(self.maxReconnectAttempts)")
                self.connectionStatus = "Reconnecting... (attempt \(self.reconnectAttempts))"
                
                // Try to reconnect to last known peripheral first
                if let lastPeripheral = self.lastKnownPeripheral {
                    print("Attempting to reconnect to last known peripheral")
                    self.centralManager.connect(lastPeripheral, options: nil)
                }
                
                // Also start scanning for new devices
                self.startScanning()
            } else {
                print("Max reconnection attempts reached")
                self.connectionStatus = "Unable to reconnect. Please restart app."
                self.stopReconnectionTimer()
            }
        }
    }
    
    private func stopReconnectionTimer() {
        reconnectTimer?.invalidate()
        reconnectTimer = nil
    }
    
    private func handleHeartRateData(_ data: Data) {
        guard data.count >= 2 else { return }
        
        // Parse Heart Rate Monitor data format
        // Byte 0: Flags (0b00001110 = 16-bit value, sensor contact detected)
        // Byte 1: Heart Rate Value (our temperature in Fahrenheit)
        let flags = data[0]
        let temperatureF = Double(data[1])  // Temperature in Fahrenheit from heart[1]
        
        // Convert to Celsius
        let temperatureC = (temperatureF - 32.0) * 5.0 / 9.0
        
        // For humidity, we'll use a default value since ESP32 doesn't send it
        let humidity = 45.0  // Default humidity value
        
        DispatchQueue.main.async {
            self.currentTemperature = temperatureC
            self.currentHumidity = humidity
            self.lastUpdateTime = Date()
            
            // Save to data manager
            DataManager.shared.saveTemperature(temperatureC, humidity: humidity)
        }
        
        print("Received temperature: \(temperatureF)°F (\(temperatureC)°C) from Heart Rate Monitor")
    }
}

// MARK: - CBCentralManagerDelegate

extension BluetoothManager: CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOn:
            connectionStatus = "Ready to connect"
            if shouldReconnect {
                startScanning()
            }
        case .poweredOff:
            connectionStatus = "Bluetooth is off"
            isConnected = false
        case .unauthorized:
            connectionStatus = "Bluetooth permission denied"
        case .unsupported:
            connectionStatus = "Bluetooth not supported"
        case .resetting:
            connectionStatus = "Bluetooth resetting"
        case .unknown:
            connectionStatus = "Bluetooth state unknown"
        @unknown default:
            connectionStatus = "Bluetooth state unknown"
        }
    }
    
    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String : Any], rssi RSSI: NSNumber) {
        print("Discovered device: \(peripheral.name ?? "Unknown")")
        print("Advertisement data: \(advertisementData)")
        
        // Look for our specific device name or Heart Rate service
        let isKelvynDevice = peripheral.name?.contains("Kelvyn") == true || 
                           peripheral.name?.contains("KelvynTemp") == true ||
                           peripheral.name?.contains("FT7") == true
        
        // Also check if it's advertising Heart Rate service
        let hasHeartRateService = advertisementData["kCBAdvDataServiceUUIDs"] != nil
        
        if isKelvynDevice || hasHeartRateService {
            print("Found potential ESP32 device: \(peripheral.name ?? "Unknown")")
            stopScanning()
            shouldReconnect = true
            self.connectToPeripheral(peripheral)
        }
    }
    
    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        print("Connected to Kelvyn Temperature Monitor")
        
        // Stop reconnection timer on successful connection
        stopReconnectionTimer()
        reconnectAttempts = 0
        
        DispatchQueue.main.async {
            self.isConnected = true
            self.connectionStatus = "Connected to Temperature Monitor"
        }
        
        peripheral.discoverServices([heartRateServiceUUID])
    }
    
    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        print("Disconnected from ESP32: \(error?.localizedDescription ?? "No error")")
        
        DispatchQueue.main.async {
            self.isConnected = false
            self.connectionStatus = "Disconnected - Reconnecting..."
            self.heartRateCharacteristic = nil
        }
        
        // Don't clear connectedPeripheral, we might need it for reconnection
        // connectedPeripheral = nil
        
        // ESP32 goes to sleep after disconnection, so start reconnection process immediately
        if shouldReconnect {
            print("Starting automatic reconnection...")
            
            // Try immediate reconnection first
            DispatchQueue.main.asyncAfter(deadline: .now() + 1.0) {
                if let lastPeripheral = self.lastKnownPeripheral {
                    print("Attempting immediate reconnection to last peripheral")
                    self.centralManager.connect(lastPeripheral, options: nil)
                }
            }
            
            // Start scanning and timer-based reconnection
            DispatchQueue.main.asyncAfter(deadline: .now() + 2.0) {
                self.connectionStatus = "ESP32 may be sleeping... Reconnecting..."
                self.startReconnectionTimer()
            }
        }
    }
    
    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        print("Failed to connect to ESP32: \(error?.localizedDescription ?? "Unknown error")")
        
        DispatchQueue.main.async {
            self.connectionStatus = "Connection failed. Retrying..."
        }
        
        // Don't give up, keep trying
        if shouldReconnect {
            DispatchQueue.main.asyncAfter(deadline: .now() + 2.0) {
                self.startReconnectionTimer()
            }
        }
    }
}

// MARK: - CBPeripheralDelegate

extension BluetoothManager: CBPeripheralDelegate {
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard let services = peripheral.services else { return }
        
        for service in services {
            if service.uuid == heartRateServiceUUID {
                peripheral.discoverCharacteristics([heartRateMeasurementUUID, sensorPositionUUID], for: service)
            }
        }
    }
    
    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        guard let characteristics = service.characteristics else { return }
        
        for characteristic in characteristics {
            if characteristic.uuid == heartRateMeasurementUUID {
                heartRateCharacteristic = characteristic
                
                // Subscribe to notifications for Heart Rate Monitor
                peripheral.setNotifyValue(true, for: characteristic)
                
                print("Subscribed to Heart Rate Monitor notifications")
            } else if characteristic.uuid == sensorPositionUUID {
                sensorPositionCharacteristic = characteristic
                
                // Read sensor position
                peripheral.readValue(for: characteristic)
                
                print("Read sensor position")
            }
        }
    }
    
    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard let data = characteristic.value else { return }
        
        if characteristic.uuid == heartRateMeasurementUUID {
            handleHeartRateData(data)
        }
    }
    
    func peripheral(_ peripheral: CBPeripheral, didUpdateNotificationStateFor characteristic: CBCharacteristic, error: Error?) {
        if let error = error {
            print("Notification state update failed: \(error.localizedDescription)")
        } else {
            print("Notification state updated for characteristic: \(characteristic.uuid)")
        }
    }
}
