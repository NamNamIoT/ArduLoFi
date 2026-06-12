// ============================================================================
// LoRaOne TDMA Gateway Firmware - Hardened & Optimized Version
// Features: 0% String usage in critical paths, Millis-safe, RAM-optimized (81%)
// ============================================================================

#include <Arduino.h>
#include "config.h"

// ── RADIO CONFIGURATION ─────────────────────────────────────────────────────
#define RX_WINDOW_MS 65534
#define SYNC_SLOT_MS 1000UL          // Fixed 1s for Beacon
static uint32_t SLOT_MS = 1000UL;     // Depends on SF

inline uint32_t getSlotMsFromSF(uint16_t sfVal) {
  if (sfVal == 12) return 5000UL;
  if (sfVal == 10) return 2000UL;
  return 1000UL; // For SF7, SF8, SF9
}

#define SYNC_DISCOVERY_MS 5000UL
#define IDLE_AFTER_CHAIN_MS 1000UL
#define CALC_TOTAL_CYCLE(n)                                                    \
  (SYNC_SLOT_MS + ((uint32_t)(n) * SLOT_MS) + SYNC_DISCOVERY_MS + IDLE_AFTER_CHAIN_MS)
#define MAX_NODES 15
#define SYNC_FIRST_POS 1

uint32_t myFreq = LORA_FREQUENCY;
uint16_t sf = LORA_SF;
uint16_t bw = LORA_BW;
uint16_t cr = LORA_CR;
uint16_t preamble = LORA_PREAMBLE;
uint16_t txPower = LORA_TX_POWER;

static void applySF_GW(uint16_t newSF) {
  sf = newSF;
  SLOT_MS = getSlotMsFromSF(sf);
  api.lora.psf.set(sf);
}

// SF10 Airtime Constants
#define SF10_SYNC_AIRTIME 150UL
#define SF10_DATA_AIRTIME 395UL
#define SF12_SYNC_AIRTIME 600UL
#define SF12_DATA_AIRTIME 1650UL

inline uint32_t getDataAirtime() {
  return (sf == 12) ? SF12_DATA_AIRTIME : SF10_DATA_AIRTIME;
}
inline uint32_t getSyncAirtime() {
  return (sf == 12) ? SF12_SYNC_AIRTIME : SF10_SYNC_AIRTIME;
}

// ── PINS (Gateway-specific) ─────────────────────────────────────────────────
#define ENABLE_VETH PA0
#define LED_RAK_RECV PA8
#define LED_RAK_SEND PA9
#define LED_RAK_RUN PA15
#define LED_SYNC PA1

// ── GLOBAL STATE ────────────────────────────────────────────────────────────
uint8_t syncTotalNodes = 1;
#define GET_SYNC_INTERVAL() (CALC_TOTAL_CYCLE(syncTotalNodes))

enum SyncState {
  SYNC_IDLE = 0,
  SYNC_WAIT_TX_START,
  SYNC_STARTED,
  SYNC_CHAINING,
  SYNC_WAIT_TX_END,
  SYNC_DISCOVERY
};

SyncState syncState = SYNC_IDLE;
uint32_t syncStateStartMs = 0;
uint32_t syncAnchorMs = 0;
uint32_t lastSyncSentMillis = 0;
uint16_t syncCycleCounter = 0;
uint32_t actualCycleIntervalMs = 65000;
uint32_t lastSyncCycleStartMs = 0;

uint32_t rxPacketCount = 0;
uint32_t lastRxAssertMs = 0;
volatile bool rxRearmNeeded = false;

// Buffers
char inputBuffer[512] = "";
uint16_t inputIdx = 0;
bool stringComplete = false;

char usbInputBuffer[256] = "";
uint16_t usbInputIdx = 0;
bool usbStringComplete = false;

// Optimized Downlink Queue (uses DOWNLINK_QUEUE_LEN from config.h)
char downlinkQueue[DOWNLINK_QUEUE_LEN][32];
uint8_t downlinkNodeIds[DOWNLINK_QUEUE_LEN];
uint8_t downlinkHead = 0;
uint8_t downlinkTail = 0;

// Node Tracking
typedef struct {
  uint32_t last_seen;
  int32_t last_drift;
  uint8_t status; // 0=DEAD, 1=ACTIVE, 2=DEGRADED
  uint8_t retry_count;
} ChainNodeInfo;

