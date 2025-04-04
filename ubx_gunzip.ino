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
// hexdump -v  -e '"  /*%08.8_ax*/ "' -e' 16/1 "0x%02x, "  ' -e '"\n"' Downloads/sample-5.gz
static const U1 gzfile[] = {
  /*00000000*/ 0x1f, 0x8b, 0x08, 0x08, 0x40, 0x48, 0xaf, 0x63, 0x00, 0x03, 0x73, 0x61, 0x6d, 0x70, 0x6c, 0x65,
  /*00000010*/ 0x2d, 0x35, 0x2e, 0x73, 0x76, 0x67, 0x00, 0x55, 0x91, 0x4f, 0x6f, 0xc2, 0x30, 0x0c, 0xc5, 0xef,
  /*00000020*/ 0xfb, 0x14, 0x56, 0xee, 0x71, 0x63, 0x27, 0x75, 0xd3, 0x89, 0x22, 0x6d, 0xbb, 0x96, 0x2b, 0xf7,
  /*00000030*/ 0xb2, 0x31, 0x5a, 0x29, 0xac, 0x13, 0x74, 0x14, 0xf1, 0xe9, 0xe7, 0xc0, 0xfe, 0x4a, 0x39, 0x24,
  /*00000040*/ 0xce, 0xfb, 0x3d, 0xe7, 0x39, 0x8b, 0xe3, 0x69, 0x07, 0xe7, 0x7d, 0x7a, 0x3b, 0x36, 0xa6, 0x9f,
  /*00000050*/ 0xa6, 0xf7, 0xfb, 0xa2, 0x98, 0xe7, 0x19, 0x67, 0x8f, 0xe3, 0x61, 0x57, 0xb0, 0x73, 0xae, 0x50,
  /*00000060*/ 0x85, 0x81, 0x79, 0x78, 0x99, 0xfa, 0xc6, 0x90, 0x18, 0xe8, 0xb7, 0xc3, 0xae, 0x9f, 0x6e, 0xfb,
  /*00000070*/ 0xd7, 0x21, 0xa5, 0xc6, 0x3c, 0x7f, 0x1c, 0x0e, 0xdb, 0xb7, 0xe9, 0x69, 0x4c, 0xe3, 0xc1, 0xc0,
  /*00000080*/ 0x73, 0xea, 0x8e, 0xea, 0xb6, 0x19, 0x60, 0x33, 0xd8, 0xcd, 0x78, 0xb6, 0xc7, 0x6d, 0xb7, 0x37,
  /*00000090*/ 0x70, 0x1a, 0xb6, 0xf3, 0xe3, 0x78, 0x6e, 0x8c, 0x03, 0x07, 0x24, 0xba, 0xcc, 0xf2, 0x0e, 0x60,
  /*000000a0*/ 0xf1, 0xde, 0x4d, 0x3d, 0xbc, 0x34, 0x66, 0x15, 0x91, 0xa2, 0x96, 0x91, 0xc8, 0x77, 0x58, 0x62,
  /*000000b0*/ 0x09, 0x59, 0xe8, 0x2c, 0xfa, 0x8a, 0xc1, 0xb5, 0x84, 0x31, 0x08, 0x78, 0x2c, 0x13, 0x63, 0x70,
  /*000000c0*/ 0x01, 0x6b, 0xa1, 0x96, 0x5c, 0xde, 0x02, 0x27, 0xcb, 0xc8, 0x14, 0x2d, 0xc6, 0x58, 0x5d, 0xf6,
  /*000000d0*/ 0xaa, 0x91, 0xa0, 0x3e, 0x81, 0xa5, 0x55, 0x9b, 0x5a, 0xa0, 0x84, 0x08, 0x65, 0x26, 0x80, 0x02,
  /*000000e0*/ 0x52, 0x19, 0xae, 0x36, 0xf6, 0xea, 0x63, 0x73, 0x39, 0x33, 0x5c, 0x2a, 0x52, 0x59, 0xd1, 0xbe,
  /*000000f0*/ 0x8c, 0x72, 0xaa, 0xb0, 0x66, 0x4e, 0x7a, 0x52, 0x99, 0xac, 0x03, 0x72, 0xb8, 0xac, 0x2a, 0xbd,
  /*00000100*/ 0x53, 0x83, 0x4a, 0x78, 0x2d, 0x18, 0x7d, 0x6c, 0x09, 0xf4, 0xc2, 0xd7, 0x57, 0xb1, 0x4f, 0x5f,
  /*00000110*/ 0x68, 0xd6, 0x85, 0xe0, 0x35, 0x4c, 0xe8, 0x28, 0x13, 0x5f, 0x49, 0xe8, 0x1a, 0x2d, 0x80, 0x4b,
  /*00000120*/ 0x15, 0x12, 0xd7, 0x2a, 0x8d, 0x25, 0x3f, 0xfc, 0x04, 0xa5, 0x3c, 0x13, 0x7d, 0xd7, 0x29, 0xa2,
  /*00000130*/ 0x08, 0x77, 0x7a, 0xbc, 0x95, 0x2d, 0x0a, 0xd7, 0xea, 0x1f, 0x93, 0x55, 0x2e, 0xe6, 0x16, 0xb1,
  /*00000140*/ 0x0a, 0xbf, 0x03, 0xa2, 0xef, 0x01, 0xa1, 0x78, 0x20, 0x8f, 0xae, 0xfe, 0xc7, 0x7a, 0x9b, 0xd9,
  /*00000150*/ 0xb5, 0x1a, 0xff, 0x41, 0x00, 0x3d, 0x69, 0xf2, 0x20, 0xa1, 0xfd, 0x79, 0xeb, 0xc5, 0x14, 0xcb,
  /*00000160*/ 0xbb, 0x45, 0xfe, 0xec, 0xe5, 0x27, 0x25, 0xff, 0x3c, 0x7e, 0x14, 0x02, 0x00, 0x00
};

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
        iCompressedSize = sizeof(gzfile);
        pCompressed = (uint8_t*)gzfile;
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


