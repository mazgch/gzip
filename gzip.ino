//
// zlib_turbo example showing how to decompress gzip
// data received from a HTTP request
// written by Larry Bank (bitbank@pobox.com)
// July 8, 2024
//
// This particular example pings a US grocery store chain (Publix)
// Their website returns a relatively small html response and accepts
// gzip as a Content-Encoding option
//
#include <WiFi.h>
#include <HTTPClient.h>
#include "tinf.h"

HTTPClient http;
//const char *url = "https://epics.anl.gov/download/extensions/caObject2_1_1_3_0.tar.gz"; // 12kB
const char *url = "https://epics.anl.gov/download/extensions/ts_20070703.tar.gz"; //16kB
const char *SSID = "MAZGCH_iot";
const char *PW = "xiro1234";
#define TIMEOUT 2000

static unsigned int my_read_le32(const unsigned char *p)
{
	return ((unsigned int) p[0])
	     | ((unsigned int) p[1] << 8)
	     | ((unsigned int) p[2] << 16)
	     | ((unsigned int) p[3] << 24);
}

//#define STATIC 
#ifdef STATIC 
static uint8_t pCompressed[16034+8];
static uint8_t pUncompressed[71680];
#endif

void setup()
{
  int iTimeout, httpCode;
#ifndef STATIC
  uint8_t *pCompressed = NULL, *pUncompressed = NULL;
#endif
  long l, iCount, iCompressedSize = 0, iUncompressedSize = 0;
  WiFiClient * stream;
  Serial.begin(115200);
  delay(2000); // give a moment for serial to start
  Serial.println("HTTP GZIP Example");
  // Connect to wifi
  WiFi.begin(SSID, PW);
  iTimeout = 0;
  Serial.print("Connecting to wifi");
  while (WiFi.status() != WL_CONNECTED && WiFi.status() != WL_CONNECT_FAILED && iTimeout < TIMEOUT)
  {
    delay(500); // allow up to 10 seconds to connect
    iTimeout++;
    Serial.print(".");
  }
  if (iTimeout == TIMEOUT) {
    Serial.println("\nConnection timed out!");
  } else {
    Serial.println("\nConnected!");
    Serial.printf("Sending GET request to %s\n", url);
    http.begin(url);
    http.setAcceptEncoding("gzip"); // ask for response to be compressed
    httpCode = http.GET();  //send GET request
    iCompressedSize = 0;
    if (httpCode != 200) {
      Serial.print("Error on HTTP request: ");
      Serial.println(httpCode);
      http.end();
    } else {
      Serial.println("GET request succeeded (return code = 200)");
      iCompressedSize = http.getSize();
      Serial.printf("payload size = %ld\n", iCompressedSize);
      // Allocate a buffer to receive the compressed (gzip) response
#ifdef STATIC
      if (iCompressedSize + 8 <= sizeof(pCompressed))
#else
      pCompressed = (uint8_t *)malloc(iCompressedSize+8); // allow a little extra for reading past the end
      if(pCompressed)
#endif
      {
        stream = http.getStreamPtr();
        iCount = 0;
        // Allow 4 seconds to receive the compressed data
        l = millis();
        while (iCount < iCompressedSize && (millis() - l) < 4000) {
            if (stream->available()) {
                char c = stream->read();
                pCompressed[iCount++] = c;
            } else {
                vTaskDelay(5); // allow time for data to receive
            }
        } // while
      } else {
        Serial.printf("Not enough memory for %ld\n", iCompressedSize);
        iCompressedSize = 0;
      }

      http.end(); // we're done, close the connection
    } // http connection succeeded
    WiFi.disconnect();
  } // wifi connection
  
  if (iCompressedSize > 12 && pCompressed[0] == 0x1f && pCompressed[1] == 0x8b) {
    Serial.println("It's a gzip file!");
    iUncompressedSize = my_read_le32(pCompressed + iCompressedSize - 4); // allow a little extra for reading past the end
#ifdef STATIC
    if (iUncompressedSize <= sizeof(pUncompressed))
#else
    pUncompressed = (uint8_t *)malloc(iUncompressedSize); // allow a little extra for reading past the end
    if(pUncompressed)
#endif
    {
      unsigned long startTime = millis();
      unsigned int decSize = iUncompressedSize;
      int res = tinf_gzip_uncompress(pUncompressed, &decSize, pCompressed, iCompressedSize);
      unsigned long deltaTime = millis() - startTime;
#ifndef STATIC
      free(pUncompressed);
#endif
      Serial.printf("Uncompressed %d %s compressed %ld expected %d uncompressed %ld byte in %ld ms\n", 
          res, (res == TINF_DATA_ERROR) ? "DATA_ERR" : (res == TINF_BUF_ERROR) ? "BUF_ERR" : (res == TINF_OK) ? "OK" : "?", 
          iCompressedSize, decSize, iUncompressedSize, deltaTime);
    }
    else {
      Serial.printf("Not enough memory for %ld\n", iUncompressedSize);
    }
  } else {
    Serial.println("It's not a gzip file, something went wrong :(");
  }
#ifndef STATIC
  free(pCompressed);
#endif

} /* setup() */

void loop()
{
} /* loop() */
