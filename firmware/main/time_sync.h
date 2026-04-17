#pragma once

namespace tokmagotchi {

// Start SNTP against pool.ntp.org. Non-blocking; time becomes valid after a
// few seconds. The rest of the system should treat time() returning a year
// < 2024 as "not synced yet".
void time_sync_start();

// Set TZ from a POSIX TZ string (e.g. "PST8PDT,M3.2.0,M11.1.0"). Safe to
// call repeatedly; defaults to UTC if never called.
void time_sync_set_timezone(const char *tz);

bool time_sync_is_ready();

} // namespace tokmagotchi
