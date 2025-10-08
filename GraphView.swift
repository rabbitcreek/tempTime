import SwiftUI
import Charts

// MARK: - Time Range for Graph View
enum TimeRange: String, CaseIterable {
    case today = "Today"
    case week = "Week"
    case month = "Month"
    case all = "All"
}

// MARK: - Chart Data Point
struct ChartDataPoint: Identifiable {
    let id = UUID()
    let timestamp: Date
    let temperature: Double
    let humidity: Double
}

struct GraphView: View {
    @EnvironmentObject var bluetoothManager: BluetoothManager
    @StateObject private var dataManager = DataManager.shared
    @State private var selectedTimeRange: TimeRange = .today
    @State private var useFahrenheit = false
    @State private var showStatistics = false
    
    private var readings: [TemperatureDataPoint] {
        switch selectedTimeRange {
        case .today:
            return dataManager.fetchTodayReadings()
        case .week:
            return dataManager.fetchWeekReadings()
        case .month:
            return dataManager.fetchMonthReadings()
        case .all:
            return dataManager.fetchRecentReadings(limit: 1000)
        }
    }
    
    private var chartData: [ChartDataPoint] {
        readings.map { reading in
            ChartDataPoint(
                timestamp: reading.timestamp,
                temperature: useFahrenheit ? TemperatureScale.toFahrenheit(reading.temperature) : reading.temperature,
                humidity: reading.humidity
            )
        }
    }
    
    private var temperatureStats: TemperatureStats {
        TemperatureStats(readings: readings)
    }
    
    private var temperatureUnit: String {
        useFahrenheit ? "°F" : "°C"
    }
    
    var body: some View {
        NavigationView {
            ZStack {
                // Background gradient
                LinearGradient(
                    gradient: Gradient(colors: [Color.black, Color.gray.opacity(0.3)]),
                    startPoint: .topLeading,
                    endPoint: .bottomTrailing
                )
                .ignoresSafeArea()
                
                ScrollView {
                    VStack(spacing: 20) {
                        // Header
                        HStack {
                            Text("Temperature History")
                                .font(.largeTitle)
                                .fontWeight(.bold)
                                .foregroundColor(.white)
                            
                            Spacer()
                            
                            // Unit toggle
                            Button(action: {
                                withAnimation(.easeInOut(duration: 0.3)) {
                                    useFahrenheit.toggle()
                                }
                            }) {
                                Text(useFahrenheit ? "°F" : "°C")
                                    .font(.title3)
                                    .fontWeight(.semibold)
                                    .foregroundColor(.white)
                                    .padding(.horizontal, 12)
                                    .padding(.vertical, 6)
                                    .background(Color.white.opacity(0.2))
                                    .cornerRadius(15)
                            }
                        }
                        .padding(.horizontal, 20)
                        .padding(.top, 10)
                        
                        // Time range selector
                        Picker("Time Range", selection: $selectedTimeRange) {
                            ForEach(TimeRange.allCases, id: \.self) { range in
                                Text(range.rawValue).tag(range)
                            }
                        }
                        .pickerStyle(SegmentedPickerStyle())
                        .padding(.horizontal, 20)
                        
                        // Statistics cards
                        if readings.count > 0 {
                            statisticsView
                        }
                        
                        // Chart
                        if chartData.count > 0 {
                            chartView
                        } else {
                            emptyStateView
                        }
                        
                        // Data summary
                        if readings.count > 0 {
                            dataSummaryView
                        }
                    }
                    .padding(.bottom, 20)
                }
            }
            .navigationBarHidden(true)
        }
    }
    
    private var statisticsView: some View {
        VStack(spacing: 12) {
            HStack(spacing: 12) {
                StatCard(
                    title: "Current",
                    value: String(format: "%.1f", useFahrenheit ? TemperatureScale.toFahrenheit(temperatureStats.current) : temperatureStats.current),
                    unit: temperatureUnit,
                    color: .blue
                )
                
                StatCard(
                    title: "Average",
                    value: String(format: "%.1f", useFahrenheit ? TemperatureScale.toFahrenheit(temperatureStats.average) : temperatureStats.average),
                    unit: temperatureUnit,
                    color: .green
                )
            }
            
            HStack(spacing: 12) {
                StatCard(
                    title: "Min",
                    value: String(format: "%.1f", useFahrenheit ? TemperatureScale.toFahrenheit(temperatureStats.min) : temperatureStats.min),
                    unit: temperatureUnit,
                    color: .cyan
                )
                
                StatCard(
                    title: "Max",
                    value: String(format: "%.1f", useFahrenheit ? TemperatureScale.toFahrenheit(temperatureStats.max) : temperatureStats.max),
                    unit: temperatureUnit,
                    color: .red
                )
            }
        }
        .padding(.horizontal, 20)
    }
    
