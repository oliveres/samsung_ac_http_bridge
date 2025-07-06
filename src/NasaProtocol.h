#pragma once

#include <Arduino.h>
#include <vector>

// Enums from the original ESP Home component
enum class AddressClass : uint8_t {
    Outdoor = 0x10,
    HTU = 0x11,
    Indoor = 0x20,
    ERV = 0x30,
    Diffuser = 0x35,
    MCU = 0x38,
    RMC = 0x40,
    WiredRemote = 0x50,
    PIM = 0x58,
    SIM = 0x59,
    Peak = 0x5A,
    PowerDivider = 0x5B,
    OnOffController = 0x60,
    WiFiKit = 0x62,
    CentralController = 0x65,
    DMS = 0x6A,
    JIGTester = 0x80,
    BroadcastSelfLayer = 0xB0,
    BroadcastControlLayer = 0xB1,
    BroadcastSetLayer = 0xB2,
    BroadcastControlAndSetLayer = 0xB3,
    BroadcastModuleLayer = 0xB4,
    BroadcastCSM = 0xB7,
    BroadcastLocalLayer = 0xB8,
    BroadcastCSML = 0xBF,
    Undefined = 0xFF,
};

enum class PacketType : uint8_t {
    StandBy = 0,
    Normal = 1,
    Gathering = 2,
    Install = 3,
    Download = 4
};

enum class DataType : uint8_t {
    Undefined = 0,
    Read = 1,
    Write = 2,
    Request = 3,
    Notification = 4,
    Response = 5,
    Ack = 6,
    Nack = 7
};

enum class MessageSetType : uint8_t {
    Enum = 0,
    Variable = 1,
    LongVariable = 2,
    Structure = 3
};

enum class MessageNumber : uint16_t {
    Undefined = 0,
    ENUM_in_operation_power = 0x4000,
    ENUM_in_operation_automatic_cleaning = 0x4111,
    ENUM_in_water_heater_power = 0x4065,
    ENUM_in_operation_mode = 0x4001,
    ENUM_in_water_heater_mode = 0x4066,
    ENUM_in_fan_mode = 0x4006,
    ENUM_in_fan_mode_real = 0x4007,
    ENUM_in_alt_mode = 0x4060,
    ENUM_in_louver_hl_swing = 0x4011,
    ENUM_in_louver_lr_swing = 0x407e,
    ENUM_in_state_humidity_percent = 0x4038,
    VAR_in_temp_room_f = 0x4203,
    VAR_in_temp_target_f = 0x4201,
    VAR_in_temp_water_outlet_target_f = 0x4247,
    VAR_in_temp_water_tank_f = 0x4237,
    VAR_out_sensor_airout = 0x8204,
    VAR_in_temp_water_heater_target_f = 0x4235,
    VAR_in_temp_eva_in_f = 0x4205,
    VAR_in_temp_eva_out_f = 0x4206,
    VAR_out_error_code = 0x8235,
    LVAR_OUT_CONTROL_WATTMETER_1W_1MIN_SUM = 0x8413,
    LVAR_OUT_CONTROL_WATTMETER_ALL_UNIT_ACCUM = 0x8414,
    VAR_OUT_SENSOR_CT1 = 0x8217,
    LVAR_NM_OUT_SENSOR_VOLTAGE = 0x24fc,
    
    // Additional messages from ESPHome implementation
    VAR_IN_FSV_3021 = 0x4260,                      // FSV sensor 1 (division by 10)
    VAR_IN_FSV_3022 = 0x4261,                      // FSV sensor 2 (division by 10)
    VAR_IN_FSV_3023 = 0x4262,                      // FSV sensor 3 (division by 10)
    NASA_OUTDOOR_CONTROL_WATTMETER_1UNIT = 0x8411, // Single unit wattmeter
    TOTAL_PRODUCED_ENERGY = 0x8427,                // Total produced energy
    ACTUAL_PRODUCED_ENERGY = 0x8426,               // Actual produced energy
    NASA_OUTDOOR_CONTROL_WATTMETER_TOTAL_SUM = 0x8415,       // Total wattmeter sum
    NASA_OUTDOOR_CONTROL_WATTMETER_TOTAL_SUM_ACCUM = 0x8416, // Total wattmeter accumulator
    
    // Ventilation and advanced indoor unit messages
    ENUM_IN_OPERATION_VENT_POWER = 0x4003,         // Ventilation power on/off
    ENUM_IN_OPERATION_VENT_MODE = 0x4004,          // Ventilation mode
    ENUM_in_louver_hl_part_swing = 0x4012,         // Partial swing mode
    ENUM_IN_QUIET_MODE = 0x406E,                   // Quiet mode
    ENUM_IN_OPERATION_POWER_ZONE1 = 0x4119,       // Zone 1 power
    ENUM_IN_OPERATION_POWER_ZONE2 = 0x411E,       // Zone 2 power
    ENUM_in_operation_mode_real = 0x4002,          // Real operation mode
    ENUM_in_fan_vent_mode = 0x4008,                // Fan ventilation mode
    VAR_in_capacity_request = 0x4211,              // Capacity request (kW, division by 8.6)
    
