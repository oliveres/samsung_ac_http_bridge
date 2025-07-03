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

class NasaProtocol {
public:
    NasaProtocol() = default;
    
    void publishRequest(class MessageTarget* target, const String& address, struct ProtocolRequest& request);
    void protocolUpdate(class MessageTarget* target);
};

extern Packet globalPacket;