import SwiftUI
import KeyboardShortcuts

extension KeyboardShortcuts.Name {
    static let openZoomify = Self("openZoomify", default: .init(.z, modifiers: [.command, .shift]))
}

func openZoomify() {
    guard let executablePath = Bundle.main.path(forResource: "zoomify", ofType: nil) else {
        print("Zoomify executable not found")
        return
    }
    
    let process = Process()
    process.executableURL = URL(fileURLWithPath: executablePath)
    
    do {
        try process.run()
        process.waitUntilExit()
    } catch {
        print("Error running executable: \(error)")
    }
}

@MainActor
@Observable
final class AppState {
    init() {
        KeyboardShortcuts.onKeyUp(for: .openZoomify, action: {
            openZoomify()
        })
    }
}

struct SettingsScreen: View {
    var body: some View {
        VStack(alignment: HorizontalAlignment.center) {
            Text("Change keybind").padding(EdgeInsets(top: 16, leading: 0, bottom: 6, trailing: 0))
            Divider().padding(EdgeInsets(top: 0, leading: 24, bottom: 0, trailing: 24))
            Form {
                KeyboardShortcuts.Recorder("Open Zoomify:", name: .openZoomify)
                Spacer()
            }
            .padding()
            .frame(minWidth: 280)
            .frame(minHeight: 200)
        }
    }
}

@main
struct zoomifydApp: App {
    @State private var appState = AppState()
    @Environment(\.openWindow) private var openWindow
    
    var body: some Scene {
        Window("Zoomify Settings", id: "zoomify-settings") {
            SettingsScreen()
        }.defaultLaunchBehavior(.suppressed)
        
        MenuBarExtra("Hello", systemImage: "magnifyingglass") {
            VStack {
                Button("Open Zoomify") {
                    openZoomify()
                }.keyboardShortcut("z", modifiers: [.command, .shift])
                Button("Zoomify Settings") {
                    openWindow(id: "zoomify-settings")
                }
                Divider()
                Button("Quit") {
                    NSApplication.shared.terminate(nil)
                }
            }
        }
    }
    
}

#Preview {
    SettingsScreen()
}
