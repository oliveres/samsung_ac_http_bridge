#include "NasaProtocol.h"
#include "SamsungACBridge.h"
#include "config.h"
#include <Arduino.h>

Packet globalPacket;
static int packetCounter = 0;

// CRC16 calculation
uint16_t crc16(std::vector<uint8_t>& data, int startIndex, int length) {
    uint16_t crc = 0;
    for (int index = startIndex; index < startIndex + length; ++index) {
        crc = crc ^ ((uint16_t)((uint8_t)data[index]) << 8);
        for (uint8_t i = 0; i < 8; i++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

String bytesToHex(const std::vector<uint8_t>& data) {
    String result = "";
    for (size_t i = 0; i < data.size(); i++) {
        if (data[i] < 16) result += "0";
        result += String(data[i], HEX);
        if (i < data.size() - 1) result += " ";
    }
    result.toUpperCase();
    return result;
}

int variableToSigned(int value) {
    if (value < 65535) return value;
    return value - 65535 - 1;
}

// Address implementation
Address Address::parse(const String& str) {
    Address address;
    int dotIndex1 = str.indexOf('.');
    int dotIndex2 = str.indexOf('.', dotIndex1 + 1);
    
    String klassStr = str.substring(0, dotIndex1);
    String channelStr = str.substring(dotIndex1 + 1, dotIndex2);
    String addressStr = str.substring(dotIndex2 + 1);
    
    address.klass = (AddressClass)strtol(klassStr.c_str(), nullptr, 16);
    address.channel = strtol(channelStr.c_str(), nullptr, 16);
    address.address = strtol(addressStr.c_str(), nullptr, 16);
    
    return address;
}

Address Address::getMyAddress() {
    Address address;
    address.klass = AddressClass::JIGTester;
    address.channel = 0xFF;
    address.address = 0;
    return address;
}

void Address::decode(std::vector<uint8_t>& data, unsigned int index) {
    klass = (AddressClass)data[index];
    channel = data[index + 1];
    address = data[index + 2];
}

void Address::encode(std::vector<uint8_t>& data) {
    data.push_back((uint8_t)klass);
    data.push_back(channel);
    data.push_back(address);
}

String Address::toString() {
    char str[9];
    sprintf(str, "%02x.%02x.%02x", (uint8_t)klass, (uint8_t)channel, (uint8_t)address);
    return String(str);
}

// Command implementation
void Command::decode(std::vector<uint8_t>& data, unsigned int index) {
    packetInformation = ((int)data[index] & 128) >> 7 == 1;
    protocolVersion = (uint8_t)(((int)data[index] & 96) >> 5);
    retryCount = (uint8_t)(((int)data[index] & 24) >> 3);
    packetType = (PacketType)(((int)data[index + 1] & 240) >> 4);
    dataType = (DataType)((int)data[index + 1] & 15);
    packetNumber = data[index + 2];
}

void Command::encode(std::vector<uint8_t>& data) {
    data.push_back((uint8_t)((((int)packetInformation ? 1 : 0) << 7) + ((int)protocolVersion << 5) + ((int)retryCount << 3)));
    data.push_back((uint8_t)(((int)packetType << 4) + (int)dataType));
    data.push_back(packetNumber);
}

String Command::toString() {
    String str = "{";
    str += "PacketInformation: " + String(packetInformation) + ";";
    str += "ProtocolVersion: " + String(protocolVersion) + ";";
    str += "RetryCount: " + String(retryCount) + ";";
    str += "PacketType: " + String((int)packetType) + ";";
    str += "DataType: " + String((int)dataType) + ";";
    str += "PacketNumber: " + String(packetNumber);
    str += "}";
    return str;
}

// MessageSet implementation
MessageSet::MessageSet(MessageNumber messageNumber) {
    this->messageNumber = messageNumber;
    this->type = (MessageSetType)(((uint32_t)messageNumber & 1536) >> 9);
}

MessageSet MessageSet::decode(std::vector<uint8_t>& data, unsigned int index, int capacity) {
    MessageSet set = MessageSet((MessageNumber)((uint32_t)data[index] * 256U + (uint32_t)data[index + 1]));
    
    switch (set.type) {
        case MessageSetType::Enum:
            set.value = (int)data[index + 2];
            set.size = 3;
            break;
        case MessageSetType::Variable:
            set.value = (int)data[index + 2] << 8 | (int)data[index + 3];
            set.size = 4;
            break;
        case MessageSetType::LongVariable:
            set.value = (int)data[index + 2] << 24 | (int)data[index + 3] << 16 | (int)data[index + 4] << 8 | (int)data[index + 5];
            set.size = 6;
            break;
        case MessageSetType::Structure:
            if (capacity != 1) {
                DEBUG_PRINTF("structure messages can only have one message but is %d\n", capacity);
                return set;
            }
            Buffer buffer;
            set.size = data.size() - index - 3; // 3=end bytes
            buffer.size = set.size - 2;
            for (int i = 0; i < buffer.size && i < 255; i++) {
                buffer.data[i] = data[index + 2 + i]; // Skip message number bytes
            }
            set.structure = buffer;
            break;
        default:
            DEBUG_PRINTLN("Unknown message type");
    }
    
    return set;
}

void MessageSet::encode(std::vector<uint8_t>& data) {
    uint16_t messageNumber = (uint16_t)this->messageNumber;
    data.push_back((uint8_t)((messageNumber >> 8) & 0xff));
    data.push_back((uint8_t)(messageNumber & 0xff));
    
    switch (type) {
        case MessageSetType::Enum:
            data.push_back((uint8_t)value);
            break;
        case MessageSetType::Variable:
            data.push_back((uint8_t)(value >> 8) & 0xff);
            data.push_back((uint8_t)(value & 0xff));
            break;
        case MessageSetType::LongVariable:
            data.push_back((uint8_t)(value & 0x000000ff));
            data.push_back((uint8_t)((value & 0x0000ff00) >> 8));
            data.push_back((uint8_t)((value & 0x00ff0000) >> 16));
            data.push_back((uint8_t)((value & 0xff000000) >> 24));
            break;
        case MessageSetType::Structure:
            for (int i = 0; i < structure.size; i++) {
                data.push_back(structure.data[i]);
            }
            break;
        default:
            DEBUG_PRINTLN("Unknown message type");
    }
}

String MessageSet::toString() {
    switch (type) {
        case MessageSetType::Enum:
            return "Enum " + String((uint16_t)messageNumber, HEX) + " = " + String(value);
        case MessageSetType::Variable:
            return "Variable " + String((uint16_t)messageNumber, HEX) + " = " + String(value);
        case MessageSetType::LongVariable:
            return "LongVariable " + String((uint16_t)messageNumber, HEX) + " = " + String(value);
        case MessageSetType::Structure:
            return "Structure #" + String((uint16_t)messageNumber, HEX) + " = " + String(structure.size);
        default:
            return "Unknown";
    }
}

// Packet implementation
Packet Packet::create(Address da, DataType dataType, MessageNumber messageNumber, int value) {
    Packet packet = createPartial(da, dataType);
    MessageSet message(messageNumber);
    message.value = value;
    packet.messages.push_back(message);
    return packet;
}

Packet Packet::createPartial(Address da, DataType dataType) {
    Packet packet;
    packet.sa = Address::getMyAddress();
    packet.da = da;
    packet.command.packetInformation = true;
    packet.command.packetType = PacketType::Normal;
    packet.command.dataType = dataType;
    packet.command.packetNumber = packetCounter++;
    return packet;
}

DecodeResult Packet::decode(std::vector<uint8_t>& data) {
    if (data[0] != 0x32)
        return DecodeResult::InvalidStartByte;
        
    if (data.size() < 16 || data.size() > 1500)
        return DecodeResult::UnexpectedSize;
        
    int size = (int)data[1] << 8 | (int)data[2];
    if (size + 2 != (int)data.size())
        return DecodeResult::SizeDidNotMatch;
        
    if (data[data.size() - 1] != 0x34)
        return DecodeResult::InvalidEndByte;
        
    uint16_t crc_actual = crc16(data, 3, size - 4);
    uint16_t crc_expected = (int)data[data.size() - 3] << 8 | (int)data[data.size() - 2];
    if (crc_expected != crc_actual) {
        DEBUG_PRINTF("NASA: invalid crc - got %d but should be %d: %s\n", 
                     crc_actual, crc_expected, bytesToHex(data).c_str());
        return DecodeResult::CrcError;
    }
    
    unsigned int cursor = 3;
    
    sa.decode(data, cursor);
    cursor += sa.size;
    
    da.decode(data, cursor);
    cursor += da.size;
    
    command.decode(data, cursor);
    cursor += command.size;
    
    int capacity = (int)data[cursor];
    cursor++;
    
    messages.clear();
    for (int i = 1; i <= capacity; ++i) {
        MessageSet set = MessageSet::decode(data, cursor, capacity);
        messages.push_back(set);
        cursor += set.size;
    }
    
    return DecodeResult::Ok;
}

std::vector<uint8_t> Packet::encode() {
    std::vector<uint8_t> data;
    
    data.push_back(0x32);
    data.push_back(0); // size
    data.push_back(0); // size
    sa.encode(data);
    da.encode(data);
    command.encode(data);
    
    data.push_back((uint8_t)messages.size());
    for (size_t i = 0; i < messages.size(); i++) {
        messages[i].encode(data);
    }
    
    int endPosition = data.size() + 1;
    data[1] = (uint8_t)(endPosition >> 8);
    data[2] = (uint8_t)(endPosition & 0xFF);
    
    uint16_t checksum = crc16(data, 3, endPosition - 4);
    data.push_back((uint8_t)((unsigned int)checksum >> 8));
    data.push_back((uint8_t)((unsigned int)checksum & 0xFF));
    
    data.push_back(0x34);
    
    return data;
}

String Packet::toString() {
    String str = "#Packet Src:" + sa.toString() + " Dst:" + da.toString() + " " + command.toString() + "\n";
    
    for (size_t i = 0; i < messages.size(); i++) {
        if (i > 0) str += "\n";
        str += " > " + messages[i].toString();
    }
    
    return str;
}

// Conversion functions
Mode operationModeToMode(int value) {
    switch (value) {
        case 0: return Mode::Auto;
        case 1: return Mode::Cool;
        case 2: return Mode::Dry;
        case 3: return Mode::Fan;
        case 4: return Mode::Heat;
        default: return Mode::Unknown;
    }
}

FanMode fanModeRealToFanMode(int value) {
    switch (value) {
        case 1: return FanMode::Low;
        case 2: return FanMode::Mid;
        case 3: return FanMode::High;
        case 4: return FanMode::Turbo;
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15: return FanMode::Auto;
        case 254: return FanMode::Off;
        default: return FanMode::Unknown;
    }
}

int fanModeToNasaFanMode(FanMode mode) {
    switch (mode) {
        case FanMode::Low: return 1;
        case FanMode::Mid: return 2;
        case FanMode::High: return 3;
        case FanMode::Turbo: return 4;
        case FanMode::Auto:
        default: return 0;
    }
}

// Protocol processing
DecodeResult tryDecodeNasaPacket(std::vector<uint8_t> data) {
    return globalPacket.decode(data);
}

void processNasaPacket(MessageTarget* target) {
    const String source = globalPacket.sa.toString();
    const String dest = globalPacket.da.toString();
    
    target->registerAddress(source);
    
    DEBUG_PRINTF("MSG: %s\n", globalPacket.toString().c_str());
    
    if (globalPacket.command.dataType == DataType::Ack) {
        DEBUG_PRINTF("Ack %s\n", globalPacket.toString().c_str());
        return;
    }
    
    if (globalPacket.command.dataType != DataType::Notification)
        return;
        
    for (auto& message : globalPacket.messages) {
        processMessageSet(source, dest, message, target);
    }
}

void processMessageSet(String source, String dest, MessageSet& message, MessageTarget* target) {
    target->setCustomSensor(source, (uint16_t)message.messageNumber, (float)message.value);
    
    switch (message.messageNumber) {
        case MessageNumber::VAR_in_temp_room_f: {
            double temp = (double)message.value / 10.0;
            DEBUG_PRINTF("s:%s d:%s VAR_in_temp_room_f %g\n", source.c_str(), dest.c_str(), temp);
            target->setRoomTemperature(source, temp);
            break;
        }
        case MessageNumber::VAR_in_temp_target_f: {
            double temp = (double)message.value / 10.0;
            DEBUG_PRINTF("s:%s d:%s VAR_in_temp_target_f %g\n", source.c_str(), dest.c_str(), temp);
            target->setTargetTemperature(source, temp);
            break;
        }
        case MessageNumber::ENUM_in_operation_power: {
            DEBUG_PRINTF("s:%s d:%s ENUM_in_operation_power %g\n", source.c_str(), dest.c_str(), (double)message.value);
            target->setPower(source, message.value != 0);
            break;
        }
        case MessageNumber::ENUM_in_operation_mode: {
            DEBUG_PRINTF("s:%s d:%s ENUM_in_operation_mode %g\n", source.c_str(), dest.c_str(), (double)message.value);
            target->setMode(source, operationModeToMode(message.value));
            break;
        }
        case MessageNumber::ENUM_in_fan_mode: {
            DEBUG_PRINTF("s:%s d:%s ENUM_in_fan_mode %g\n", source.c_str(), dest.c_str(), (double)message.value);
            FanMode mode = FanMode::Unknown;
            if (message.value == 0) mode = FanMode::Auto;
            else if (message.value == 1) mode = FanMode::Low;
            else if (message.value == 2) mode = FanMode::Mid;
            else if (message.value == 3) mode = FanMode::High;
            else if (message.value == 4) mode = FanMode::Turbo;
            target->setFanMode(source, mode);
            break;
        }
        case MessageNumber::ENUM_in_louver_hl_swing: {
            DEBUG_PRINTF("s:%s d:%s ENUM_in_louver_hl_swing %g\n", source.c_str(), dest.c_str(), (double)message.value);
            target->setSwingVertical(source, message.value == 1);
            break;
        }
        case MessageNumber::ENUM_in_louver_lr_swing: {
            DEBUG_PRINTF("s:%s d:%s ENUM_in_louver_lr_swing %g\n", source.c_str(), dest.c_str(), (double)message.value);
            target->setSwingHorizontal(source, message.value == 1);
            break;
        }
        case MessageNumber::ENUM_in_alt_mode: {
            DEBUG_PRINTF("s:%s d:%s ENUM_in_alt_mode %g\n", source.c_str(), dest.c_str(), (double)message.value);
            Preset preset = static_cast<Preset>(message.value);
            target->setPreset(source, preset);
            break;
        }
        case MessageNumber::VAR_out_sensor_airout: {
            double temp = (double)((int16_t)message.value) / 10.0;
            DEBUG_PRINTF("s:%s d:%s VAR_out_sensor_airout %g\n", source.c_str(), dest.c_str(), temp);
            target->setOutdoorTemperature(source, temp);
            break;
        }
        case MessageNumber::VAR_in_temp_eva_in_f: {
            double temp = ((int16_t)message.value) / 10.0;
            DEBUG_PRINTF("s:%s d:%s VAR_in_temp_eva_in_f %g\n", source.c_str(), dest.c_str(), temp);
            target->setIndoorEvaInTemperature(source, temp);
            break;
        }
        case MessageNumber::VAR_in_temp_eva_out_f: {
            double temp = ((int16_t)message.value) / 10.0;
            DEBUG_PRINTF("s:%s d:%s VAR_in_temp_eva_out_f %g\n", source.c_str(), dest.c_str(), temp);
            target->setIndoorEvaOutTemperature(source, temp);
            break;
        }
        case MessageNumber::VAR_out_error_code: {
            int code = static_cast<int>(message.value);
            DEBUG_PRINTF("s:%s d:%s VAR_out_error_code %d\n", source.c_str(), dest.c_str(), code);
            target->setErrorCode(source, code);
            break;
        }
        case MessageNumber::LVAR_OUT_CONTROL_WATTMETER_1W_1MIN_SUM: {
            double value = static_cast<double>(message.value);
            DEBUG_PRINTF("s:%s d:%s LVAR_OUT_CONTROL_WATTMETER_1W_1MIN_SUM %g\n", source.c_str(), dest.c_str(), value);
            target->setOutdoorInstantaneousPower(source, value);
            break;
        }
        case MessageNumber::LVAR_OUT_CONTROL_WATTMETER_ALL_UNIT_ACCUM: {
            double value = static_cast<double>(message.value);
            DEBUG_PRINTF("s:%s d:%s LVAR_OUT_CONTROL_WATTMETER_ALL_UNIT_ACCUM %g\n", source.c_str(), dest.c_str(), value);
            target->setOutdoorCumulativeEnergy(source, value);
            break;
        }
        case MessageNumber::VAR_OUT_SENSOR_CT1: {
            double value = static_cast<double>(message.value) / 10.0;
            DEBUG_PRINTF("s:%s d:%s VAR_OUT_SENSOR_CT1 %g\n", source.c_str(), dest.c_str(), value);
            target->setOutdoorCurrent(source, value);
            break;
        }
        case MessageNumber::LVAR_NM_OUT_SENSOR_VOLTAGE: {
            double value = static_cast<double>(message.value);
            DEBUG_PRINTF("s:%s d:%s LVAR_NM_OUT_SENSOR_VOLTAGE %g\n", source.c_str(), dest.c_str(), value);
            target->setOutdoorVoltage(source, value);
            break;
        }
        default: {
            DEBUG_PRINTF("Undefined s:%s d:%s %s\n", source.c_str(), dest.c_str(), message.toString().c_str());
            break;
        }
    }
}

// NasaProtocol implementation
void NasaProtocol::publishRequest(MessageTarget* target, const String& address, ProtocolRequest& request) {
    Packet packet = Packet::createPartial(Address::parse(address), DataType::Request);
    
    if (request.hasMode) {
        request.hasPower = true; // ensure system turns on when mode is set
        
        MessageSet mode(MessageNumber::ENUM_in_operation_mode);
        mode.value = (int)request.mode;
        packet.messages.push_back(mode);
    }
    
    if (request.hasPower) {
        MessageSet power(MessageNumber::ENUM_in_operation_power);
        power.value = request.power ? 1 : 0;
        packet.messages.push_back(power);
    }
    
    if (request.hasTargetTemperature) {
        MessageSet targettemp(MessageNumber::VAR_in_temp_target_f);
        targettemp.value = request.targetTemperature * 10.0;
        packet.messages.push_back(targettemp);
    }
    
    if (request.hasFanMode) {
        MessageSet fanmode(MessageNumber::ENUM_in_fan_mode);
        fanmode.value = fanModeToNasaFanMode(request.fanMode);
        packet.messages.push_back(fanmode);
    }
    
    if (request.hasSwingVertical || request.hasSwingHorizontal) {
        if (request.hasSwingVertical) {
            MessageSet hl_swing(MessageNumber::ENUM_in_louver_hl_swing);
            hl_swing.value = request.swingVertical ? 1 : 0;
            packet.messages.push_back(hl_swing);
        }
        
        if (request.hasSwingHorizontal) {
            MessageSet lr_swing(MessageNumber::ENUM_in_louver_lr_swing);
            lr_swing.value = request.swingHorizontal ? 1 : 0;
            packet.messages.push_back(lr_swing);
        }
    }
    
    if (request.hasPreset) {
        MessageSet preset(MessageNumber::ENUM_in_alt_mode);
        preset.value = static_cast<int>(request.preset);
        packet.messages.push_back(preset);
    }
    
    if (packet.messages.size() == 0)
        return;
        
    DEBUG_PRINTF("publish packet %s\n", packet.toString().c_str());
    
    auto data = packet.encode();
    target->publishData(data);
}

void NasaProtocol::protocolUpdate(MessageTarget* target) {
    // Unused for NASA protocol
}