ChainNodeInfo chain_map[MAX_NODES + 1];
bool nodePrecise[MAX_NODES + 1];
uint32_t lastStatusBroadcastMs = 0;

char seenDiscoveryUIDs[10][17];
uint8_t discoveryCount = 0;

// GW_ACK buffer
char gwAckFrame[128] = "";
uint8_t gwAckNode = 0;
bool gwAckPending = false;
uint32_t gwAckLastSentMs = 0;
uint32_t gwAckLastRxMs = 0;

// Battle Mode
bool battleMode = false;
uint32_t lastCfgBroadcastMs = 0;

// Recovery Queue (uses RECOVERY_QUEUE_LEN from config.h)
char recoveryQueue[RECOVERY_QUEUE_LEN][48];
uint8_t recoveryQueueHead = 0, recoveryQueueTail = 0;

// Urgent CMD (uses URGENT_* from config.h)
bool urgentPending = false;
char urgentFrame[128] = "";
uint8_t urgentSendCount = 0;
uint32_t urgentLastSendMs = 0;

uint32_t lastTankSendMs = 0;

// ── FORWARD DECLARATIONS ────────────────────────────────────────────────────
void rearmGatewayRX(const char *reason);
void recv_cb(rui_lora_p2p_recv_t data);
void send_cb(void);
void processCommandFrame(char *frame);
int calculateSyncGuide(long drift, uint32_t cycleMs);

// ── CHAIN MONITOR ───────────────────────────────────────────────────────────
class ChainMonitor {
public:
  uint8_t health_score = 100;
  uint8_t expected_next = 1;

  void on_packet_received(uint8_t node_id) {
    uint32_t now = millis();
    if (node_id < 1 || node_id > MAX_NODES)
      return;

    uint32_t anchor = (syncAnchorMs > 0) ? syncAnchorMs : syncStateStartMs;
    uint32_t ideal_ts = anchor + (uint32_t)(node_id) * SLOT_MS + 200 - getSyncAirtime();
    int32_t drift = (int32_t)(now - getDataAirtime() - ideal_ts);

    if (chain_map[node_id].last_seen > 0) {
      uint32_t actual_interval = now - chain_map[node_id].last_seen;
      int32_t interval_diff =
          (int32_t)actual_interval - (int32_t)actualCycleIntervalMs;
      nodePrecise[node_id] = (abs(interval_diff) < 2000 || abs(drift) < 1500);
    }

    chain_map[node_id].last_seen = now;
    chain_map[node_id].last_drift = drift;
    chain_map[node_id].status = 1;
    chain_map[node_id].retry_count = 0;

    if (syncState == SYNC_CHAINING) {
      if (node_id != expected_next) {
        handle_chain_break(expected_next, node_id);
      }
      expected_next = (node_id % syncTotalNodes) + 1;
    }
  }

  void handle_chain_break(uint8_t expected_id, uint8_t received_id) {
    if (expected_id >= 1 && expected_id <= MAX_NODES) {
      chain_map[expected_id].status = 2;
      chain_map[expected_id].retry_count++;
      char buf[64];
      snprintf(buf, sizeof(buf), "*RECOVER,1,%u,%u,0,0,0,0#", expected_id,
               expected_id);
      enqueueRecovery(buf);
    }
  }

  uint8_t calculate_health() {
    int score = 100;
    for (int i = 1; i <= syncTotalNodes; i++) {
      if (chain_map[i].status == 0)
        score -= 5;
      if (chain_map[i].status == 2)
        score -= 2;
    }
    if (score < 0)
      score = 0;
    health_score = (uint8_t)score;
    return health_score;
  }

  void on_cfg_ack(uint8_t node_id, uint8_t confirmed_total) {
    char fwd[48];
    snprintf(fwd, sizeof(fwd), "*CFG_ACK,%u,%u#", node_id, confirmed_total);
    Serial1.println(fwd);
  }

  void broadcast_health() {
    uint8_t score = calculate_health();
    char buf[48];
    snprintf(buf, sizeof(buf), "*CHAIN_CHECK_RESULT,%u,OK#", score);
    api.lora.precv(0);
    delay(50);
    api.lora.psend(strlen(buf), (uint8_t *)buf);
    Serial1.println(buf);
  }

  void enqueueRecovery(const char *s) {
    uint8_t next = (recoveryQueueTail + 1) % RECOVERY_QUEUE_LEN;
    if (next == recoveryQueueHead)
      recoveryQueueHead = (recoveryQueueHead + 1) % RECOVERY_QUEUE_LEN;
    strncpy(recoveryQueue[recoveryQueueTail], s, 47);
    recoveryQueue[recoveryQueueTail][47] = '\0';
    recoveryQueueTail = next;
  }

