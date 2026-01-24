# Security Hardening Documentation

## Overview

This firmware has been hardened with critical security features to protect against common web application vulnerabilities and unauthorized access.

## ⚠️ CRITICAL: Default Credentials

**YOU MUST CHANGE THE DEFAULT PASSWORD BEFORE DEPLOYMENT**

Default credentials (located at top of `.ino` file):
```
Username: admin
Password: CHANGE_ME_NOW
```

**How to change:**
1. Open the `.ino` file
2. Find the security configuration section (around line 20)
3. Change `AUTH_PASSWORD` to a strong password
4. Optionally change `AUTH_USERNAME`
5. Recompile and upload

## Security Features Implemented

### 1. HTTP Basic Authentication

- **Protected Endpoints**: All sensitive operations require authentication
- **Rate Limiting**: 3 failed attempts trigger 5-second lockout
- **Credentials**: Configurable username/password at compile time

**Protected Operations:**
- I2C device configuration (`/connect`, `/setpin`, `/changeaddr`)
- Firmware flashing (`/flashfw`, `/upload_fw`, `/upload_hex`)
- File operations (SD card and LittleFS)
- Configuration management
- System settings
- WiFi reset
- Filesystem format
- OTA updates

**Public Endpoints:**
- `/` - Dashboard (read-only display)
- `/api/health` - Health check for monitoring

### 2. Input Validation

All user inputs are validated before processing:

**I2C Address Validation:**
- Range: 0x08 to 0x77 (valid 7-bit I2C addresses)
- Prevents invalid bus operations

**Pin Number Validation:**
- Range: 0-39 (ESP32 GPIO range)
- Excludes flash pins (6-11)
- Prevents damage to hardware

**Path Validation:**
- No parent directory traversal (`..`)
- No null bytes
- No control characters
- Must start with `/` (absolute path)

**Filename Validation:**
- Alphanumeric, underscore, dash, dot only
- No hidden files (starting with `.`)
- Max 255 characters

**JSON Size Validation:**
- Maximum 8KB payload
- Prevents memory exhaustion

**Upload Size Validation:**
- Maximum 5MB per upload
- Prevents storage exhaustion

### 3. Path Traversal Protection

Enhanced protection against directory traversal attacks:

- `sanitizePath()` removes `..` sequences
- `isPathSafe()` validates path structure
- `isFilenameSafe()` validates filename characters
- All file operations use validated paths

### 4. Security Headers

All HTTP responses include security headers:

```
X-Content-Type-Options: nosniff
X-Frame-Options: DENY
X-XSS-Protection: 1; mode=block
Content-Security-Policy: default-src 'self' 'unsafe-inline'; img-src 'self' data:;
Strict-Transport-Security: max-age=31536000; includeSubDomains
Access-Control-Allow-Origin: * (configure for production)
```

**Protects Against:**
- MIME type sniffing attacks
- Clickjacking (iframe embedding)
- Cross-site scripting (XSS)
- Mixed content attacks

### 5. Request Size Limits

Configurable limits prevent resource exhaustion:

- **Upload Size**: 5MB maximum
- **JSON Payload**: 8KB maximum
- Configurable via `MAX_UPLOAD_SIZE` and `MAX_JSON_SIZE`

### 6. Rate Limiting

Simple rate limiting on authentication:

- Track failed login attempts
- 5-second lockout after 3 failures
- Automatic reset after timeout

### 7. OTA Security

Over-The-Air updates are password-protected:

- Uses same password as HTTP authentication
- Hostname: `wombat-bridge`
- Error logging for troubleshooting
- Protected against unauthorized firmware updates

### 8. Error Sanitization

Error messages are sanitized to prevent information disclosure:

- Filesystem paths are redacted (`/littlefs/` → `[FS]/`)
- Error messages limited to 128 characters
- Generic errors for failed operations

### 9. Health Check Endpoint

Public monitoring endpoint at `/api/health`:

```json
{
  "status": "ok",
  "uptime_ms": 123456,
  "heap_free": 234567,
  "wifi_connected": true,
  "wifi_rssi": -45,
  "ip": "192.168.1.100",
  "sd_mounted": true
}
```

