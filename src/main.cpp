#include <WiFi.h>
#include <HTTPClient.h>
#include <ESP32DMASPISlave.h>

const char *ssid = "Maestro";
const char *password = "Mwbsc@2023";

static const uint32_t BUFFER_SIZE = 8192;
uint8_t *spi_slave_tx_buf;
uint8_t *spi_slave_rx_buf;
const int IRQpin = 12;

String dataToSend = "";

ESP32DMASPI::Slave slave;
HTTPClient http;

void set_buffer()
{
  Serial.println("Set Buffer");
  for (uint32_t i = 0; i < BUFFER_SIZE; i++)
  {
    spi_slave_tx_buf[i] = (0xFF - i) & 0xFF;
  }
  memset(spi_slave_rx_buf, 0, BUFFER_SIZE);
}

void esp32_Data_send()
{
  Serial.println("Schedule queue");
  // Clear the receive buffer
  memset(spi_slave_tx_buf, 0, BUFFER_SIZE);

  dataToSend.getBytes(spi_slave_tx_buf, BUFFER_SIZE);

  slave.queue(spi_slave_rx_buf, spi_slave_tx_buf, BUFFER_SIZE);

  digitalWrite(IRQpin, LOW);
  delay(100);
  digitalWrite(IRQpin, HIGH);
  delay(100);
}

void http_get_fun(String URLstr)
{
  Serial.println("URLstr: ");
  Serial.println(URLstr);
  if ((WiFi.status() == WL_CONNECTED)) // Check the current connection status
  {
    HTTPClient http;
    Serial.println("http Get");
    http.begin(URLstr);        // Specify the URL like http://cbe.themaestro.in/maestro_erp/webservice/getproductslocation/8
    int httpCode = http.GET(); // Make the request

    if (httpCode > 0) // Check for the returning code
    {
      Serial.println("HTTP Success");
      String payload = http.getString();
      Serial.println(httpCode);
      Serial.println(payload);
      if (httpCode == 200)
      {
        dataToSend = payload;
        esp32_Data_send();
      }
      else if (httpCode == 404)
      {
        Serial.println("Not Found Error");
      }
      else if (httpCode == 500)
      {
        Serial.println("Internal Server Error");
      }
    }
    else
    {
      Serial.println("Error on HTTP request");
    }
    http.end(); // Free the resources
  }
}

void setup()
{
  Serial.begin(115200);
  delay(100);
  pinMode(IRQpin, OUTPUT);
  digitalWrite(IRQpin, HIGH);
  // to use DMA buffer, use these methods to allocate buffer
  spi_slave_tx_buf = slave.allocDMABuffer(BUFFER_SIZE);
  spi_slave_rx_buf = slave.allocDMABuffer(BUFFER_SIZE);

  set_buffer();
  delay(5000);

  // slave device configuration
  slave.setDataMode(SPI_MODE0);
  slave.setMaxTransferSize(BUFFER_SIZE);

  // begin() after setting
  // note: the default pins are different depending on the board
  // please refer to README Section "SPI Buses and SPI Pins" for more details
  slave.begin(VSPI); // VSPI

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  Serial.println("Connected to the WiFi network");
}

void loop()
{
  // if there is no transaction in the queue, add a transaction
  if (slave.remained() == 0)
  {
    Serial.println("receive QUEUE");
    slave.queue(spi_slave_rx_buf, spi_slave_tx_buf, BUFFER_SIZE);
  }
  while (slave.available())
  {
    String receivedString = "";
    Serial.println("RECEIVE STRING");
    for (size_t i = 0; i < BUFFER_SIZE; ++i)
    {
      if (spi_slave_rx_buf[i] == '\0')
      {
        break; // Stop if null character is encountered
      }
      receivedString += (char)spi_slave_rx_buf[i];
    }

    // Show received data
    Serial.println("Received: " + receivedString);

    // Clear the receive buffer
    memset(spi_slave_rx_buf, 0, BUFFER_SIZE);

    if (receivedString != "")
    {
      receivedString.trim();
      http_get_fun(receivedString);
    }
    slave.pop();
  }
}