  bool dequeueRecovery(char *out, size_t maxLen) {
    if (recoveryQueueHead == recoveryQueueTail)
      return false;
    strncpy(out, recoveryQueue[recoveryQueueHead], maxLen - 1);
    out[maxLen - 1] = '\0';
    recoveryQueueHead = (recoveryQueueHead + 1) % RECOVERY_QUEUE_LEN;
    return true;
  }
} chainMonitor;

// ── SETUP ───────────────────────────────────────────────────────────────────
void setup() {
  Wire.begin();
  analogReadResolution(12);
  Serial.begin(115200, RAK_CUSTOM_MODE);
  Serial1.begin(115200, SERIAL_8N1);
  // 1. Read persisted config from Flash FIRST
  uint8_t storedNodes = 0;
  bool hasStored = false;
  if (api.system.flash.get(FLASH_OFFSET_TOTAL_NODES, &storedNodes, 1) && storedNodes >= 1 && storedNodes <= MAX_NODES) {
    syncTotalNodes = storedNodes;
    hasStored = true;
  }
  uint16_t storedSF = 0;
  if (api.system.flash.get(4, (uint8_t *)&storedSF, 2) && (storedSF == 7 || storedSF == 10 || storedSF == 12)) {
    sf = storedSF;
    SLOT_MS = getSlotMsFromSF(sf);
  }

  // 2. Now print the boot banner with correct values
  Serial.println("\r\n====================================");
  Serial.println("   LoRaOne TDMA Gateway Booting...");
  Serial.println("   Mode: Hardened & Optimized");
  Serial.println("====================================");
  Serial.printf("Radio Config: %.3fMHz, SF%d, BW125, CR4/5, PWR%ddBm\r\n",
                myFreq / 1e6, sf, txPower);
  Serial.println("------------------------------------");
  Serial.printf("Sync Config: Total Nodes=%u (%s), Slot=%lums, Discovery=%lums\r\n",
                syncTotalNodes, (hasStored ? "Memory" : "Default"), SLOT_MS, SYNC_DISCOVERY_MS);
  Serial.println("------------------------------------");
  Serial.println("Initializing hardware and radio...");
  Serial.println("====================================\r\n");

  pinMode(ENABLE_VETH, OUTPUT);
  pinMode(LED_RAK_RECV, OUTPUT);
  pinMode(LED_RAK_SEND, OUTPUT);
  pinMode(LED_RAK_RUN, OUTPUT);
  pinMode(LED_SYNC, OUTPUT);
  digitalWrite(ENABLE_VETH, LOW);

  // CRITICAL: Ensure P2P mode. nwm.set(); reboots if currently in LoRaWAN mode.
  if (api.lora.nwm.get() != 0) {
    Serial.println("[RADIO] Switching to P2P mode... (will reboot)");
    api.lora.nwm.set();
    // Device reboots here — code below won't execute until next boot
  }

  api.lora.pfreq.set(myFreq);
  api.lora.psf.set(sf);
  api.lora.pbw.set(bw);
  api.lora.pcr.set(cr);
  api.lora.ppl.set(preamble);
  api.lora.ptp.set(txPower);

  // Tắt mã hóa LoRa P2P để đảm bảo không bị xung đột khóa cũ lưu trong Flash
  api.lora.encry.set(0);

  api.lora.registerPRecvCallback(recv_cb);
  api.lora.registerPSendCallback(send_cb);

  rearmGatewayRX("init");

  // Ép Gateway phát gói tin đồng bộ SYNC_START đầu tiên NGAY LẬP TỨC khi boot, không chờ chu kỳ đầu
  lastSyncSentMillis = millis() - GET_SYNC_INTERVAL();

  char radioReport[128];
  snprintf(radioReport, sizeof(radioReport),
           "*GW_RADIO,%.3fMHz,SF%u,BW125,CR4/5,PWR%udBm,RX_OK#", myFreq / 1e6,
           sf, txPower);
  Serial1.println(radioReport);
}