    // Outdoor unit pipe sensors
    VAR_OUT_SENSOR_PIPEIN3 = 0x8261,               // Pipe in sensor 3 (Celsius, division by 10)
    VAR_OUT_SENSOR_PIPEIN4 = 0x8262,               // Pipe in sensor 4 (Celsius, division by 10)
    VAR_OUT_SENSOR_PIPEIN5 = 0x8263,               // Pipe in sensor 5 (Celsius, division by 10)
    VAR_OUT_SENSOR_PIPEOUT1 = 0x8264,              // Pipe out sensor 1 (Celsius, division by 10)
    VAR_OUT_SENSOR_PIPEOUT2 = 0x8265,              // Pipe out sensor 2 (Celsius, division by 10)
    VAR_OUT_SENSOR_PIPEOUT3 = 0x8266,              // Pipe out sensor 3 (Celsius, division by 10)
    VAR_OUT_SENSOR_PIPEOUT4 = 0x8267,              // Pipe out sensor 4 (Celsius, division by 10)
    VAR_OUT_SENSOR_PIPEOUT5 = 0x8268,              // Pipe out sensor 5 (Celsius, division by 10)
    VAR_out_control_order_cfreq_comp2 = 0x8274,    // Compressor 2 frequency order
    VAR_out_control_target_cfreq_comp2 = 0x8275,   // Compressor 2 frequency target
    VAR_OUT_PROJECT_CODE = 0x82bc,                  // Project code
    VAR_OUT_PRODUCT_OPTION_CAPA = 0x82e3,           // Product option capacity
    VAR_out_sensor_top1 = 0x8280,                   // Top sensor 1 (Celsius, division by 10)
    VAR_OUT_PHASE_CURRENT = 0x82db,                 // Phase current
    
    // Air quality sensors
    VAR_IN_DUST_SENSOR_PM10_0_VALUE = 0x42d1,      // PM10.0 dust sensor value
    VAR_IN_DUST_SENSOR_PM2_5_VALUE = 0x42d2,       // PM2.5 dust sensor value
    VAR_IN_DUST_SENSOR_PM1_0_VALUE = 0x42d3,       // PM1.0 dust sensor value
    
    // Additional outdoor unit messages from NASA wiki
    ENUM_out_operation_odu_mode = 0x8001,  // Outdoor Driving Mode (OP_STOP, OP_SAFETY, OP_NORMAL, etc.)
    ENUM_out_operation_heatcool = 0x8003,  // Heat/Cool operation: ['Undefined', 'Cool', 'Heat', 'CoolMain', 'HeatMain']
    ENUM_out_load_4way = 0x801A,           // 4Way On/Off valve load
};

enum class Mode {
    Unknown = -1,
    Auto = 0,
    Cool = 1,
    Dry = 2,
    Fan = 3,
    Heat = 4,
};

enum class FanMode {
    Unknown = -1,
    Auto = 0,
    Low = 1,
    Mid = 2,
    High = 3,
    Turbo = 4,
    Off = 5
};

enum class Preset {
    None = 0,
    Sleep = 1,       // Sleep mode
    Quiet = 2,       // Quiet mode
    Fast = 3,        // Fast cooling/heating
    Longreach = 6,   // Long reach mode
    Eco = 7,         // Eco mode
    Windfree = 9     // Wind-free mode
};

enum class SwingMode : uint8_t {
    Fix = 0,
    Vertical = 1,
    Horizontal = 2,
    All = 3
};

struct Address {
    AddressClass klass;
    uint8_t channel;
    uint8_t address;
    uint8_t size = 3;
    
    static Address parse(const String& str);
    static Address getMyAddress();
    
    void decode(std::vector<uint8_t>& data, unsigned int index);
    void encode(std::vector<uint8_t>& data);
    String toString();
};

struct Command {
    bool packetInformation = true;
    uint8_t protocolVersion = 2;
    uint8_t retryCount = 0;
    PacketType packetType = PacketType::StandBy;
    DataType dataType = DataType::Undefined;
    uint8_t packetNumber = 0;
    uint8_t size = 3;
    
    void decode(std::vector<uint8_t>& data, unsigned int index);
    void encode(std::vector<uint8_t>& data);
    String toString();
};

struct Buffer {
    uint8_t size;
    uint8_t data[255];
};

struct MessageSet {
    MessageNumber messageNumber = MessageNumber::Undefined;
    MessageSetType type = MessageSetType::Enum;
    union {
        long value;
        Buffer structure;
    };
    uint16_t size = 2;
    
    MessageSet(MessageNumber messageNumber);
    static MessageSet decode(std::vector<uint8_t>& data, unsigned int index, int capacity);
    void encode(std::vector<uint8_t>& data);
    String toString();
};

enum class DecodeResult {
    Ok = 0,
    InvalidStartByte = 1,
    InvalidEndByte = 2,
    SizeDidNotMatch = 3,
    UnexpectedSize = 4,
    CrcError = 5
};

struct Packet {
    Address sa;
    Address da;
    Command command;
    std::vector<MessageSet> messages;
    
    static Packet create(Address da, DataType dataType, MessageNumber messageNumber, int value);
    static Packet createPartial(Address da, DataType dataType);
    
    DecodeResult decode(std::vector<uint8_t>& data);
    std::vector<uint8_t> encode();
    String toString();
};

// Utility functions
uint16_t crc16(std::vector<uint8_t>& data, int startIndex, int length);
String bytesToHex(const std::vector<uint8_t>& data);
int variableToSigned(int value);

// Conversion functions
Mode operationModeToMode(int value);
FanMode fanModeRealToFanMode(int value);
int fanModeToNasaFanMode(FanMode mode);

// Protocol processing functions
DecodeResult tryDecodeNasaPacket(std::vector<uint8_t> data);
void processNasaPacket(class MessageTarget* target);
void processMessageSet(String source, String dest, MessageSet& message, class MessageTarget* target);

class NasaProtocol {
public:
    NasaProtocol() = default;
    
    void publishRequest(class MessageTarget* target, const String& address, struct ProtocolRequest& request, uint8_t sequenceNumber = 0);
    void protocolUpdate(class MessageTarget* target);
};

extern Packet globalPacket;