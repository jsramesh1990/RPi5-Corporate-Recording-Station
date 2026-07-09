# 📁 Complete Documentation Files - API Directory

Here are all the API documentation files for your Raspberry Pi 5 Corporate Recording Station project.

---

## 📂 `docs/api/rest-api.md`

```markdown
# REST API Documentation
## RPi5 Corporate Recording Station

## 📋 Overview

The Recording Station provides a comprehensive REST API for controlling and monitoring the system. All endpoints return JSON responses.

**Base URL**: `http://<raspberry-pi-ip>:8080/api/v1`

**Authentication**: Bearer token (if enabled)
```
Authorization: Bearer <your-token>
```

## 🎯 Endpoints

### System Status

#### GET `/status`
Get overall system status.

**Response**:
```json
{
    "status": "online",
    "uptime": 86400,
    "uptime_formatted": "1d 0h 0m",
    "cpu_usage": 25.5,
    "cpu_temp": 45.2,
    "memory": {
        "total": 4096,
        "used": 1536,
        "free": 2560,
        "available": 2048,
        "percentage": 37.5
    },
    "storage": {
        "total": 128000,
        "used": 76800,
        "free": 51200,
        "percentage": 60.0
    },
    "recording": {
        "active": false,
        "duration": 0,
        "file": ""
    },
    "cloud": {
        "connected": true,
        "provider": "nextcloud",
        "syncing": false
    },
    "timestamp": "2026-07-08T10:30:45Z"
}
```

#### GET `/health`
Simple health check endpoint.

**Response**:
```json
{
    "status": "healthy",
    "version": "2.0.0",
    "timestamp": "2026-07-08T10:30:45Z"
}
```

### Recording Control

#### POST `/recording/start`
Start a new recording.

**Request** (optional body):
```json
{
    "duration": 3600,  // Optional: max duration in seconds
    "filename": "meeting_20260708.mp4",  // Optional
    "video": {
        "width": 1920,
        "height": 1080,
        "framerate": 30,
        "bitrate": 4000000
    },
    "audio": {
        "sample_rate": 48000,
        "channels": 2,
        "bitrate": 128000
    }
}
```

**Response**:
```json
{
    "success": true,
    "recording_id": "rec_20260708_103045",
    "filename": "meeting_20260708.mp4",
    "start_time": "2026-07-08T10:30:45Z",
    "status": "recording"
}
```

#### POST `/recording/stop`
Stop the current recording.

**Response**:
```json
{
    "success": true,
    "recording_id": "rec_20260708_103045",
    "filename": "meeting_20260708.mp4",
    "duration": 3600,
    "size": 1500000000,
    "stop_time": "2026-07-08T11:30:45Z",
    "status": "completed"
}
```

#### GET `/recording/status`
Get current recording status.

**Response**:
```json
{
    "active": true,
    "recording_id": "rec_20260708_103045",
    "filename": "meeting_20260708.mp4",
    "start_time": "2026-07-08T10:30:45Z",
    "duration": 1800,
    "size": 750000000
}
```

#### GET `/recording/list`
List all recordings.

**Query Parameters**:
- `limit`: Number of recordings (default: 100)
- `offset`: Pagination offset (default: 0)
- `sort`: Sort by (date, size, name) (default: date)
- `order`: asc or desc (default: desc)

**Response**:
```json
{
    "total": 247,
    "recordings": [
        {
            "id": "rec_20260708_103045",
            "filename": "meeting_20260708.mp4",
            "path": "/var/lib/recording-station/recordings/meeting_20260708.mp4",
            "size": 1500000000,
            "duration": 3600,
            "created": "2026-07-08T10:30:45Z",
            "modified": "2026-07-08T11:30:45Z",
            "format": "mp4",
            "video": {
                "width": 1920,
                "height": 1080,
                "codec": "h264",
                "framerate": 30
            },
            "audio": {
                "codec": "aac",
                "sample_rate": 48000,
                "channels": 2
            }
        }
    ]
}
```

#### DELETE `/recording/{id}`
Delete a recording.

**Response**:
```json
{
    "success": true,
    "id": "rec_20260708_103045",
    "deleted": true
}
```

### Storage Management

