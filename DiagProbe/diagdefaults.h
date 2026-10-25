#ifndef DIAGDEFAULTS_H
#define DIAGDEFAULTS_H

// Values the diagnostic description does not (yet) contain. The ECU's UDS server
// must use the same ones.

// S3server: without a request for this long the ECU falls back to the default session.
const int S3Time = 5000; // ms

// The tester sends TesterPresent (3E 80) this often while it is outside the default session.
const int TesterPresentInterval = 2000; // ms

// Added to P2 / P2* while the tester waits for a response, for the delay of the PC itself.
const int ResponseTimeMargin = 100; // ms

#endif // DIAGDEFAULTS_H