    private var chartView: some View {
        VStack(alignment: .leading, spacing: 16) {
            Text("Temperature Trend")
                .font(.headline)
                .foregroundColor(.white)
                .padding(.horizontal, 20)
            
            VStack {
                Chart(chartData) { dataPoint in
                    LineMark(
                        x: .value("Time", dataPoint.timestamp),
                        y: .value("Temperature", dataPoint.temperature)
                    )
                    .foregroundStyle(
                        LinearGradient(
                            gradient: Gradient(colors: [.blue, .cyan, .green, .yellow, .orange, .red]),
                            startPoint: .leading,
                            endPoint: .trailing
                        )
                    )
                    .lineStyle(StrokeStyle(lineWidth: 3, lineCap: .round))
                    
                    AreaMark(
                        x: .value("Time", dataPoint.timestamp),
                        y: .value("Temperature", dataPoint.temperature)
                    )
                    .foregroundStyle(
                        LinearGradient(
                            gradient: Gradient(colors: [.blue.opacity(0.3), .cyan.opacity(0.1)]),
                            startPoint: .top,
                            endPoint: .bottom
                        )
                    )
                }
                .chartXAxis {
                    AxisMarks(values: .stride(by: timeStride)) { value in
                        if let date = value.as(Date.self) {
                            AxisGridLine(stroke: StrokeStyle(lineWidth: 0.5))
                                .foregroundStyle(.white.opacity(0.3))
                            
                            AxisValueLabel {
                                Text(date, style: .time)
                                    .foregroundStyle(.white)
                                    .font(.caption)
                            }
                        }
                    }
                }
                .chartYAxis {
                    AxisMarks(position: .leading) { value in
                        AxisGridLine(stroke: StrokeStyle(lineWidth: 0.5))
                            .foregroundStyle(.white.opacity(0.3))
                        
                        AxisValueLabel {
                            if let temp = value.as(Double.self) {
                                Text("\(Int(temp))\(temperatureUnit)")
                                    .foregroundStyle(.white)
                                    .font(.caption)
                            }
                        }
                    }
                }
                .frame(height: 300)
                .padding()
                .background(
                    RoundedRectangle(cornerRadius: 16)
                        .fill(Color.white.opacity(0.1))
                        .overlay(
                            RoundedRectangle(cornerRadius: 16)
                                .stroke(Color.white.opacity(0.2), lineWidth: 1)
                        )
                )
                .padding(.horizontal, 20)
            }
        }
    }
    
    private var emptyStateView: some View {
        VStack(spacing: 16) {
            Image(systemName: "thermometer")
                .font(.system(size: 60))
                .foregroundColor(.white.opacity(0.5))
            
            Text("No Data Available")
                .font(.title2)
                .fontWeight(.medium)
                .foregroundColor(.white)
            
            Text("Connect to your ESP32 to start collecting temperature data")
                .font(.subheadline)
                .foregroundColor(.white.opacity(0.7))
                .multilineTextAlignment(.center)
                .padding(.horizontal, 40)
        }
        .frame(height: 300)
        .frame(maxWidth: .infinity)
        .background(
            RoundedRectangle(cornerRadius: 16)
                .fill(Color.white.opacity(0.1))
        )
        .padding(.horizontal, 20)
    }
    
    private var dataSummaryView: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("Data Summary")
                .font(.headline)
                .foregroundColor(.white)
            
            HStack {
                VStack(alignment: .leading, spacing: 4) {
                    Text("Total Readings")
                        .font(.caption)
                        .foregroundColor(.white.opacity(0.7))
                    Text("\(readings.count)")
                        .font(.title2)
                        .fontWeight(.semibold)
                        .foregroundColor(.white)
                }
                
                Spacer()
                
                VStack(alignment: .trailing, spacing: 4) {
                    Text("Time Range")
                        .font(.caption)
                        .foregroundColor(.white.opacity(0.7))
                    Text(selectedTimeRange.rawValue)
                        .font(.title2)
                        .fontWeight(.semibold)
                        .foregroundColor(.white)
                }
            }
            
            // Trend indicator
            HStack {
                Image(systemName: trendIcon)
                    .foregroundColor(trendColor)
                
                Text(trendText)
                    .font(.subheadline)
                    .foregroundColor(.white)
                
                Spacer()
            }
        }
        .padding(16)
        .background(
            RoundedRectangle(cornerRadius: 12)
                .fill(Color.white.opacity(0.1))
        )
        .padding(.horizontal, 20)
    }
    
    private var timeStride: Calendar.Component {
        switch selectedTimeRange {
        case .today:
            return .hour
        case .week:
            return .day
        case .month:
            return .day
        case .all:
            return .day
        }
    }
    
    private var trendIcon: String {
        switch temperatureStats.trend {
        case .rising:
            return "arrow.up.right"
        case .falling:
            return "arrow.down.right"
        case .stable:
            return "arrow.right"
        }
    }
    
    private var trendColor: Color {
        switch temperatureStats.trend {
        case .rising:
            return .red
        case .falling:
            return .blue
        case .stable:
            return .green
        }
    }
    
    private var trendText: String {
        switch temperatureStats.trend {
        case .rising:
            return "Temperature Rising"
        case .falling:
            return "Temperature Falling"
        case .stable:
            return "Temperature Stable"
        }
    }
}

struct StatCard: View {
    let title: String
    let value: String
    let unit: String
    let color: Color
    
    var body: some View {
        VStack(spacing: 8) {
            Text(title)
                .font(.caption)
                .foregroundColor(.white.opacity(0.7))
            
            HStack(alignment: .lastTextBaseline, spacing: 2) {
                Text(value)
                    .font(.title2)
                    .fontWeight(.bold)
                    .foregroundColor(.white)
                
                Text(unit)
                    .font(.caption)
                    .foregroundColor(.white.opacity(0.8))
            }
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, 16)
        .background(
            RoundedRectangle(cornerRadius: 12)
                .fill(color.opacity(0.2))
                .overlay(
                    RoundedRectangle(cornerRadius: 12)
                        .stroke(color.opacity(0.3), lineWidth: 1)
                )
        )
    }
}

#Preview {
    GraphView()
        .environmentObject(BluetoothManager.shared)
}