#### GET `/storage/status`
Get storage status.

**Response**:
```json
{
    "total": 128000,
    "used": 76800,
    "free": 51200,
    "percentage": 60.0,
    "files": 1247,
    "directories": 56,
    "last_scan": "2026-07-08T10:30:45Z",
    "scan_duration": 2.3,
    "warnings": [
        {
            "level": "warning",
            "message": "Storage usage above 80%"
        }
    ]
}
```

#### GET `/storage/files`
List files in storage.

**Query Parameters**:
- `path`: Directory path (default: /)
- `recursive`: Include subdirectories (default: false)
- `type`: Filter by file type (video, audio, image, other)
- `pattern`: Filter by filename pattern

**Response**:
```json
{
    "path": "/var/lib/recording-station/recordings",
    "files": [
        {
            "name": "meeting_20260708.mp4",
            "path": "/var/lib/recording-station/recordings/meeting_20260708.mp4",
            "type": "video",
            "size": 1500000000,
            "created": "2026-07-08T10:30:45Z",
            "modified": "2026-07-08T11:30:45Z",
            "permissions": "rw-r--r--",
            "owner": "root",
            "group": "root"
        }
    ],
    "total": 247,
    "size": 76800000000
}
```

### Cloud Management

#### GET `/cloud/status`
Get cloud connection status.

**Response**:
```json
{
    "connected": true,
    "provider": "nextcloud",
    "url": "https://nextcloud.example.com",
    "username": "recording_station",
    "syncing": false,
    "last_sync": "2026-07-08T10:30:45Z",
    "queue_size": 0
}
```

#### POST `/cloud/upload`
Upload a file to cloud.

**Request**:
```json
{
    "file": "/var/lib/recording-station/recordings/meeting_20260708.mp4",
    "path": "/Recordings/2026/07/",  // Optional
    "provider": "nextcloud"  // Optional
}
```

**Response**:
```json
{
    "success": true,
    "file": "/var/lib/recording-station/recordings/meeting_20260708.mp4",
    "cloud_path": "/Recordings/2026/07/meeting_20260708.mp4",
    "size": 1500000000,
    "uploaded": "2026-07-08T10:35:45Z",
    "provider": "nextcloud"
}
```

#### POST `/cloud/sync`
Sync all files to cloud.

**Response**:
```json
{
    "success": true,
    "synced": 24,
    "failed": 0,
    "total": 24,
    "timestamp": "2026-07-08T10:30:45Z"
}
```

### Hardware Control

#### GET `/hardware/status`
Get hardware status.

**Response**:
```json
{
    "gpio": {
        "status": "ok",
        "pins": {
            "17": { "function": "output", "value": 1 },
            "27": { "function": "output", "value": 0 },
            "22": { "function": "input", "value": 1 }
        }
    },
    "uart": {
        "status": "ok",
        "device": "/dev/ttyAMA0",
        "baud": 115200
    },
    "camera": {
        "status": "ok",
        "device": "/dev/video0",
        "resolution": "1920x1080",
        "framerate": 30
    },
    "audio": {
        "status": "ok",
        "device": "hw:0,0",
        "sample_rate": 48000,
        "channels": 2
    }
}
```

#### POST `/hardware/led`
Control LED status.

**Request**:
```json
{
    "pin": 17,
    "state": 1  // 0=off, 1=on, 2=toggle
}
```

**Response**:
```json
{
    "success": true,
    "pin": 17,
    "state": 1
}
```

### System Control

#### POST `/system/reboot`
Reboot the system.

**Response**:
```json
{
    "success": true,
    "message": "System rebooting in 5 seconds"
}
```

#### POST `/system/shutdown`
Shutdown the system.

**Response**:
```json
{
    "success": true,
    "message": "System shutting down in 5 seconds"
}
```

## 📊 WebSocket Events

The WebSocket server provides real-time updates on port 8081.

### Events

#### `system.status`
System status updates.

```json
{
    "event": "system.status",
    "data": {
        "cpu_usage": 25.5,
        "memory_usage": 37.5,
        "temperature": 45.2
    }
}
```

#### `recording.started`
Recording started event.

```json
{
    "event": "recording.started",
    "data": {
        "id": "rec_20260708_103045",
        "filename": "meeting_20260708.mp4"
    }
}
```

