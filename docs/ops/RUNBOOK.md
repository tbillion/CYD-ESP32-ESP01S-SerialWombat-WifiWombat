# Operations Runbook - CYD ESP32 Serial Wombat

**Version:** 2.0.0 | **Date:** 2026-01-24

## Quick Reference

### Essential Commands
```bash
# Health check (no auth)
curl http://device-ip/api/health

# System info (requires auth)
curl -u admin:password http://device-ip/api/system

# I2C scan
curl -u admin:password http://device-ip/scan-data

# Config backup
curl -u admin:password http://device-ip/api/config/load > backup.json

# Serial monitor
pio device monitor --baud 115200
```

### Default Settings
- HTTP: Port 80
- TCP Bridge: Port 23
- OTA: Port 3232
- Serial: 115200 baud
- Default AP: `Wombat-Setup`
- Default credentials: `admin:CHANGE_ME_NOW` (MUST CHANGE)

## Pre-Deployment Checklist

**Security Configuration (REQUIRED):**
1. Edit firmware: `CYD_Framework_LGFX_LVGL_Final_COMPILE_OK_FIXED6.ino`
2. Change `AUTH_PASSWORD` (line 26)
3. Change `OTA_PASSWORD` (line 30)  
4. Configure `CORS_ALLOW_ORIGIN` (line 47)
5. Recompile: `pio run`
6. Verify no security warnings in build output

**Network Setup:**
- Isolated IoT VLAN recommended
- VPN or bastion host for management
- WPA3 WiFi (minimum WPA2-Enterprise)
- Static DHCP reservation
- Firewall rules limiting access

## Deployment

### 1. Flash Firmware
```bash
pio run --target upload --environment esp32-devkit
pio device monitor --baud 115200
```

### 2. Configure WiFi
1. Connect to AP: `Wombat-Setup`
2. Enter WiFi credentials in captive portal
3. Device reboots and connects

### 3. Verify
- [ ] Device boots
- [ ] Web interface accessible
- [ ] Authentication works
- [ ] I2C scan functions
- [ ] Display works (if enabled)

## Monitoring

### Automated Health Check Script
```bash
#!/bin/bash
DEVICE_IP="192.168.1.100"
curl -s http://$DEVICE_IP/api/health || echo "Device down" | mail -s "Alert" ops@example.com
```

Schedule with cron: `*/5 * * * * /usr/local/bin/check-esp32.sh`

### Key Metrics
- Heap memory: Alert if <50KB
- WiFi RSSI: Alert if <-75dBm
- Auth failures: Alert if >10/hour
- Uptime: Track for stability

## Troubleshooting

### Device Unreachable
1. Check power (LED on?)
2. Check network: `ping device-ip`
3. Check serial: `pio device monitor`
4. Reset WiFi: Hold button 10s or use `/resetwifi`
5. Power cycle if hung
6. Reflash if repeated failures

### Authentication Issues
1. Verify password in source code
2. Wait 5s if rate limited
3. Check serial logs for "AUTH FAIL"
4. Reflash with correct password if needed

### Memory Problems
1. Check heap: `curl http://device-ip/api/health`
2. If <50KB, investigate leak
3. Immediate: Reboot device
4. Long-term: Disable unused features or report issue

### I2C Not Working
1. Run scan: `curl -u admin:pass http://device-ip/scan-data`
2. Check wiring (SDA/SCL, pullups, power)
3. Verify pins in config: `/api/system`
4. Update config if needed

### SD Card Issues
1. Check status: `curl -u admin:pass http://device-ip/api/sd/status`
2. Ensure FAT32 format
3. Reinsert card
4. Verify pin configuration

### Display Problems
1. Check serial logs for init errors
2. Verify panel type in config matches hardware
3. Check pin configuration
4. Test headless mode: `DISPLAY_SUPPORT_ENABLED=0`

## Backup & Recovery

### Daily Config Backup
```bash
#!/bin/bash
DATE=$(date +%Y%m%d)
curl -u admin:pass http://device-ip/api/config/load > /backup/config_$DATE.json
find /backup -name "config_*.json" -mtime +30 -delete
```

### Restore Configuration
```bash
curl -u admin:pass \
  -X POST \
  -H "Content-Type: application/json" \
  -d @backup/config.json \
  http://device-ip/api/config/save
```

### Disaster Recovery
1. Flash firmware on replacement device
2. Configure WiFi
3. Restore config from backup
4. Verify functionality

**RTO:** <1 hour | **RPO:** 24 hours

## Security Incidents

### Suspected Breach Response
1. **Isolate:** Disconnect from network
2. **Preserve:** Copy all logs and config
3. **Analyze:** Review logs for indicators
4. **Remediate:** Change passwords, reflash firmware
5. **Monitor:** Enhanced monitoring for 30 days

### Brute Force Attack
1. Check serial logs for repeated "AUTH FAIL"
2. Identify attacker IP from logs
3. Block at firewall
4. Change password immediately
5. Review for successful breach

## Emergency Procedures

### Emergency Shutdown
```bash
# Graceful (if accessible)
curl -u admin:pass -X POST http://device-ip/api/system/shutdown

# Or disconnect power
```

### Emergency Reflash
```bash
# Physical access required
# Connect USB, hold BOOT button, press RESET
pio run --target upload --upload-port /dev/ttyUSB0
```

### Factory Reset
```bash
curl -u admin:pass -X POST http://device-ip/formatfs
# Deletes all config, recreates defaults on reboot
```

## Firmware Updates

### Update Procedure
1. **Backup:** `curl -u admin:pass http://device-ip/api/config/load > backup.json`
2. **Update:** `python -m espota -i device-ip -p 3232 -f firmware.bin --auth=password`
3. **Verify:** `curl http://device-ip/api/health`
4. **Monitor:** Watch for 1 hour

### Rollback
```bash
git checkout previous-version
pio run --target upload
# Restore config from backup
```

## Routine Maintenance

**Daily:** Automated health checks  
**Weekly:** System review, config backup  
**Monthly:** Security log review, dependency updates  
**Quarterly:** Full audit, DR test

## Contact Information

- **Primary On-Call:** [Name] [Phone] [Email]
- **Security Team:** [Email]
- **Documentation:** See `/docs` directory

## Additional Resources

- [README.md](../../README.md) - Complete documentation
- [SECURITY.md](../../SECURITY.md) - Security guidelines
- [docs/security/THREAT_MODEL.md](../security/THREAT_MODEL.md) - Threat analysis
- [docs/audit/HARDENING_PLAN.md](../audit/HARDENING_PLAN.md) - Security roadmap

---

**Last Updated:** 2026-01-24  
**Next Review:** 2026-04-24
