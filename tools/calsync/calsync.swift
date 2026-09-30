// Pushes the time and the next 7 days of Apple Calendar events to the unidex device over USB.
// Runs as a LaunchAgent (see install.sh). `--print` shows the payload instead of sending it.
import EventKit
import Foundation

let DAYS_AHEAD = 7, MAX_EVENTS = 64
let TIOCMBIC: UInt = 0x8004_746B  // _IOW('t', 107, int): clear modem lines
let store = EKEventStore()

func log(_ s: String) {
  let ts = ISO8601DateFormatter().string(from: Date())
  print("\(ts) \(s)")
  fflush(stdout)
}

func requestAccess() -> Bool {
  let done = DispatchSemaphore(value: 0)
  var granted = false
  store.requestFullAccessToEvents { ok, _ in granted = ok; done.signal() }
  done.wait()
  return granted
}

// The device font is ASCII only, and fields are comma-separated with fixed sizes.
func clean(_ s: String?, _ max: Int) -> String {
  let ascii = (s ?? "").applyingTransform(.toLatin, reverse: false)?
    .applyingTransform(.stripDiacritics, reverse: false) ?? ""
  let safe = String(ascii.unicodeScalars.filter { $0.isASCII && $0.value >= 32 && $0 != "," }.map(Character.init))
  return String(safe.trimmingCharacters(in: .whitespaces).prefix(max))
}

func eventLines() -> [String] {
  var cal = Calendar(identifier: .gregorian)
  cal.timeZone = TimeZone(identifier: "Europe/London")!  // the device shows London time
  let start = cal.startOfDay(for: Date())
  let end = cal.date(byAdding: .day, value: DAYS_AHEAD + 1, to: start)!
  let day = DateFormatter(), hm = DateFormatter()
  for f in [day, hm] { f.locale = Locale(identifier: "en_US_POSIX"); f.timeZone = cal.timeZone }
  day.dateFormat = "yyyy-MM-dd"
  hm.dateFormat = "HH:mm"

  var lines: [(Date, String)] = []
  let events = store.events(matching: store.predicateForEvents(withStart: start, end: end, calendars: nil))
  for e in events {
    let title = clean(e.title, 23), place = clean(e.location, 11)
    if e.isAllDay {
      // One line per day it covers, inside the window.
      var d = max(cal.startOfDay(for: e.startDate), start)
      while d < min(e.endDate, end) {
        lines.append((d, "\(day.string(from: d)),,,\(title),\(place)"))
        d = cal.date(byAdding: .day, value: 1, to: d)!
      }
    } else {
      // A timed event crossing midnight is cut at 23:59 on its first day.
      let sameDay = cal.isDate(e.startDate, inSameDayAs: e.endDate)
      let endText = sameDay ? hm.string(from: e.endDate) : "23:59"
      lines.append((e.startDate, "\(day.string(from: e.startDate)),\(hm.string(from: e.startDate)),\(endText),\(title),\(place)"))
    }
  }
  return lines.sorted { $0.0 < $1.0 }.prefix(MAX_EVENTS).map { $0.1 }
}

// zlib crc32, matching esp_rom_crc32_le on the device.
func crc32(_ data: [UInt8]) -> UInt32 {
  var crc: UInt32 = 0xFFFF_FFFF
  for b in data {
    crc ^= UInt32(b)
    for _ in 0..<8 { crc = (crc >> 1) ^ (crc & 1 == 1 ? 0xEDB8_8320 : 0) }
  }
  return ~crc
}

func payload(_ lines: [String]) -> String {
  let body = lines.map { $0 + "\n" }.joined()
  return "E \(lines.count) \(crc32(Array(body.utf8)))\n" + body
}

final class Port {
  let fd: Int32
  var buffer = ""