**Use Cases:**
- External monitoring systems
- Health checks in docker/k8s
- Status dashboards
- No sensitive information exposed

## Configuration Options

### Compile-Time Settings

Located at top of `.ino` file:

```cpp
#define SECURITY_ENABLED 1           // Enable/disable security
#define AUTH_USERNAME "admin"        // HTTP auth username
#define AUTH_PASSWORD "CHANGE_ME_NOW" // HTTP auth password (CHANGE THIS!)
#define MAX_UPLOAD_SIZE (5 * 1024 * 1024)  // 5MB max upload
#define MAX_JSON_SIZE 8192           // 8KB max JSON
```

### Disabling Security (NOT RECOMMENDED)

Set `SECURITY_ENABLED 0` to disable authentication.

**WARNING:** Only disable for testing in isolated networks. Never expose to internet without authentication.

## Best Practices

### For Production Deployment

1. **Change default password** to strong, unique password
2. **Enable HTTPS** if possible (requires certificate setup)
3. **Restrict CORS** - Change `Access-Control-Allow-Origin` from `*` to specific domain
4. **Use VPN or firewall** - Don't expose directly to internet
5. **Regular updates** - Keep firmware updated with security patches
6. **Monitor logs** - Watch for authentication failures
7. **Backup configurations** - Before making changes

### Network Security

- Deploy on isolated network segment
- Use firewall rules to restrict access
- Consider VPN for remote access
- Monitor for unusual traffic patterns

### Password Guidelines

Strong passwords should:
- Be at least 12 characters
- Include uppercase, lowercase, numbers, symbols
- Not be dictionary words
- Not be reused from other systems
- Be changed periodically

Example strong password: `Womb@t2024!Secur3`

## Known Limitations

### Current Implementation

1. **Single User**: Only one username/password (no multi-user support)
2. **In-Memory Credentials**: Password stored in firmware (not runtime configurable)
3. **HTTP Basic Auth**: Credentials sent base64 encoded (use HTTPS in production)
4. **Simple Rate Limiting**: Basic lockout mechanism (not persistent across reboots)
5. **No Session Management**: Authentication required on each request
6. **CORS Wide Open**: Default allows all origins (configure for production)

### Future Enhancements

Consider implementing:
- Runtime password configuration via secure setup
- HTTPS/TLS encryption
- Session tokens/JWT
- Role-based access control
- Persistent rate limiting
- Audit logging
- Two-factor authentication

## Testing Security

### Test Authentication

```bash
# Should fail without auth
curl http://192.168.1.100/api/system

# Should succeed with auth
curl -u admin:CHANGE_ME_NOW http://192.168.1.100/api/system
```

### Test Rate Limiting

```bash
# Try 4 failed logins rapidly
for i in {1..4}; do
  curl -u admin:wrong http://192.168.1.100/api/system
done
# Should see 429 Too Many Requests on 4th attempt
```

### Test Path Traversal Protection

```bash
# Should fail - path traversal attempt
curl -u admin:CHANGE_ME_NOW \
  -X POST http://192.168.1.100/api/sd/list \
  -H "Content-Type: application/json" \
  -d '{"dir":"/../../../etc"}'
```

### Test Input Validation

```bash
# Should fail - invalid I2C address
curl -u admin:CHANGE_ME_NOW \
  "http://192.168.1.100/connect?addr=FF"
```

## Incident Response

If you suspect a security breach:

1. **Immediately change password**
2. **Check logs** for unauthorized access
3. **Review configuration** for unexpected changes
4. **Scan for malware** if firmware was modified
5. **Update firmware** to latest version
6. **Isolate device** from network if necessary

## Security Disclosure

If you discover a security vulnerability:

1. Do not publicly disclose until patched
2. Contact maintainers privately
3. Provide detailed reproduction steps
4. Allow time for fix before public disclosure

## Compliance Notes

This implementation provides:
- **Authentication** - HTTP Basic Auth
- **Authorization** - Role-based (single admin role)
- **Input Validation** - Comprehensive validation
- **Error Handling** - Sanitized error messages
- **Security Headers** - Industry standard headers
- **Audit Trail** - Serial logging of security events

Consider additional hardening for regulated environments.