#### `recording.stopped`
Recording stopped event.

```json
{
    "event": "recording.stopped",
    "data": {
        "id": "rec_20260708_103045",
        "filename": "meeting_20260708.mp4",
        "duration": 3600
    }
}
```

#### `storage.changed`
Storage changed event.

```json
{
    "event": "storage.changed",
    "data": {
        "action": "file_added",
        "file": "meeting_20260708.mp4",
        "size": 1500000000
    }
}
```

#### `cloud.synced`
Cloud sync complete event.

```json
{
    "event": "cloud.synced",
    "data": {
        "synced": 24,
        "failed": 0
    }
}
```

## 🔧 Error Codes

| Code | Description |
|------|-------------|
| 200 | Success |
| 400 | Bad Request |
| 401 | Unauthorized |
| 403 | Forbidden |
| 404 | Not Found |
| 409 | Conflict |
| 500 | Internal Server Error |
| 503 | Service Unavailable |

## 📝 Example Usage

### cURL Examples

```bash
# Check status
curl -X GET http://localhost:8080/api/v1/status

# Start recording
curl -X POST http://localhost:8080/api/v1/recording/start \
  -H "Content-Type: application/json" \
  -d '{"duration": 3600}'

# Stop recording
curl -X POST http://localhost:8080/api/v1/recording/stop

# List recordings
curl -X GET http://localhost:8080/api/v1/recording/list?limit=10

# Upload to cloud
curl -X POST http://localhost:8080/api/v1/cloud/upload \
  -H "Content-Type: application/json" \
  -d '{"file": "/var/lib/recording-station/recordings/meeting_20260708.mp4"}'
```

### Python Example

```python
import requests
import json

BASE_URL = "http://raspberry-pi-ip:8080/api/v1"

# Check status
response = requests.get(f"{BASE_URL}/status")
print(json.dumps(response.json(), indent=2))

# Start recording
data = {"duration": 3600, "filename": "meeting.mp4"}
response = requests.post(f"{BASE_URL}/recording/start", json=data)
print(response.json())

# Get recording status
response = requests.get(f"{BASE_URL}/recording/status")
print(response.json())
```

### JavaScript Example

```javascript
const BASE_URL = 'http://raspberry-pi-ip:8080/api/v1';

// Check status
fetch(`${BASE_URL}/status`)
  .then(response => response.json())
  .then(data => console.log(data));

// Start recording
fetch(`${BASE_URL}/recording/start`, {
  method: 'POST',
  headers: {
    'Content-Type': 'application/json',
  },
  body: JSON.stringify({
    duration: 3600,
    filename: 'meeting.mp4'
  })
})
.then(response => response.json())
.then(data => console.log(data));

// WebSocket connection
const ws = new WebSocket('ws://raspberry-pi-ip:8081');
ws.onmessage = function(event) {
  const data = JSON.parse(event.data);
  console.log('Event:', data);
};
```
```

---

## 📂 `docs/api/uart-commands.md`

```markdown
# UART Command Interface Documentation
## RPi5 Corporate Recording Station

## 📋 Overview

The Recording Station provides a command-line interface via UART for headless operation and debugging. Connect to the UART console at **115200 baud, 8N1**.

## 🔌 Connection

### Hardware Setup

| USB-to-UART Adapter | Raspberry Pi 5 Pin | GPIO |
|---------------------|-------------------|------|
| GND (Black) | Pin 6 | GND |
| RX (White) | Pin 8 | GPIO 14 (TX) |
| TX (Green) | Pin 10 | GPIO 15 (RX) |

**Alternative**: Dedicated 3-pin UART connector (next to HDMI)

### Software Connection

```bash
# Using screen
screen /dev/ttyUSB0 115200

# Using minicom
minicom -D /dev/ttyUSB0 -b 115200

# Using tio
tio -b 115200 /dev/ttyUSB0
```

## 📝 Available Commands

### System Commands

| Command | Description | Example |
|---------|-------------|---------|
| `status` | Show system status | `status` |
| `help` | Show available commands | `help` |
| `uptime` | Show system uptime | `uptime` |
| `version` | Show software version | `version` |
| `reboot` | Reboot the system | `reboot` |
| `shutdown` | Shutdown the system | `shutdown` |

