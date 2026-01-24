#include "auth_service.h"

// Rate limiting (simple time-based)
unsigned long g_last_auth_fail = 0;
uint8_t g_auth_fail_count = 0;

/**
 * Add security headers to HTTP response
 * Implements: CORS, CSP, X-Frame-Options, X-Content-Type-Options
 * 
 * Notes:
 * - HSTS is included but only effective over HTTPS (not yet implemented)
 * - CSP uses 'unsafe-inline' due to embedded HTML with inline scripts
 * - CORS uses wildcard by default - CHANGE CORS_ALLOW_ORIGIN for production
 */
void addSecurityHeaders(WebServer& server) {
  server.sendHeader("X-Content-Type-Options", "nosniff");
  server.sendHeader("X-Frame-Options", "DENY");
  server.sendHeader("X-XSS-Protection", "1; mode=block");
  // CSP: 'unsafe-inline' required for embedded HTML, consider external JS in future
  server.sendHeader("Content-Security-Policy", "default-src 'self' 'unsafe-inline'; img-src 'self' data:;");
  // HSTS: Only effective over HTTPS, included for future HTTPS implementation
  server.sendHeader("Strict-Transport-Security", "max-age=31536000; includeSubDomains");
  // CORS - PRODUCTION WARNING: Change CORS_ALLOW_ORIGIN to specific domain
  server.sendHeader("Access-Control-Allow-Origin", CORS_ALLOW_ORIGIN);
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
}

/**
 * Check HTTP Basic Authentication
 * Returns true if authenticated or security disabled, false otherwise
 */
bool checkAuth(WebServer& server) {
#if !SECURITY_ENABLED
  return true;  // Security disabled at compile time
#endif

  // Rate limiting: simple lockout after repeated failures
  if (g_auth_fail_count >= 3) {
    unsigned long now = millis();
    if (now - g_last_auth_fail < AUTH_LOCKOUT_MS) {
      server.send(429, "text/plain", "Too many failed attempts. Try again later.");
      return false;
    } else {
      // Reset after lockout period
      g_auth_fail_count = 0;
    }
  }

  if (!server.authenticate(AUTH_USERNAME, AUTH_PASSWORD)) {
    g_auth_fail_count++;
    g_last_auth_fail = millis();
    server.requestAuthentication(BASIC_AUTH, "Wombat Manager", "Authentication required");
    return false;
  }

  // Success - reset fail counter
  g_auth_fail_count = 0;
  return true;
}