// ── LOOP ────────────────────────────────────────────────────────────────────
void loop() {
  uint32_t now = millis();

  // 1. Serial Handlers
  while (Serial1.available()) {
    char c = (char)Serial1.read();
    if (inputIdx < 511) {
      inputBuffer[inputIdx++] = c;
      inputBuffer[inputIdx] = '\0';
    }
    if (c == '\n')
      stringComplete = true;
  }
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (usbInputIdx < 255) {
      usbInputBuffer[usbInputIdx++] = c;
      usbInputBuffer[usbInputIdx] = '\0';
    }
    if (c == '\n' || c == '\r')
      usbStringComplete = true;
  }

  if (usbStringComplete) {
    strncpy(inputBuffer, usbInputBuffer, sizeof(inputBuffer) - 1);
    stringComplete = true;
    usbStringComplete = false;
    usbInputIdx = 0;
  }

  if (stringComplete) {
    char *start = inputBuffer;
    char *hash = strchr(start, '#');
    while (hash != NULL) {
      *hash = '\0';
      processCommandFrame(start);
      start = hash + 1;
      hash = strchr(start, '#');
    }
    inputIdx = 0;
    inputBuffer[0] = '\0';
    stringComplete = false;
  }

  // 2. ACK — HIGHEST PRIORITY
  if (gwAckPending) {
    api.lora.precv(0);
    delay(50);
    if (api.lora.psend(strlen(gwAckFrame), (uint8_t *)gwAckFrame)) {
      gwAckPending = false;
      gwAckLastSentMs = millis();
      rxRearmNeeded = false;
      Serial.printf("[GW_ACK] Sent to Node %u (%lums after RX)\r\n", gwAckNode,
                    (unsigned long)(gwAckLastSentMs - gwAckLastRxMs));
    } else {
      Serial.println("[GW_ACK] Radio busy, retrying...");
    }
  }

  // 3. Periodic Status Broadcast
  if ((int32_t)(now - lastStatusBroadcastMs) >= (int32_t)GW_STATUS_INTERVAL_MS) {
    lastStatusBroadcastMs = now;
    const char *stateNames[] = {"IDLE", "WAIT_TX", "STARTED", "CHAIN", "WAIT_TX_END", "DISC"};
    uint32_t nextSync = ((int32_t)(now - lastSyncSentMillis) < (int32_t)GET_SYNC_INTERVAL())
                            ? (GET_SYNC_INTERVAL() - (now - lastSyncSentMillis)) / 1000
                            : 0;
    char status[200];
    snprintf(status, sizeof(status),
             "[GW_STATUS] uptime=%lus state=%s next=%lus health=%d battle=%d nodes=%u#",
             now / 1000, stateNames[syncState], nextSync,
             chainMonitor.health_score, battleMode ? 1 : 0, syncTotalNodes);
    Serial1.println(status);
    Serial.println(status);
    digitalWrite(LED_RAK_RUN, !digitalRead(LED_RAK_RUN));
  }

  // 4. Tank Level Simulation
  if ((int32_t)(now - lastTankSendMs) >= (int32_t)GW_TANK_SEND_INTERVAL) {
    lastTankSendMs = now;
    int raw = analogRead(PB3);
    int mv = (int)((raw / 4095.0f) * 3300.0f);
    char tank[64];
    snprintf(tank, sizeof(tank), "*RAK,T,09,%d#", mv);
    Serial1.println(tank);
  }

  // 5. Sync State Machine
  if (!battleMode && syncState == SYNC_IDLE) {
    if ((int32_t)(now - lastSyncSentMillis) >= (int32_t)GET_SYNC_INTERVAL()) {
      syncCycleCounter++;
      char start[48];
      snprintf(start, sizeof(start), "*SYNC_START,%u,%u,%u#", syncTotalNodes, syncCycleCounter, sf);
      api.lora.precv(0);
      delay(50);
      if (api.lora.psend(strlen(start), (uint8_t *)start)) {
        if (lastSyncCycleStartMs > 0) actualCycleIntervalMs = now - lastSyncCycleStartMs;
        lastSyncCycleStartMs = now;
        lastSyncSentMillis = now;
        syncState = SYNC_WAIT_TX_START;
        syncStateStartMs = now;
        syncAnchorMs = now;
        chainMonitor.expected_next = 1;
        Serial1.println(start);
        Serial.println("\r\n====================================");
        Serial.printf("[SYNC] Starting Cycle #%u\r\n", syncCycleCounter);
        Serial.printf("[SYNC] Nodes: %u | Interval: %lu ms\r\n", syncTotalNodes, GET_SYNC_INTERVAL());
        Serial.println("====================================");
      }
    }
  } 
  else if (syncState == SYNC_CHAINING) {
    uint32_t totalSlotsTime = (uint32_t)syncTotalNodes * SLOT_MS;
    uint32_t chainingWindow = SYNC_SLOT_MS + totalSlotsTime; 
    if ((int32_t)(now - syncAnchorMs) >= (int32_t)chainingWindow) {
      char end[48];
      snprintf(end, sizeof(end), "*SYNC_END,%u,%u#", syncTotalNodes, syncCycleCounter);
      api.lora.precv(0);
      delay(50);
      if (api.lora.psend(strlen(end), (uint8_t *)end)) {
        syncState = SYNC_WAIT_TX_END;
        syncStateStartMs = now;
        Serial1.println(end);
        Serial.println("[SYNC] Chaining finished. Discovery window active.");
      }
    }
  } 
  else if (syncState == SYNC_DISCOVERY) {
    static uint32_t lastHealthMs = 0;
    if ((int32_t)(now - lastHealthMs) >= 2000) {
      chainMonitor.broadcast_health();
      lastHealthMs = now;
    }

    char rec[64];
    if (chainMonitor.dequeueRecovery(rec, sizeof(rec))) {
      api.lora.precv(0);
      delay(50);
      api.lora.psend(strlen(rec), (uint8_t *)rec);
      Serial1.println(rec);
    }

    if ((int32_t)(now - syncStateStartMs) >= (int32_t)SYNC_DISCOVERY_MS) {
      discoveryCount = 0;
      for (int i = 0; i < 10; i++)
        seenDiscoveryUIDs[i][0] = '\0';
      rearmGatewayRX("end-discovery");
      syncState = SYNC_IDLE;
    }
  }

  // Old ACK block removed — now handled at step 2 (highest priority)

  if (rxRearmNeeded && !gwAckPending) {
    rxRearmNeeded = false;
    rearmGatewayRX("loop-rearm");
  }

  // Periodic RX assert — guarantee radio stays in RX during CHAIN/DISC
  if (!gwAckPending && (syncState == SYNC_CHAINING || syncState == SYNC_DISCOVERY || syncState == SYNC_IDLE)) {
    if ((int32_t)(now - lastRxAssertMs) >= (int32_t)RX_ASSERT_INTERVAL_MS) {
      rearmGatewayRX("periodic-assert");
    }
  }
}