### Recording Commands

| Command | Description | Example |
|---------|-------------|---------|
| `record` | Start recording | `record` |
| `record 3600` | Start recording for 1 hour | `record 3600` |
| `record meeting.mp4` | Start recording with filename | `record meeting.mp4` |
| `stop` | Stop recording | `stop` |
| `recording` | Show recording status | `recording` |
| `list` | List recent recordings | `list` |
| `list 20` | List last 20 recordings | `list 20` |

### Storage Commands

| Command | Description | Example |
|---------|-------------|---------|
| `storage` | Show storage usage | `storage` |
| `files` | List files in recordings | `files` |
| `files /path` | List files in specific path | `files /var/lib/recording-station` |
| `delete <filename>` | Delete a recording | `delete meeting_20260708.mp4` |

### Cloud Commands

| Command | Description | Example |
|---------|-------------|---------|
| `cloud` | Show cloud status | `cloud` |
| `upload <filename>` | Upload a file to cloud | `upload meeting_20260708.mp4` |
| `sync` | Sync all files to cloud | `sync` |
| `provider` | Show current cloud provider | `provider` |

### Memory Commands

| Command | Description | Example |
|---------|-------------|---------|
| `memory` | Show memory usage | `memory` |
| `meminfo` | Show detailed memory info | `meminfo` |
| `zram` | Show ZRAM status | `zram` |
| `swap` | Show swap usage | `swap` |

### Hardware Commands

| Command | Description | Example |
|---------|-------------|---------|
| `gpio` | Show GPIO status | `gpio` |
| `gpio <pin>` | Show specific GPIO pin | `gpio 17` |
| `gpio <pin> <0/1>` | Set GPIO pin | `gpio 17 1` |
| `uart` | Show UART status | `uart` |
| `camera` | Show camera status | `camera` |
| `audio` | Show audio status | `audio` |
| `temp` | Show CPU temperature | `temp` |

### Debug Commands

| Command | Description | Example |
|---------|-------------|---------|
| `debug` | Enable debug mode | `debug` |
| `debug off` | Disable debug mode | `debug off` |
| `logs` | Show recent logs | `logs` |
| `logs 50` | Show last 50 logs | `logs 50` |
| `test` | Run system test | `test` |
| `test all` | Run all tests | `test all` |

## 📊 Command Output Examples

### `status`
```
╔═══════════════════════════════════════════════════════════╗
║              System Status                               ║
╠═══════════════════════════════════════════════════════════╣
║  Status:          Online                                 ║
║  Uptime:          12d 4h 32m                            ║
║  CPU Usage:       25.5%                                 ║
║  CPU Temp:        45.2°C                                ║
║  Memory:          1.5/4.0 GB (37.5%)                   ║
║  Storage:         76.8/128.0 GB (60.0%)                ║
║  Recording:       Idle                                  ║
║  Cloud:           Connected (Nextcloud)                 ║
║  UART:            Active (/dev/ttyAMA0)                ║
╚═══════════════════════════════════════════════════════════╝
```

### `recording`
```
╔═══════════════════════════════════════════════════════════╗
║              Recording Status                            ║
╠═══════════════════════════════════════════════════════════╣
║  Status:          Recording                              ║
║  File:            meeting_20260708.mp4                   ║
║  Duration:        00:30:45                              ║
║  Size:            750.5 MB                              ║
║  Video:           1920x1080 @ 30fps                     ║
║  Audio:           48kHz 2ch AAC                         ║
║  Started:         2026-07-08 10:30:45                  ║
║  Projected Size:  1.5 GB (at 1 hour)                   ║
╚═══════════════════════════════════════════════════════════╝
```

### `storage`
```
╔═══════════════════════════════════════════════════════════╗
║              Storage Information                         ║
╠═══════════════════════════════════════════════════════════╣
║  Total Space:     128.0 GB                              ║
║  Used Space:      76.8 GB (60.0%)                      ║
║  Free Space:      51.2 GB                              ║
║  Files:           1,247                                ║
║  Directories:     56                                   ║
║  Largest File:    meeting_20260707.mp4 (2.4 GB)        ║
║  Oldest File:     meeting_20260601.mp4 (37 days)       ║
║  Newest File:     meeting_20260708.mp4 (2 min ago)     ║
║  File Types:                                           ║
║    Video:         342                                   ║
║    Audio:         156                                   ║
║    Metadata:      89                                    ║
║    Other:         660                                   ║
╚═══════════════════════════════════════════════════════════╝
```