  init?(_ path: String) {
    fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK)
    if fd < 0 { return nil }
    // The ESP32-S3 resets when RTS is on while DTR is off. open() turns both on, so clear
    // RTS first, then DTR; clearing DTR first (or both at once) resets the board.
    var rts: Int32 = 0x004, dtr: Int32 = 0x002  // TIOCM_RTS, TIOCM_DTR
    _ = ioctl(fd, TIOCMBIC, &rts)
    _ = ioctl(fd, TIOCMBIC, &dtr)
    var t = termios()
    tcgetattr(fd, &t)
    cfmakeraw(&t)
    t.c_cflag |= tcflag_t(CLOCAL | CREAD)
    tcsetattr(fd, TCSANOW, &t)
  }

  deinit { close(fd) }

  func send(_ s: String) -> Bool {
    let bytes = Array(s.utf8)
    var sent = 0
    let deadline = Date().addingTimeInterval(5)
    while sent < bytes.count && Date() < deadline {
      let n = bytes[sent...].withUnsafeBytes { write(fd, $0.baseAddress, $0.count) }
      if n > 0 { sent += n } else { usleep(2000) }
    }
    return sent == bytes.count
  }

  // Waits for a line starting with `prefix`; other lines (debug logs) are skipped.
  func expect(_ prefix: String, timeout: TimeInterval) -> String? {
    let deadline = Date().addingTimeInterval(timeout)
    var chunk = [UInt8](repeating: 0, count: 256)
    while Date() < deadline {
      while let nl = buffer.firstIndex(of: "\n") {
        let line = buffer[..<nl].trimmingCharacters(in: .whitespacesAndNewlines)
        buffer.removeSubrange(...nl)
        if line.hasPrefix(prefix) { return line }
      }
      let n = read(fd, &chunk, chunk.count)
      // Drop \r: Swift treats "\r\n" as one Character, so a search for "\n" would never match.
      if n > 0 { buffer += String(decoding: chunk[0..<n].filter { $0 != 13 }, as: UTF8.self) } else { usleep(10000) }
    }
    return nil
  }
}

func sync(_ path: String, calendar: Bool) -> Bool {
  guard let port = Port(path) else { log("\(path): can't open"); return false }
  // Keep asking for up to 6 s: after a cold boot the device is busy with the splash for ~3.5 s
  // (it queues the pings and answers once free), and old log output may still be draining.
  let hello = (0..<6).contains { _ in port.send("?\n") && port.expect("unidex 1", timeout: 1) != nil }
  guard hello else {
    log("\(path): not a unidex (no reply)")
    return false
  }
  _ = port.send("T \(Int(Date().timeIntervalSince1970.rounded()))\n")
  let t = port.expect("OK T", timeout: 2) != nil
  guard calendar else {
    log("\(path): time \(t ? "set" : "no reply"); no Calendar access, events not sent")
    return true
  }
  let lines = eventLines()
  _ = port.send(payload(lines))
  let reply = port.expect("OK E", timeout: 5) ?? port.expect("ERR", timeout: 0) ?? "no reply"
  log("\(path): time \(t ? "set" : "no reply"), \(lines.count) events: \(reply)")
  return true
}

func usbPorts() -> Set<String> {
  let names = (try? FileManager.default.contentsOfDirectory(atPath: "/dev")) ?? []
  return Set(names.filter { $0.hasPrefix("cu.usbmodem") }.map { "/dev/" + $0 })
}

let calendar = requestAccess()
if CommandLine.arguments.contains("--print") {
  if !calendar { print("no Calendar access"); exit(1) }
  print(payload(eventLines()), terminator: "")
  exit(0)
}
log("started; Calendar access \(calendar ? "granted" : "denied")")

// A port is synced once per appearance: every wake while plugged in re-syncs, and the
// device skips unchanged events. Ports that aren't a unidex are ignored until they go away.
// A port must be present for two polls in a row, to keep out of esptool's way while flashing.
var done = Set<String>(), previous = Set<String>()
while true {
  let ports = usbPorts()
  done.formIntersection(ports)
  for p in ports.intersection(previous).subtracting(done) {
    _ = sync(p, calendar: calendar)
    done.insert(p)
  }
  previous = ports
  sleep(1)
}