// ── HELPERS ─────────────────────────────────────────────────────────────────
void processCommandFrame(char *frame) {
  while (*frame == ' ' || *frame == '\r' || *frame == '\n')
    frame++;
  if (strlen(frame) == 0)
    return;

  if (strncmp(frame, "SET_TOTAL_NODES=", 16) == 0) {
    uint8_t n = (uint8_t)atoi(frame + 16);
    if (n >= 1 && n <= MAX_NODES) {
      syncTotalNodes = n;
      api.system.flash.set(FLASH_OFFSET_TOTAL_NODES, &syncTotalNodes, 1);
      Serial.printf("[CFG] Nodes=%u\r\n", n);
      Serial.printf("*GW_CFG,NODES,%u#\r\n", n); // Explicit confirmation to Dashboard Serial COM
      
      // Broadcast over the air immediately
      char buf[32];
      snprintf(buf, sizeof(buf), "*CFG_NODES,%u#", n);
      api.lora.precv(0);
      delay(50);
      api.lora.psend(strlen(buf), (uint8_t *)buf);
      Serial1.println(buf); // Inform ESP32/Dashboard
      rearmGatewayRX("cfg-nodes-broadcast");
    }
  } else if (strncmp(frame, "SET_SF=", 7) == 0) {
    uint16_t newSF = (uint16_t)atoi(frame + 7);
    if (newSF == 10 || newSF == 12) {
      applySF_GW(newSF);
      api.system.flash.set(4, (uint8_t *)&newSF, 2);
      // Broadcast to all Nodes
      char cfgFrame[24];
      snprintf(cfgFrame, sizeof(cfgFrame), "*CFG_SF,%u#", newSF);
      api.lora.precv(0);
      delay(50);
      api.lora.psend(strlen(cfgFrame), (uint8_t *)cfgFrame);
      Serial.printf("[CFG] SF=%u SLOT_MS=%lu broadcast to nodes\r\n", newSF, (unsigned long)SLOT_MS);
    }
  } else if (strcmp(frame, "FORCE_SYNC") == 0) {
    lastSyncSentMillis = 0;
  } else if (strcmp(frame, "GET_STATUS") == 0) {
    lastStatusBroadcastMs = 0; // Force immediate status report in loop()
  } else if (strncmp(frame, "*CMD,", 5) == 0) {
    int id;
    char cmd[32];
    if (sscanf(frame, "*CMD,%d,%[^#]", &id, cmd) == 2) {
      if (id == 0) {
        if (strcmp(cmd, "BATTLE_ON") == 0) {
          battleMode = true;
          syncState = SYNC_IDLE;
          lastSyncSentMillis = millis();
        } else if (strncmp(cmd, "SET_TOTAL_NODES,", 16) == 0) {
          uint8_t n = (uint8_t)atoi(cmd + 16);
          if (n >= 1 && n <= MAX_NODES) {
            syncTotalNodes = n;
            api.system.flash.set(FLASH_OFFSET_TOTAL_NODES, &syncTotalNodes, 1);
            Serial.printf("[CFG] Nodes=%u (via CMD)\r\n", n);
            Serial.printf("*GW_CFG,NODES,%u#\r\n", n); // Explicit confirmation to Dashboard Serial COM
            
            // Broadcast over the air immediately
            char buf[32];
            snprintf(buf, sizeof(buf), "*CFG_NODES,%u#", n);
            api.lora.precv(0);
            delay(50);
            api.lora.psend(strlen(buf), (uint8_t *)buf);
            Serial1.println(buf); // Inform ESP32/Dashboard
            rearmGatewayRX("cfg-nodes-cmd-broadcast");
          }
        } else if (strncmp(cmd, "SET_SF,", 7) == 0) {
          uint16_t newSF = (uint16_t)atoi(cmd + 7);
          if (newSF == 10 || newSF == 12) {
            applySF_GW(newSF);
            api.system.flash.set(4, (uint8_t *)&newSF, 2);
            char cfgFrame[24];
            snprintf(cfgFrame, sizeof(cfgFrame), "*CFG_SF,%u#", newSF);
            api.lora.precv(0);
            delay(50);
            api.lora.psend(strlen(cfgFrame), (uint8_t *)cfgFrame);
            Serial.printf("[CFG] SF=%u broadcast (via CMD)\r\n", newSF);
          }
        } else if (strncmp(cmd, "SET_PREWAKE,", 12) == 0) {
          uint32_t pw = (uint32_t)atoi(cmd + 12);
          if (pw >= 300 && pw <= 60000) {
            char cfgFrame[48];
            snprintf(cfgFrame, sizeof(cfgFrame), "*CFG_PREWAKE,%lu#", (unsigned long)pw);
            api.lora.precv(0);
            delay(50);
            api.lora.psend(strlen(cfgFrame), (uint8_t *)cfgFrame);
            Serial.printf("[CFG] PreWake=%lu broadcast to nodes\r\n", (unsigned long)pw);
          }
        } else if (strcmp(cmd, "BATTLE_OFF") == 0) {
          battleMode = false;
          lastSyncSentMillis = millis() - GET_SYNC_INTERVAL();
        }
      } else if (id >= 1 && id <= MAX_NODES) {
        uint8_t nextHead = (downlinkHead + 1) % DOWNLINK_QUEUE_LEN;
        if (nextHead != downlinkTail) {
          downlinkNodeIds[downlinkHead] = (uint8_t)id;
          strncpy(downlinkQueue[downlinkHead], cmd, 31);
          downlinkQueue[downlinkHead][31] = '\0';
          downlinkHead = nextHead;
        } else {
          Serial.println("[WARN] Downlink queue full, dropping command");
        }
      }
    }
  }
}

