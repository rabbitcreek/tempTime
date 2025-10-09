import Foundation
import Combine

class DataManager: ObservableObject {
    static let shared = DataManager()
    
    @Published private(set) var temperatureReadings: [TemperatureDataPoint] = []
    
    private init() {
        // Simple in-memory storage for now
        // Can be upgraded to Core Data later if persistence is needed
    }
    
    func saveTemperature(_ temperature: Double, humidity: Double = 0.0) {
        let reading = TemperatureDataPoint(temperature: temperature, humidity: humidity, timestamp: Date())
        temperatureReadings.append(reading)
        
        // Keep only last 1000 readings to prevent memory issues
        if temperatureReadings.count > 1000 {
            temperatureReadings.removeFirst(temperatureReadings.count - 1000)
        }
        
        // Trigger UI updates
        objectWillChange.send()
    }
    
    func fetchRecentReadings(limit: Int = 100) -> [TemperatureDataPoint] {
        return Array(temperatureReadings.suffix(limit))
    }
    
    func fetchReadings(from startDate: Date, to endDate: Date) -> [TemperatureDataPoint] {
        return temperatureReadings.filter { reading in
            reading.timestamp >= startDate && reading.timestamp <= endDate
        }.sorted { $0.timestamp < $1.timestamp }
    }
    
    func fetchTodayReadings() -> [TemperatureDataPoint] {
        let calendar = Calendar.current
        let startOfDay = calendar.startOfDay(for: Date())
        let endOfDay = calendar.date(byAdding: .day, value: 1, to: startOfDay)!
        
        return fetchReadings(from: startOfDay, to: endOfDay)
    }
    
    func fetchWeekReadings() -> [TemperatureDataPoint] {
        let calendar = Calendar.current
        let startOfWeek = calendar.dateInterval(of: .weekOfYear, for: Date())?.start ?? Date()
        let endOfWeek = calendar.dateInterval(of: .weekOfYear, for: Date())?.end ?? Date()
        
        return fetchReadings(from: startOfWeek, to: endOfWeek)
    }
    
    func fetchMonthReadings() -> [TemperatureDataPoint] {
        let calendar = Calendar.current
        let startOfMonth = calendar.dateInterval(of: .month, for: Date())?.start ?? Date()
        let endOfMonth = calendar.dateInterval(of: .month, for: Date())?.end ?? Date()
        
        return fetchReadings(from: startOfMonth, to: endOfMonth)
    }
    
    func deleteOldReadings(olderThan days: Int = 30) {
        let cutoffDate = Calendar.current.date(byAdding: .day, value: -days, to: Date()) ?? Date()
        temperatureReadings.removeAll { $0.timestamp < cutoffDate }
        objectWillChange.send()
    }
    
    func getTemperatureStats(for readings: [TemperatureDataPoint]) -> TemperatureStats {
        return TemperatureStats(readings: readings)
    }
    
    var latestReading: TemperatureDataPoint? {
        return temperatureReadings.last
    }
    
    var hasReadings: Bool {
        return !temperatureReadings.isEmpty
    }
}