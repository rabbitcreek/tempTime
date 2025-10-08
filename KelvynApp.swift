import SwiftUI

@main
struct KelvynApp: App {
    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(BluetoothManager.shared)
        }
    }
}