void rearmGatewayRX(const char *reason) {
  api.lora.precv(0); // Stop first to be safe
  delay(50);         // Wait for radio to stabilize (TX -> RX)
  bool ok = api.lora.precv(RX_WINDOW_MS);
  if (!ok) {
    Serial.printf("[RADIO] precv(%u) FAILED (%s)! Retrying...\r\n", RX_WINDOW_MS, reason);
    delay(200);
    ok = api.lora.precv(RX_WINDOW_MS);
    Serial.printf("[RADIO] precv retry: %s\r\n", ok ? "OK" : "FAILED AGAIN");
  } else {
    Serial.printf("[RADIO] RX armed (%s)\r\n", reason);
  }

  lastRxAssertMs = millis();
  rxRearmNeeded = false;
}

int calculateSyncGuide(long drift, uint32_t cycleMs) {
  // Normalize drift to [-cycle/2, cycle/2] range for circular drift handling
  long normalizedDrift = drift;
  if (cycleMs > 0) {
    long halfCycle = (long)(cycleMs / 2);
    while (normalizedDrift > halfCycle) normalizedDrift -= (long)cycleMs;
    while (normalizedDrift < -halfCycle) normalizedDrift += (long)cycleMs;
  }

  // Convert normalizedDrift to Index 1-15 (8 = PERFECT)
  if (normalizedDrift > 20000) return 15;
  if (normalizedDrift < -20000) return 1;
  if (normalizedDrift > 10000) return 14;
  if (normalizedDrift < -10000) return 2;
  if (normalizedDrift > 5000)  return 13;
  if (normalizedDrift < -5000)  return 3;
  if (normalizedDrift > 2500)  return 12;
  if (normalizedDrift < -2500)  return 4;
  if (normalizedDrift > 1000)  return 11;
  if (normalizedDrift < -1000)  return 5;
  if (normalizedDrift > 500)   return 10;
  if (normalizedDrift < -500)   return 6;
  if (normalizedDrift > 100)   return 9;
  if (normalizedDrift < -100)  return 7;
  
  return 8; // PERFECT
}

