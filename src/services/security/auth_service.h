#pragma once

#include <WebServer.h>
#include "../../config/defaults.h"

// Rate limiting globals
extern unsigned long g_last_auth_fail;
extern uint8_t g_auth_fail_count;

/**
 * Add security headers to HTTP response
 * Implements: CORS, CSP, X-Frame-Options, X-Content-Type-Options
 * 
 * Notes:
 * - HSTS is included but only effective over HTTPS (not yet implemented)
 * - CSP uses 'unsafe-inline' due to embedded HTML with inline scripts
 * - CORS uses wildcard by default - CHANGE CORS_ALLOW_ORIGIN for production
 */
void addSecurityHeaders(WebServer& server);

/**
 * Check HTTP Basic Authentication
 * Returns true if authenticated or security disabled, false otherwise
 */
bool checkAuth(WebServer& server);
