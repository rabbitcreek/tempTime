import SwiftUI

struct SpeedometerView: View {
    @EnvironmentObject var bluetoothManager: BluetoothManager
    @StateObject private var dataManager = DataManager.shared
    @State private var useFahrenheit = false
    @State private var animationOffset: CGFloat = 0
    
    private var currentTemperature: Double {
        if useFahrenheit {
            return TemperatureScale.toFahrenheit(bluetoothManager.currentTemperature)
        }
        return bluetoothManager.currentTemperature
    }
    
    private var temperatureRange: (min: Double, max: Double) {
        return TemperatureScale.getMinMax(for: useFahrenheit)
    }
    
    private var needleAngle: Double {
        let range = temperatureRange.max - temperatureRange.min
        let normalizedTemp = (currentTemperature - temperatureRange.min) / range
        return (normalizedTemp * 180.0) - 90.0 // -90 to +90 degrees
    }
    
    private var connectionStatusColor: Color {
        switch bluetoothManager.connectionStatus {
        case "Connected to ESP32":
            return .green
        case "Scanning for ESP32...", "Connecting to ESP32...":
            return .orange
        default:
            return .red
        }
    }
    
    var body: some View {
        ZStack {
            // Background gradient
            LinearGradient(
                gradient: Gradient(colors: [Color.black, Color.gray.opacity(0.3)]),
                startPoint: .topLeading,
                endPoint: .bottomTrailing
            )
            .ignoresSafeArea()
            
            VStack(spacing: 20) {
                // Header
                HStack {
                    VStack(alignment: .leading, spacing: 4) {
                        Text("Kelvyn")
                            .font(.largeTitle)
                            .fontWeight(.bold)
                            .foregroundColor(.white)
                        
                        Text("Temperature Monitor")
                            .font(.caption)
                            .foregroundColor(.white.opacity(0.7))
                    }
                    
                    Spacer()
                    
                    // Unit toggle
                    Button(action: {
                        withAnimation(.easeInOut(duration: 0.3)) {
                            useFahrenheit.toggle()
                        }
                    }) {
                        Text(useFahrenheit ? "°F" : "°C")
                            .font(.title2)
                            .fontWeight(.semibold)
                            .foregroundColor(.white)
                            .padding(.horizontal, 16)
                            .padding(.vertical, 8)
                            .background(Color.white.opacity(0.2))
                            .cornerRadius(20)
                    }
                }
                .padding(.horizontal, 20)
                .padding(.top, 10)
                
                Spacer()
                
                // Speedometer
                ZStack {
                    // Outer ring
                    Circle()
                        .stroke(
                            LinearGradient(
                                gradient: Gradient(colors: [.blue, .green, .yellow, .orange, .red]),
                                startPoint: .topLeading,
                                endPoint: .bottomTrailing
                            ),
                            lineWidth: 8
                        )
                        .frame(width: 280, height: 280)
                        .rotationEffect(.degrees(90))
                    
                    // Inner background
                    Circle()
                        .fill(Color.black.opacity(0.3))
                        .frame(width: 260, height: 260)
                    
                    // Tick marks
                    ForEach(0..<11) { index in
                        let angle = Double(index) * 18.0 - 90.0 // -90 to +90 degrees
                        let isMajorTick = index % 2 == 0
                        
                        Rectangle()
                            .fill(Color.white)
                            .frame(width: isMajorTick ? 3 : 1, height: isMajorTick ? 20 : 12)
                            .offset(y: -130)
                            .rotationEffect(.degrees(angle))
                    }
                    
                    // Temperature labels
                    ForEach(0..<6) { index in
                        let angle = Double(index) * 36.0 - 90.0
                        let temp = temperatureRange.min + (temperatureRange.max - temperatureRange.min) * Double(index) / 5.0
                        let label = String(format: "%.0f", temp)
                        
                        Text(label)
                            .font(.caption)
                            .fontWeight(.medium)
                            .foregroundColor(.white)
                            .offset(
                                x: sin(angle * .pi / 180) * 110,
                                y: -cos(angle * .pi / 180) * 110
                            )
                    }
                    
                    // Needle
                    Rectangle()
                        .fill(
                            LinearGradient(
                                gradient: Gradient(colors: [.white, .red]),
                                startPoint: .leading,
                                endPoint: .trailing
                            )
                        )
                        .frame(width: 4, height: 100)
                        .offset(y: -50)
                        .rotationEffect(.degrees(needleAngle + animationOffset))
                        .animation(.easeInOut(duration: 1.0), value: needleAngle)
                    
                    // Center dot
                    Circle()
                        .fill(Color.white)
                        .frame(width: 20, height: 20)
                        .overlay(
                            Circle()
                                .stroke(Color.black, lineWidth: 2)
                        )
                    
                    // Temperature display
                    VStack(spacing: 4) {
                        Text(String(format: "%.1f", currentTemperature))
                            .font(.system(size: 48, weight: .bold, design: .rounded))
                            .foregroundColor(.white)
                            .contentTransition(.numericText())
                        
                        Text(useFahrenheit ? "°F" : "°C")
                            .font(.title2)
                            .foregroundColor(.white.opacity(0.8))
                        
                        if bluetoothManager.currentHumidity > 0 {
                            Text("Humidity: \(String(format: "%.1f", bluetoothManager.currentHumidity))%")
                                .font(.caption)
                                .foregroundColor(.white.opacity(0.7))
                        }
                    }
                    .offset(y: 80)
                }
                
                Spacer()
                
                // Connection status
                VStack(spacing: 8) {
                    HStack(spacing: 8) {
                        Circle()
                            .fill(connectionStatusColor)
                            .frame(width: 12, height: 12)
                            .scaleEffect(bluetoothManager.isConnected ? 1.2 : 1.0)
                            .animation(.easeInOut(duration: 0.5).repeatForever(), value: bluetoothManager.isConnected)
                        
                        Text(bluetoothManager.connectionStatus)
                            .font(.subheadline)
                            .foregroundColor(.white)
                    }
                    
                    if let lastUpdate = bluetoothManager.lastUpdateTime {
                        Text("Last update: \(lastUpdate, style: .time)")
                            .font(.caption)
                            .foregroundColor(.white.opacity(0.6))
                    }
                    
                    // Manual connect button
                    if !bluetoothManager.isConnected {
                        Button(action: {
                            bluetoothManager.startScanning()
                        }) {
                            Text("Connect to ESP32")
                                .font(.subheadline)
                                .fontWeight(.medium)
                                .foregroundColor(.white)
                                .padding(.horizontal, 20)
                                .padding(.vertical, 10)
                                .background(Color.blue)
                                .cornerRadius(20)
                        }
                        .disabled(bluetoothManager.isScanning)
                    }
                }
                .padding(.bottom, 20)
            }
        }
        .onAppear {
            // Start connection when view appears
            if !bluetoothManager.isConnected {
                bluetoothManager.startScanning()
            }
        }
        .onReceive(NotificationCenter.default.publisher(for: UIApplication.didBecomeActiveNotification)) { _ in
            // Reconnect when app becomes active
            if !bluetoothManager.isConnected {
                bluetoothManager.startScanning()
            }
        }
    }
}

#Preview {
    SpeedometerView()
        .environmentObject(BluetoothManager.shared)
}