// ── CALLBACKS ───────────────────────────────────────────────────────────────
void recv_cb(rui_lora_p2p_recv_t data) {
  digitalWrite(LED_RAK_RECV, HIGH);
  char lora[256];
  int len = (data.BufferSize > 255) ? 255 : data.BufferSize;
  memcpy(lora, data.Buffer, len);
  lora[len] = '\0';

  // Forward raw LoRa frame directly to ESP32 Serial
  Serial1.println(lora);

  if (lora[0] == '*') {
    char *p = lora + 1;
    char *end = strchr(p, '#');
    if (end)
      *end = '\0';
    char *saveptr;
    char *tag = strtok_r(p, ",", &saveptr);
    if (tag) {
      if (strcmp(tag, "JOIN") == 0) {
        char *uid = strtok_r(NULL, ",", &saveptr);
        if (uid) {
          bool seen = false;
          for (int i = 0; i < 10; i++)
            if (strcmp(seenDiscoveryUIDs[i], uid) == 0) {
              seen = true;
              break;
            }
          if (!seen) {
            strncpy(seenDiscoveryUIDs[discoveryCount % 10], uid, 16);
            discoveryCount++;
          }
          char ack[64];
          snprintf(ack, sizeof(ack), "*JOIN_ACK,%s,%u#", uid, syncCycleCounter);
          api.lora.precv(0);
          delay(50);
          api.lora.psend(strlen(ack), (uint8_t *)ack);
        }
      } else if (strcmp(tag, "DATA") == 0) {
        char *idStr = strtok_r(NULL, ",", &saveptr);
        char *typeStr = strtok_r(NULL, ",", &saveptr);
        char *s1Str = strtok_r(NULL, ",", &saveptr);
        char *s2Str = strtok_r(NULL, ",", &saveptr);
        char *vbatStr = strtok_r(NULL, ",", &saveptr);
        char *seqStr = strtok_r(NULL, ",", &saveptr);
        char *starStr = strtok_r(NULL, ",", &saveptr);
        char *driftStr = strtok_r(NULL, ",", &saveptr);
        char *cfgStr = strtok_r(NULL, ",", &saveptr);
        char *sfStr = strtok_r(NULL, ",", &saveptr);
        if (idStr && seqStr) {
          uint8_t id = atoi(idStr);
          uint8_t reportedType = typeStr ? atoi(typeStr) : 1;
          uint8_t reportedCfg = cfgStr ? atoi(cfgStr) : 0;
          uint16_t reportedSF = sfStr ? atoi(sfStr) : 0;
          chainMonitor.on_packet_received(id);
          uint32_t currentCycleMs = CALC_TOTAL_CYCLE(syncTotalNodes);
          int perf = calculateSyncGuide((long)chain_map[id].last_drift, currentCycleMs);

          char cmdPart[64] = "";
          while (downlinkTail != downlinkHead) {
            if (downlinkNodeIds[downlinkTail] == id && downlinkQueue[downlinkTail][0] != '\0') {
              snprintf(cmdPart, sizeof(cmdPart), ",CMD=%s", downlinkQueue[downlinkTail]);
              downlinkQueue[downlinkTail][0] = '\0';
              downlinkNodeIds[downlinkTail] = 0;
              downlinkTail = (downlinkTail + 1) % DOWNLINK_QUEUE_LEN;
              break;
            }
            downlinkTail = (downlinkTail + 1) % DOWNLINK_QUEUE_LEN;
          }
          int isOk = (perf >= 7 && perf <= 9) ? 1 : 0;
          snprintf(gwAckFrame, sizeof(gwAckFrame),
                   "*GW_ACK,%u,OK=%d,ADV=%d,DFT=%ld,NODES=%u,SF=%u%s#", id, isOk, perf,
                   (long)chain_map[id].last_drift,
                   syncTotalNodes, sf, cmdPart);
          gwAckNode = id;
          gwAckPending = true;
          gwAckLastRxMs = millis();

          // --- ENHANCED DEBUG LOG ON USB SERIAL ---
          Serial.printf("\r\n[RECV] Node %u <<<<< \r\n", id);
          Serial.printf("  - Signal: RSSI=%d, SNR=%d\r\n", data.Rssi, data.Snr);
          Serial.printf("  - Timing: Drift=%+ld ms\r\n", (long)chain_map[id].last_drift);
          Serial.printf("  - Config: Active Nodes=%u, SF=%u\r\n", reportedCfg, reportedSF);
          Serial.printf("  - Advice: ADV=%d (%s)\r\n", perf, (perf < 8 ? "EARLY -> Wake LATER" : (perf > 8 ? "LATE -> Wake EARLIER" : "PERFECT ⭐")));
          Serial.println("-------------------------");

          char fwd[200];
          // Forward structured node data frame to ESP32 for MQTT publishing
          // Format: *NODE_DATA,ID,Type,S1,S2,Vbat,Seq,Star,Drift,Nodes,SF,RSSI,SNR,ADV#
          snprintf(fwd, sizeof(fwd),
              "*NODE_DATA,%u,%u,%s,%s,%s,%s,%s,%ld,%u,%u,%d,%d,%d#",
              id, reportedType,
              s1Str ? s1Str : "0", s2Str ? s2Str : "0",
              vbatStr ? vbatStr : "0", seqStr ? seqStr : "0",
              starStr ? starStr : "0",
              (long)chain_map[id].last_drift, syncTotalNodes, sf,
              data.Rssi, data.Snr, perf);
          Serial1.println(fwd);

          // Also forward advisory for backward compat
          char fwd_adv[64];
          snprintf(fwd_adv, sizeof(fwd_adv), "*RAK,%u,0F,%d#", id, perf);
          Serial1.println(fwd_adv);

          snprintf(
              fwd, sizeof(fwd),
              "[GW_RX] Node %u: Type=%u V1=%s V2=%s RSSI=%d SNR=%d Batt=%sV Seq=%s Perf=%d Drift=%ldms CFG=%u#", id, reportedType,
              s1Str ? s1Str : "0", s2Str ? s2Str : "0",
              data.Rssi, data.Snr, vbatStr ? vbatStr : "--", seqStr, perf,
              (long)chain_map[id].last_drift, reportedCfg);
          Serial1.println(fwd);

          // Debug log on USB serial
          Serial.printf("  -> Forwarded to ESP32: NODE_DATA id=%u s1=%s s2=%s\r\n",
              id, s1Str ? s1Str : "0", s2Str ? s2Str : "0");
        }
      } else if (strcmp(tag, "CFG_ACK") == 0) {
        char *idStr = strtok_r(NULL, ",", &saveptr);
        char *totalStr = strtok_r(NULL, ",", &saveptr);
        if (idStr && totalStr)
          chainMonitor.on_cfg_ack(atoi(idStr), atoi(totalStr));
      }
    }
  }
  rxRearmNeeded = true;
  digitalWrite(LED_RAK_RECV, LOW);
}

void send_cb(void) {
  digitalWrite(LED_RAK_SEND, HIGH);
  if (syncState == SYNC_WAIT_TX_START) {
    syncState = SYNC_CHAINING;
    syncStateStartMs = millis();
    syncAnchorMs = syncStateStartMs;
  } else if (syncState == SYNC_WAIT_TX_END) {
    syncState = SYNC_DISCOVERY;
    syncStateStartMs = millis();
  }
  rxRearmNeeded = true;
  digitalWrite(LED_RAK_SEND, LOW);
}
