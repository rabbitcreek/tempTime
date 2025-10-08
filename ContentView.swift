import SwiftUI

struct ContentView: View {
    @EnvironmentObject var bluetoothManager: BluetoothManager
    @State private var selectedTab = 0
    
    var body: some View {
        TabView(selection: $selectedTab) {
            SpeedometerView()
                .tabItem {
                    Image(systemName: "speedometer")
                    Text("Temperature")
                }
                .tag(0)
            
            GraphView()
                .tabItem {
                    Image(systemName: "chart.line.uptrend.xyaxis")
                    Text("History")
                }
                .tag(1)
        }
        .accentColor(.white)
        .onAppear {
            // Start scanning when app launches
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
    ContentView()
        .environmentObject(BluetoothManager.shared)
}