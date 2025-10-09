import Foundation

// MARK: - Temperature Data Point
struct TemperatureDataPoint: Identifiable, Codable {
    let id: UUID
    let temperature: Double
    let humidity: Double
    let timestamp: Date
    
    init(temperature: Double, humidity: Double = 0.0, timestamp: Date = Date()) {
        self.id = UUID()
        self.temperature = temperature
        self.humidity = humidity
        self.timestamp = timestamp
    }
}

// MARK: - Temperature Statistics
struct TemperatureStats {
    let current: Double
    let min: Double
    let max: Double
    let average: Double
    let trend: TemperatureTrend
    
    enum TemperatureTrend {
        case rising, falling, stable
    }
    
    init(readings: [TemperatureDataPoint]) {
        guard !readings.isEmpty else {
            self.current = 0.0
            self.min = 0.0
            self.max = 0.0
            self.average = 0.0
            self.trend = .stable
            return
        }
        
        let sortedReadings = readings.sorted { $0.timestamp < $1.timestamp }
        let temperatures = readings.map { $0.temperature }
        
        self.current = sortedReadings.last?.temperature ?? 0.0
        self.min = temperatures.min() ?? 0.0
        self.max = temperatures.max() ?? 0.0
        self.average = temperatures.reduce(0, +) / Double(temperatures.count)
        
        // Determine trend from last 5 readings
        let recentReadings = Array(sortedReadings.suffix(5))
        if recentReadings.count >= 2 {
            let first = recentReadings.first!.temperature
            let last = recentReadings.last!.temperature
            let difference = last - first
            
            if abs(difference) < 0.5 {
                self.trend = .stable
            } else if difference > 0 {
                self.trend = .rising
            } else {
                self.trend = .falling
            }
        } else {
            self.trend = .stable
        }
    }
}

// MARK: - Temperature Scale
struct TemperatureScale {
    static let celsiusMin: Double = -10.0
    static let celsiusMax: Double = 50.0
    static let fahrenheitMin: Double = 14.0
    static let fahrenheitMax: Double = 122.0
    
    static func toFahrenheit(_ celsius: Double) -> Double {
        return celsius * 9.0 / 5.0 + 32.0
    }
    
    static func toCelsius(_ fahrenheit: Double) -> Double {
        return (fahrenheit - 32.0) * 5.0 / 9.0
    }
    
    static func getMinMax(for fahrenheit: Bool) -> (min: Double, max: Double) {
        if fahrenheit {
            return (fahrenheitMin, fahrenheitMax)
        } else {
            return (celsiusMin, celsiusMax)
        }
    }
}