### `memory`
```
╔═══════════════════════════════════════════════════════════╗
║              Memory Information                          ║
╠═══════════════════════════════════════════════════════════╣
║  Total RAM:       4.0 GB                               ║
║  Used:            1.5 GB (37.5%)                      ║
║  Free:            2.5 GB                               ║
║  Cached:          512.0 MB                             ║
║  Buffers:         128.0 MB                             ║
║  Swap Total:      1.0 GB                               ║
║  Swap Used:       128.0 MB                             ║
║  ZRAM Status:     Enabled (4.5x compression)           ║
║  Peak Usage:      2.1 GB                               ║
║  Allocations:     15,234                               ║
║  Fragmentation:   3.2%                                 ║
╚═══════════════════════════════════════════════════════════╝
```

## 🛠️ Advanced Commands

### Filtering and Sorting
```bash
# List recordings by size (largest first)
list sort=size desc

# List recordings by date (oldest first)
list sort=date asc

# List video files only
list type=video

# List files from specific date
list date=2026-07-08
```

### Batch Operations
```bash
# Delete all files older than 30 days
delete older=30d

# Upload all files from last week
upload week

# Compress all files older than 7 days
compress older=7d
```

### Scripting
```bash
# Save command output to file
status > /tmp/status.txt

# Run script from UART
source /etc/recording-station/scripts/startup.cmd

# Execute command on schedule
schedule backup daily 02:00
```

## 📝 Creating Custom Commands

Custom commands can be added by creating scripts in `/etc/recording-station/commands/`:

```bash
#!/bin/bash
# /etc/recording-station/commands/mycommand
echo "Running my custom command..."
# Your code here
```

## 🔒 Security

- Commands are logged to `/var/log/recording-station/audit.log`
- Sensitive commands require authentication
- Rate limiting prevents command flooding
- All commands are validated before execution

## 🐛 Troubleshooting

### No Response from UART
```bash
# Check UART device
ls -l /dev/ttyAMA*

# Check baud rate
stty -F /dev/ttyAMA0 115200

# Verify config
cat /boot/firmware/config.txt | grep uart
```

### Command Not Found
```bash
# List all available commands
help

# Check command path
which <command>
```

### Permission Denied
```bash
# Add user to dialout group
sudo usermod -a -G dialout $USER

# Change device permissions
sudo chmod 666 /dev/ttyAMA0
```

## 📚 Command Reference Card

```
Quick Reference:
─────────────────────────────────────────────────────
status      → System status
record      → Start recording  
stop        → Stop recording
storage     → Storage usage
memory      → Memory usage
cloud       → Cloud status
upload <f>  → Upload file
sync        → Sync all files
list        → List recordings
delete <f>  → Delete recording
gpio [pin]  → GPIO control
temp        → CPU temperature
logs        → Show logs
help        → Show commands
reboot      → Reboot system
shutdown    → Shutdown system
─────────────────────────────────────────────────────
```

---

## 📂 `docs/api/websocket-api.md`

```markdown
# WebSocket API Documentation
## RPi5 Corporate Recording Station

## 📋 Overview

The Recording Station provides a WebSocket interface for real-time updates and bidirectional communication.

**Endpoint**: `ws://<raspberry-pi-ip>:8081`

**Protocol**: WebSocket (RFC 6455)

## 🔌 Connection

### JavaScript Connection

```javascript
const ws = new WebSocket('ws://raspberry-pi-ip:8081');

ws.onopen = function() {
    console.log('Connected to WebSocket server');
};

ws.onmessage = function(event) {
    const data = JSON.parse(event.data);
    console.log('Received:', data);
};

ws.onerror = function(error) {
    console.error('WebSocket error:', error);
};

ws.onclose = function() {
    console.log('Disconnected from server');
};
```

