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
#include "lib_inflate.h"

HTTPClient http;

const char *urls[] = {
  "https://epics.anl.gov/download/extensions/caObject2_1_1_3_0.tar.gz", // 12kB
  "https://epics.anl.gov/download/extensions/ts_20070703.tar.gz", //16kB
  "https://raw.githubusercontent.com/mazgch/gzip/refs/heads/main/tinf-master.zip.gz", //41 kb
  NULL // this will use gzfile
};
const char *SSID = "u-Guest"; //"MAZGCH_iot";
const char *PW = "GqpZvmK8@r5yL#AP";//"xiro1234";
#define TIMEOUT 2000

void setup()
{
  int iTimeout, httpCode;
  uint8_t *pCompressed = NULL, *pUncompressed = NULL;
  long l, iCount; 
  unsigned int iCompressedSize = 0, iUncompressedSize = 0;
  WiFiClient * stream;
  Serial.begin(115200);
  delay(2000); // give a moment for serial to start
  Serial.println(
    "HTTP UBLOX GUNZIP with "
#ifdef LIB_INFLATE_ERROR_ENABLED
      " ERROR"
#endif
#ifdef LIB_INFLATE_CRC_ENABLED 
      " CRC"
#endif
    );
  // Connect to wifi
  WiFi.begin(SSID, PW);
  iTimeout = 0;
  Serial.printf("Connecting to wifi %s ", SSID);
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
    for (int u = 0; u < sizeof(urls)/sizeof(*urls); u ++) {
      const char * url = urls[u];
      iCompressedSize = 0;
      if (!url) {
        iCompressedSize = lib_inflate_gzromSize;
        pCompressed = (uint8_t *)lib_inflate_gzromFile;
      } else {
        Serial.printf("GET %s\n", url);
        http.begin(url);
        http.setAcceptEncoding("gzip"); // ask for response to be compressed
        httpCode = http.GET();  //send GET request
        if (httpCode != 200) {
          Serial.print("Error on HTTP request: ");
          Serial.println(httpCode);
          http.end();
        } else {
          //Serial.println("GET request succeeded (return code = 200)");
          iCompressedSize = http.getSize();
          //Serial.printf("iCompressedSize = %ld\n", iCompressedSize);
          // Allocate a buffer to receive the compressed (gzip) response
          pCompressed = (uint8_t *)malloc(iCompressedSize+8); // allow a little extra for reading past the end
          if(pCompressed)
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
            Serial.printf("Not enough memory for %d\n", iCompressedSize);
            iCompressedSize = 0;
          }
        }
        http.end(); // we're done, close the connection
      }

      if (iCompressedSize > 12 && pCompressed[0] == 0x1f && pCompressed[1] == 0x8b) {
        //Serial.println("It's a gzip file!");
        iUncompressedSize = lib_inflate_gzip_size(pCompressed, iCompressedSize); // allow a little extra for reading past the end
        pUncompressed = (uint8_t *)malloc(iUncompressedSize); // allow a little extra for reading past the end
        if(pUncompressed)
        {
          unsigned long startTime = millis();
          unsigned int decSize = iUncompressedSize;
#if defined(LIB_INFLATE_CRC_ENABLED) || defined(LIB_INFLATE_ERROR_ENABLED)
          lib_inflate_error_code res;
#endif
          int n = 0;
          do { 
#if defined(LIB_INFLATE_CRC_ENABLED) || defined(LIB_INFLATE_ERROR_ENABLED)
            res = 
#endif
            lib_inflate_gzip_uncompress(pUncompressed, &decSize, pCompressed, iCompressedSize);
          } while ((++n < 10)  
#if defined(LIB_INFLATE_CRC_ENABLED) || defined(LIB_INFLATE_ERROR_ENABLED)
            && (res == LIB_INFLATE_SUCCESS)
#endif
          );
          unsigned long deltaTime = millis() - startTime;
          free(pUncompressed);
          Serial.printf(
#if defined(LIB_INFLATE_CRC_ENABLED) || defined(LIB_INFLATE_ERROR_ENABLED)
              "Uncompressed %d times %d=%s compressed %dB uncompressed %dB in %ldms = %ldkB/s\n", n, res,
#ifdef LIB_INFLATE_ERROR_ENABLED
              (res == LIB_INFLATE_DATA_ERROR) ? "DATA_ERR" : 
              (res == LIB_INFLATE_BUF_ERROR) ? "BUF_ERR" :
#endif
#ifdef LIB_INFLATE_CRC_ENABLED 
              (res == LIB_INFLATE_CRC_ERROR) ? "CRC_ERR" : 
#endif
              (res == LIB_INFLATE_SUCCESS) ? "OK" : "?",
#else
              "Uncompressed %d times compressed %dB uncompressed %dB in %ldms = %ldkB/s\n", n, 
#endif
              iCompressedSize, decSize, deltaTime, (iCompressedSize * n) / deltaTime);
        }
        else {
          Serial.printf("Not enough memory for %d\n", iUncompressedSize);
        }
      } else {
        Serial.println("It's not a gzip file, something went wrong :(");
      }
      if (url) {
        free(pCompressed);
      }
    } // http connection succeeded
    WiFi.disconnect();
  } // wifi connection
} /* setup() */

void loop()
{
} /* loop() */


