import SwiftUI

@main
struct ICEWatchApp: App {
    @Environment(\.scenePhase) private var phase
    @State private var runtime = ICEWatchRuntime()
    var body: some Scene {
        WindowGroup {
            CompanionView(runtime: runtime)
                .onChange(of: phase) { _, value in runtime.setActive(value == .active) }
                .onAppear { runtime.setActive(phase == .active) }
        }
    }
}
struct CompanionView: View {
    @Environment(\.scenePhase) private var phase
    let runtime: ICEWatchRuntime
    @State private var motion = false
    @State private var dragging = false
    @State private var message = ""
    @State private var calibration = "Hold still, then calibrate"
    private let timer = Timer.publish(every: 0.15, on: .main, in: .common).autoconnect()
    var body: some View {
        ScrollView {
            VStack(spacing: 6) {
                Text("ICE Companion").font(.headline)
                Text(message).font(.caption2).frame(minHeight: 24)
                GeometryReader { geometry in
                    RoundedRectangle(cornerRadius: 12).fill(Color.blue.opacity(0.45))
                        .overlay(Text(motion ? "Hold · swing · release" : "Drag upward").font(.caption))
                        .gesture(DragGesture(minimumDistance: 0)
                            .onChanged { value in
                                if !dragging {
                                    runtime.pointer(0, x: Float(value.startLocation.x / geometry.size.width), y: Float(value.startLocation.y / geometry.size.height))
                                    dragging = true
                                }
                                runtime.pointer(1, x: Float(value.location.x / geometry.size.width), y: Float(value.location.y / geometry.size.height))
                            }
                            .onEnded { value in
                                runtime.pointer(2, x: Float(value.location.x / geometry.size.width), y: Float(value.location.y / geometry.size.height))
                                dragging = false
                            })
                }.frame(height: 72)
                Toggle("Motion", isOn: $motion).onChange(of: motion) { _, value in runtime.setMotion(value) }
                if motion {
                    Text(calibration).font(.caption2)
                    Button("Calibrate") { calibration = runtime.calibrate() }
                }
                Button("Record / Stop") { runtime.record() }
                Button("Replay take / demo") { runtime.replay() }
                Text("Local input test").font(.caption2).foregroundStyle(.secondary)
            }
        }.onReceive(timer) { _ in message = runtime.status() }
        .onChange(of: phase) { _, value in
            if value != .active { dragging = false; calibration = "Hold still, then calibrate" }
        }
        .onDisappear { runtime.pointer(3, x: 0, y: 0); dragging = false }
    }
}