### Python Connection

```python
import websocket
import json

def on_message(ws, message):
    data = json.loads(message)
    print(f"Received: {data}")

def on_error(ws, error):
    print(f"Error: {error}")

def on_close(ws, close_status_code, close_msg):
    print("Connection closed")

def on_open(ws):
    print("Connected to WebSocket server")

ws = websocket.WebSocketApp("ws://raspberry-pi-ip:8081",
                            on_open=on_open,
                            on_message=on_message,
                            on_error=on_error,
                            on_close=on_close)
ws.run_forever()
```

## 📊 Server-Sent Events

### System Events

#### `system.connected`
Sent when a client connects.

```json
{
    "event": "system.connected",
    "data": {
        "client_id": "client_12345",
        "timestamp": "2026-07-08T10:30:45Z"
    }
}
```

#### `system.status`
Periodic system status updates (every 5 seconds).

```json
{
    "event": "system.status",
    "data": {
        "cpu_usage": 25.5,
        "cpu_temp": 45.2,
        "memory_used": 1536,
        "memory_total": 4096,
        "memory_percentage": 37.5,
        "uptime": 86400,
        "timestamp": "2026-07-08T10:30:45Z"
    }
}
```

### Recording Events

#### `recording.started`
Triggered when recording starts.

```json
{
    "event": "recording.started",
    "data": {
        "id": "rec_20260708_103045",
        "filename": "meeting_20260708.mp4",
        "start_time": "2026-07-08T10:30:45Z",
        "video": {
            "width": 1920,
            "height": 1080,
            "framerate": 30,
            "codec": "h264"
        },
        "audio": {
            "sample_rate": 48000,
            "channels": 2,
            "codec": "aac"
        }
    }
}
```

#### `recording.stopped`
Triggered when recording stops.

```json
{
    "event": "recording.stopped",
    "data": {
        "id": "rec_20260708_103045",
        "filename": "meeting_20260708.mp4",
        "duration": 3600,
        "size": 1500000000,
        "stop_time": "2026-07-08T11:30:45Z",
        "path": "/var/lib/recording-station/recordings/meeting_20260708.mp4"
    }
}
```

#### `recording.progress`
Sent during recording (every second).

```json
{
    "event": "recording.progress",
    "data": {
        "id": "rec_20260708_103045",
        "duration": 1800,
        "size": 750000000,
        "progress": 50.0,  // percentage
        "estimated_completion": "2026-07-08T11:00:45Z"
    }
}
```

### Storage Events

#### `storage.file_added`
Triggered when a new file is added.

```json
{
    "event": "storage.file_added",
    "data": {
        "filename": "meeting_20260708.mp4",
        "path": "/var/lib/recording-station/recordings/meeting_20260708.mp4",
        "size": 1500000000,
        "type": "video",
        "timestamp": "2026-07-08T10:30:45Z"
    }
}
```

#### `storage.file_modified`
Triggered when a file is modified.

```json
{
    "event": "storage.file_modified",
    "data": {
        "filename": "meeting_20260708.mp4",
        "path": "/var/lib/recording-station/recordings/meeting_20260708.mp4",
        "old_size": 1000000000,
        "new_size": 1500000000,
        "timestamp": "2026-07-08T10:35:45Z"
    }
}
```

#### `storage.file_deleted`
Triggered when a file is deleted.

```json
{
    "event": "storage.file_deleted",
    "data": {
        "filename": "meeting_20260708.mp4",
        "path": "/var/lib/recording-station/recordings/meeting_20260708.mp4",
        "size": 1500000000,
        "timestamp": "2026-07-08T11:30:45Z"
    }
}
```

#### `storage.updated`
Periodic storage status updates.

```json
{
    "event": "storage.updated",
    "data": {
        "total": 128000,
        "used": 76800,
        "free": 51200,
        "percentage": 60.0,
        "files": 1247,
        "directories": 56,
        "timestamp": "2026-07-08T10:30:45Z"
    }
}
```

### Cloud Events

#### `cloud.connected`
Triggered when cloud connection is established.

```json
{
    "event": "cloud.connected",
    "data": {
        "provider": "nextcloud",
        "url": "https://nextcloud.example.com",
        "username": "recording_station",
        "timestamp": "2026-07-08T10:30:45Z"
    }
}
```

