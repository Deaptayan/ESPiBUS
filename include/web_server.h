#pragma once

void webServerInit();

// Call repeatedly from the web task's loop. Non-blocking-ish (handles at
// most one client action per call, as with any WebServer-based sketch).
void webServerPoll();
