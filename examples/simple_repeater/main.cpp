#include <Arduino.h>   // needed for PlatformIO
#include <Mesh.h>
#include "MyMesh.h"

#ifdef DISPLAY_CLASS
  #include "UITask.h"
  static UITask ui_task(display);
#endif

StdRNG fast_rng;
SimpleMeshTables tables;

MyMesh the_mesh(board, radio_driver, *new ArduinoMillis(), fast_rng, rtc_clock, tables);

void halt() {
  while (1) ;
}

static char command[160];

// Blocked repeater Public Key (binary format)
static const uint8_t BLOCKED_REPEATER_KEY[PUB_KEY_SIZE] = {
  0x02, 0x19, 0x50, 0x8f, 0x6b, 0x2d, 0x0f, 0x51,
  0x26, 0x1d, 0x41, 0x51, 0x87, 0x8f, 0xca, 0x72,
  0x9f, 0x6a, 0x85, 0xdd, 0x50, 0xc2, 0x6f, 0x31,
  0xfe, 0xba, 0x37, 0x93, 0x4b, 0xaa, 0x9a, 0xf0
};

void setup() {
  Serial.begin(115200);
  delay(1000);

  board.begin();

#ifdef DISPLAY_CLASS
  if (display.begin()) {
    display.startFrame();
    display.setCursor(0, 0);
    display.print("Please wait...");
    display.endFrame();
  }
#endif

  if (!radio_init()) {
    halt();
  }

  fast_rng.begin(radio_get_rng_seed());

  FILESYSTEM* fs;
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  InternalFS.begin();
  fs = &InternalFS;
  IdentityStore store(InternalFS, "");
#elif defined(ESP32)
  SPIFFS.begin(true);
  fs = &SPIFFS;
  IdentityStore store(SPIFFS, "/identity");
#elif defined(RP2040_PLATFORM)
  LittleFS.begin();
  fs = &LittleFS;
  IdentityStore store(LittleFS, "/identity");
  store.begin();
#else
  #error "need to define filesystem"
#endif

  if (!store.load("_main", the_mesh.self_id)) {
    MESH_DEBUG_PRINTLN("Generating new keypair");
    the_mesh.self_id = radio_new_identity();
    int count = 0;
    while (count < 10 && (the_mesh.self_id.pub_key[0] == 0x00 || the_mesh.self_id.pub_key[0] == 0xFF)) {
      the_mesh.self_id = radio_new_identity(); count++;
    }
    store.save("_main", the_mesh.self_id);
  }

  Serial.print("Repeater ID: ");
  mesh::Utils::printHex(Serial, the_mesh.self_id.pub_key, PUB_KEY_SIZE); Serial.println();

  command[0] = 0;

  sensors.begin();
  the_mesh.begin(fs);

#ifdef DISPLAY_CLASS
  ui_task.begin(the_mesh.getNodePrefs(), FIRMWARE_BUILD_DATE, FIRMWARE_VERSION);
#endif

  // 🛑 Packet-level hook to block messages with specific repeater in path
  the_mesh.addPacketHandler([](mesh::Packet* pkt) -> mesh::DispatcherAction {
    if (!pkt || pkt->path_len == 0 || pkt->path_len > 64) return mesh::DISPATCHER_CONTINUE;

    size_t hop_count = pkt->path_len / PUB_KEY_SIZE;
    for (size_t i = 0; i < hop_count; ++i) {
      const uint8_t* hop = pkt->path + i * PUB_KEY_SIZE;
      if (memcmp(hop, BLOCKED_REPEATER_KEY, PUB_KEY_SIZE) == 0) {
        Serial.println("Blocked packet: repeater found in path.");
        return mesh::DISPATCHER_DROP;
      }
    }

    return mesh::DISPATCHER_CONTINUE;
  });

  the_mesh.sendSelfAdvertisement(16000);
}

void loop() {
  int len = strlen(command);
  while (Serial.available() && len < sizeof(command)-1) {
    char c = Serial.read();
    if (c != '\n') {
      command[len++] = c;
      command[len] = 0;
    }
    Serial.print(c);
  }
  if (len == sizeof(command)-1) {
    command[sizeof(command)-1] = '\r';
  }

  if (len > 0 && command[len - 1] == '\r') {
    command[len - 1] = 0;
    char reply[160];
    the_mesh.handleCommand(0, command, reply);
    if (reply[0]) {
      Serial.print("  -> "); Serial.println(reply);
    }
    command[0] = 0;
  }

  the_mesh.loop();
  sensors.loop();
#ifdef DISPLAY_CLASS
  ui_task.loop();
#endif
}