#### `cloud.upload_started`
Triggered when cloud upload starts.

```json
{
    "event": "cloud.upload_started",
    "data": {
        "filename": "meeting_20260708.mp4",
        "size": 1500000000,
        "provider": "nextcloud",
        "timestamp": "2026-07-08T10:35:45Z"
    }
}
```

#### `cloud.upload_progress`
Sent during cloud upload (every 10%).

```json
{
    "event": "cloud.upload_progress",
    "data": {
        "filename": "meeting_20260708.mp4",
        "progress": 50.0,
        "uploaded": 750000000,
        "total": 1500000000,
        "speed": 1024000,  // bytes/second
        "eta": 730  // seconds
    }
}
```

#### `cloud.upload_complete`
Triggered when cloud upload completes.

```json
{
    "event": "cloud.upload_complete",
    "data": {
        "filename": "meeting_20260708.mp4",
        "cloud_path": "/Recordings/2026/07/meeting_20260708.mp4",
        "size": 1500000000,
        "duration": 120,  // upload duration in seconds
        "timestamp": "2026-07-08T10:37:45Z"
    }
}
```

#### `cloud.upload_failed`
Triggered when cloud upload fails.

```json
{
    "event": "cloud.upload_failed",
    "data": {
        "filename": "meeting_20260708.mp4",
        "error": "Connection timeout",
        "attempts": 3,
        "timestamp": "2026-07-08T10:37:45Z"
    }
}
```

### Hardware Events

#### `hardware.gpio_changed`
Triggered when GPIO state changes.

```json
{
    "event": "hardware.gpio_changed",
    "data": {
        "pin": 17,
        "state": 1,
        "timestamp": "2026-07-08T10:30:45Z"
    }
}
```

#### `hardware.button_pressed`
Triggered when physical button is pressed.

```json
{
    "event": "hardware.button_pressed",
    "data": {
        "pin": 22,
        "type": "start",
        "timestamp": "2026-07-08T10:30:45Z"
    }
}
```

#### `hardware.temperature_warning`
Triggered when temperature exceeds threshold.

```json
{
    "event": "hardware.temperature_warning",
    "data": {
        "temperature": 82.5,
        "threshold": 80.0,
        "level": "warning",
        "timestamp": "2026-07-08T10:30:45Z"
    }
}
```

## 📤 Client Commands

### Subscribe to Events

```javascript
// Subscribe to specific events
ws.send(JSON.stringify({
    "action": "subscribe",
    "events": ["recording.*", "storage.file_added"]
}));

// Unsubscribe from events
ws.send(JSON.stringify({
    "action": "unsubscribe",
    "events": ["recording.progress"]
}));
```

### Send Commands

```javascript
// Start recording
ws.send(JSON.stringify({
    "action": "command",
    "command": "record",
    "params": {
        "duration": 3600,
        "filename": "meeting.mp4"
    }
}));

// Stop recording
ws.send(JSON.stringify({
    "action": "command",
    "command": "stop"
}));

// Upload file
ws.send(JSON.stringify({
    "action": "command",
    "command": "upload",
    "params": {
        "file": "/var/lib/recording-station/recordings/meeting.mp4"
    }
}));
```

## 📊 Event Categories

| Category | Events | Description |
|----------|--------|-------------|
| System | `system.*` | System status and health |
| Recording | `recording.*` | Recording lifecycle |
| Storage | `storage.*` | File system changes |
| Cloud | `cloud.*` | Cloud operations |
| Hardware | `hardware.*` | Hardware events |
| Security | `security.*` | Security events |

## 🔧 Client Implementation

### Complete JavaScript Client

```javascript
class RecordingStationWebSocket {
    constructor(url) {
        this.url = url;
        this.ws = null;
        this.listeners = {};
        this.reconnectAttempts = 0;
        this.maxReconnectAttempts = 10;
    }
    
    connect() {
        this.ws = new WebSocket(this.url);
        
        this.ws.onopen = () => {
            console.log('Connected to WebSocket server');
            this.reconnectAttempts = 0;
            this.dispatch('connected', {});
        };
        
        this.ws.onmessage = (event) => {
            const data = JSON.parse(event.data);
            this.dispatch(data.event, data.data);
        };
        
        this.ws.onerror = (error) => {
            console.error('WebSocket error:', error);
            this.dispatch('error', error);
        };
        
        this.ws.onclose = () => {
            console.log('Disconnected from server');
            this.dispatch('disconnected', {});
            this.reconnect();
        };
    }
    
    reconnect() {
        if (this.reconnectAttempts < this.maxReconnectAttempts) {
            this.reconnectAttempts++;
            const delay = Math.min(1000 * Math.pow(2, this.reconnectAttempts), 30000);
            console.log(`Reconnecting in ${delay}ms...`);
            setTimeout(() => this.connect(), delay);
        } else {
            console.error('Max reconnection attempts reached');
        }
    }
    
    on(event, callback) {
        if (!this.listeners[event]) {
            this.listeners[event] = [];
        }
        this.listeners[event].push(callback);
    }
    
    dispatch(event, data) {
        if (this.listeners[event]) {
            this.listeners[event].forEach(callback => callback(data));
        }
    }
    
    send(message) {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            this.ws.send(JSON.stringify(message));
            return true;
        }
        return false;
    }
    
    sendCommand(command, params = {}) {
        return this.send({
            action: 'command',
            command: command,
            params: params
        });
    }
    
    subscribe(events) {
        return this.send({
            action: 'subscribe',
            events: events
        });
    }
    
    close() {
        if (this.ws) {
            this.ws.close();
        }
    }
}

// Usage Example
const ws = new RecordingStationWebSocket('ws://raspberry-pi-ip:8081');

// Connect
ws.connect();

// Listen for events
ws.on('recording.started', (data) => {
    console.log('Recording started:', data.filename);
});

ws.on('recording.stopped', (data) => {
    console.log('Recording stopped:', data.filename, data.duration);
});

ws.on('storage.file_added', (data) => {
    console.log('New file:', data.filename);
});

// Start recording
ws.sendCommand('record', { duration: 3600 });

// Stop recording
ws.sendCommand('stop');
```

## 🔒 Security

- WebSocket connections require valid authentication token
- All messages are validated for format and content
- Rate limiting prevents abuse
- Message size limited to 1MB
- TLS/SSL supported via `wss://`

## 📈 Performance

- **Message Rate**: Up to 1000 messages/second
- **Latency**: < 10ms for local connections
- **Payload Size**: Max 1MB per message
- **Connections**: Up to 100 concurrent clients
- **Bandwidth**: ~1Mbps for high-frequency updates

## 🐛 Error Handling

### Connection Errors
```json
{
    "event": "error",
    "data": {
        "code": "CONNECTION_FAILED",
        "message": "Failed to establish connection",
        "timestamp": "2026-07-08T10:30:45Z"
    }
}
```

### Authentication Errors
```json
{
    "event": "error",
    "data": {
        "code": "UNAUTHORIZED",
        "message": "Invalid authentication token",
        "timestamp": "2026-07-08T10:30:45Z"
    }
}
```

### Command Errors
```json
{
    "event": "error",
    "data": {
        "code": "COMMAND_FAILED",
        "message": "Recording already in progress",
        "command": "record",
        "timestamp": "2026-07-08T10:30:45Z"
    }
}
```

## 📋 Event Reference Card

```
Quick Reference:
─────────────────────────────────────────────────────
System Events:
  system.connected       → Client connected
  system.status         → System status update

Recording Events:
  recording.started     → Recording started
  recording.stopped     → Recording stopped  
  recording.progress    → Recording progress

Storage Events:
  storage.file_added    → File added
  storage.file_modified → File modified
  storage.file_deleted  → File deleted
  storage.updated       → Storage update

Cloud Events:
  cloud.connected       → Cloud connected
  cloud.upload_started  → Upload started
  cloud.upload_progress → Upload progress
  cloud.upload_complete → Upload complete
  cloud.upload_failed   → Upload failed

Hardware Events:
  hardware.gpio_changed     → GPIO changed
  hardware.button_pressed   → Button pressed
  hardware.temperature_warning → Temperature warning
─────────────────────────────────────────────────────
```

---

