/*
/ Copyright (c) 2019-2020 Facebook Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#include <sys/sysinfo.h>
#include <systemd/sd-journal.h>
#include <phosphor-logging/log.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/steady_timer.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <vector>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdlib.h>
#include <string.h>

namespace firmwareUpdate
{

// Max retry limit
static constexpr uint8_t max_retry = 3;

// IANA ID
static constexpr uint8_t iana_id_0 = 0x15;
static constexpr uint8_t iana_id_1 = 0xA0;
static constexpr uint8_t iana_id_2 = 0x00;

// ME recovery cmd
static constexpr uint8_t bic_intf_me = 0x1;
static constexpr uint8_t me_recv_cmd_0 = 0xB8;
static constexpr uint8_t me_recv_cmd_1 = 0xD7;
static constexpr uint8_t me_recv_cmd_2 = 0x57;
static constexpr uint8_t me_recv_cmd_3 = 0x01;
static constexpr uint8_t me_recv_cmd_4 = 0x00;

static constexpr uint8_t verify_me_recv_cmd_0 = 0x18;
static constexpr uint8_t verify_me_recv_cmd_1 = 0x04;
static constexpr uint8_t me_recv_cmd = 0x1;

// BIOS SIZE
static constexpr uint32_t bios_64k_size = (64*1024);
static constexpr uint32_t bios_32k_size = (32*1024);

// Command Id
static constexpr uint8_t me_recv_id = 0x2;
static constexpr uint8_t get_fw_chksum_id = 0xA;
static constexpr uint8_t firmware_update_id = 0x9;
static constexpr uint8_t get_cpld_update_progress = 0x1A;

// Error Codes
static constexpr uint8_t write_flash_err   = 0x80;
static constexpr uint8_t power_sts_chk_err = 0x81;
static constexpr uint8_t data_len_err      = 0x82;
static constexpr uint8_t flash_erase_err   = 0x83;
static constexpr uint8_t cpld_err_code     = 0xFD;
static constexpr uint8_t me_recv_err_0     = 0x81;
static constexpr uint8_t me_recv_err_1     = 0x2;

// General declarations
static constexpr uint8_t resp_size = 6;
static constexpr uint8_t net_fn = 0x38;
static constexpr uint8_t ipmb_write_128b = 128;

// Host Numbers
static constexpr uint8_t host1 = 0;
static constexpr uint8_t host2 = 4;
static constexpr uint8_t host3 = 8;
static constexpr uint8_t host4 = 12;

static constexpr uint8_t update_bios= 0;
static constexpr uint8_t update_cpld= 1;
static constexpr uint8_t update_bic_bootloader= 2;
static constexpr uint8_t update_bic= 3;
static constexpr uint8_t update_vr= 4;

std::shared_ptr<sdbusplus::asio::connection> conn;
static boost::asio::io_service io;
static constexpr uint8_t lun = 0;

using respType =
    std::tuple<int, uint8_t, uint8_t, uint8_t, uint8_t, std::vector<uint8_t>>;


void print_help()
{
    phosphor::logging::log<phosphor::logging::level::ERR>(
    "Usage: <file_name> <bin_file_path> <host1/2/3/4> <--update> <bios/cpld/bic/bicbtl/vr>");
}


/*
Function Name    : sendIPMBRequest
Description      : Send data to target through IPMB
*/
int sendIPMBRequest(uint8_t host, uint8_t netFn, uint8_t cmd,
                    std::vector<uint8_t> &cmdData,
                    std::vector<uint8_t> &respData)
{
    auto method = conn->new_method_call("xyz.openbmc_project.Ipmi.Channel.Ipmb",
                                        "/xyz/openbmc_project/Ipmi/Channel/Ipmb",
                                        "org.openbmc.Ipmb", "sendRequest");
    method.append(host, netFn, lun, cmd, cmdData);

    auto reply = conn->call(method);
    if (reply.is_method_error())
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
        "Error reading from IPMB");
        return -1;
    }

    respType resp;
    reply.read(resp);

    respData =
        std::move(std::get<std::remove_reference_t<decltype(respData)>>(resp));
    respData.insert(respData.begin(), std::get<4>(resp));

    if (respData.size() <= 0)
    {
        return -1;
    }
    return 0;
}


/*
Function Name    : sendFirmwareUpdateData
Description      : Form vectors with required data to send
*/
int sendFirmwareUpdateData(uint8_t slotId, std::vector<uint8_t> &sendData,
                           uint32_t offset, uint8_t target)
{
    // Vector declaration
    std::vector<uint8_t> cmdData{iana_id_0, iana_id_1, iana_id_2};
    std::vector<uint8_t> respData;

    // Variable declaration
    int retries = max_retry;
    uint8_t len_byte[2];
    uint8_t offset_byte[4];
    *(uint32_t *)&offset_byte = offset;
    *(uint16_t *)&len_byte = sendData.size();

    // Frame the Firmware send IPMB data
    cmdData.push_back(target);
    cmdData.insert(cmdData.end(), offset_byte, offset_byte + sizeof(offset_byte));
    cmdData.insert(cmdData.end(), len_byte, len_byte + sizeof(len_byte));
    cmdData.insert(cmdData.end(), sendData.begin(), sendData.end());

    while (retries != 0)
    {
        int ret = sendIPMBRequest(slotId, net_fn, firmware_update_id,
                                  cmdData, respData);
        if (ret)
        {
            return -1;
        }
        // Check the completion code and the IANA_ID (0x15) for success
        if ((respData[0] == 0) && (respData[1] == iana_id_0)){
            break;
        } else if (respData[0] == write_flash_err) {
            phosphor::logging::log<phosphor::logging::level::ERR>(
            "Write Flash Error!!");
        } else if (respData[0] == power_sts_chk_err) {
            phosphor::logging::log<phosphor::logging::level::ERR>(
            "Power status check Fail!!");
        } else if (respData[0] == data_len_err) {
            phosphor::logging::log<phosphor::logging::level::ERR>(
            "Data length Error!!");
        } else if (respData[0] == flash_erase_err) {
            phosphor::logging::log<phosphor::logging::level::ERR>(
            "Flash Erase Error!!");
        } else {
            phosphor::logging::log<phosphor::logging::level::ERR>(
            "Invalid Data...");
        }
        std::string logMsg = "slot:" + std::to_string(slotId) + " Offset:" +
            std::to_string(offset) + " len:" + std::to_string(sendData.size()) +
            " Retrying..";
        phosphor::logging::log<phosphor::logging::level::ERR>(logMsg.c_str());
        retries--;
    }

    if (retries == 0)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
        "Error!!! Not able to send bios data!!!");
        return -1;
    }
    return 0;
}


/*
Function Name    : getChksumFW
Description      : Get the checksum value of bios image
*/
int getChksumFW(uint8_t slotId, uint32_t offset, uint32_t len,
                std::vector<uint8_t> &respData, uint8_t target)
{
    // Variable declaration
    std::vector<uint8_t> cmdData{iana_id_0, iana_id_1, iana_id_2};
    int retries = max_retry;
    uint8_t len_byte[4];
    uint8_t offset_byte[4];
    *(uint32_t *)&offset_byte = offset;
    *(uint32_t *)&len_byte = len;

    // Frame the IPMB request data
    cmdData.push_back(target);
    cmdData.insert(cmdData.end(), offset_byte, offset_byte + sizeof(offset_byte));
    cmdData.insert(cmdData.end(), len_byte, len_byte + sizeof(len_byte));

    while (retries > 0)
    {
        sendIPMBRequest(slotId, net_fn, get_fw_chksum_id, cmdData, respData);
        if (respData.size() != resp_size)
        {
            std::string logMsg = "Checksum values not obtained properly for slot: " +
               std::to_string(slotId) + " Offset:" + std::to_string(offset)
               + " len:" + std::to_string(len) + " Retrying..." ;
            phosphor::logging::log<phosphor::logging::level::ERR>(logMsg.c_str());
            retries--;
        }
    }

    if (retries == 0)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
        "Failed to get the Checksum value from firmware..");
        return -1;
    }
    return 0;
}


/*
Function Name    : meRecovery
Description      : Set Me to recovery mode
*/
int meRecovery(uint8_t slotId, uint8_t mode)
{
    // Variable declarations
    std::vector<uint8_t> cmdData{iana_id_0, iana_id_1,
                                 iana_id_2, bic_intf_me};
    std::vector<uint8_t> respData;
    int retries = max_retry;
    uint8_t me_recovery_cmd[] = {me_recv_cmd_0,
                                 me_recv_cmd_1,
                                 me_recv_cmd_2,
                                 me_recv_cmd_3,
                                 me_recv_cmd_4};

    // Frame the IPMB send request data for ME recovery
    cmdData.insert(cmdData.end(), me_recovery_cmd,
                   me_recovery_cmd + sizeof(me_recovery_cmd));
    cmdData.push_back(mode);

    phosphor::logging::log<phosphor::logging::level::INFO>(
    "Setting ME to recovery mode");
    while (retries > 0)
    {
        sendIPMBRequest(slotId, net_fn, me_recv_id, cmdData, respData);
        if (respData.size() != resp_size) {
            phosphor::logging::log<phosphor::logging::level::ERR>(
            "ME is not set into recovery mode.. Retrying...");
        } else if (respData[3] != cmdData[3]) {
            phosphor::logging::log<phosphor::logging::level::ERR>(
            "Interface not valid.. Retrying...");
        } else if (respData[0] == 0) {
            phosphor::logging::log<phosphor::logging::level::ERR>(
            "ME recovery mode -> Completion Status set..");
            break;
        } else if (respData[0] != 0) {
            phosphor::logging::log<phosphor::logging::level::ERR>(
            "ME recovery mode -> Completion Status not set.. Retrying..");
        } else {
            phosphor::logging::log<phosphor::logging::level::ERR>(
            "Invalid data or command...");
        }
        retries--;
    }

    if (retries == 0)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
        "Failed to set ME to recovery mode..");
        return -1;
    }

    // Verify whether ME went to recovery mode
    std::vector<uint8_t> meData{iana_id_0,
                                iana_id_1,
                                iana_id_2,
                                bic_intf_me,
                                verify_me_recv_cmd_0,
                                verify_me_recv_cmd_1};
    std::vector<uint8_t> meResp;
    retries = max_retry;

    while (retries != 0)
    {
        sendIPMBRequest(slotId, net_fn, me_recv_id, meData, meResp);
        if (meResp[3] != meData[3])
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
            "Interface not valid.. Retrying...");
        } else if ((mode == me_recv_id) && (meResp[1] == me_recv_err_0) &&
                                           (meResp[2] == me_recv_err_1))
        {
            return 0;
        }
        retries--;
    }

    if (retries == 0)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
        "Failed to set ME to recovery mode in self tests..");
        return -1;
    }
    phosphor::logging::log<phosphor::logging::level::INFO>(
                  "ME is set to recovery mode");
    return 0;
}


int biosVerifyImage(const char *imagePath, uint8_t slotId, uint8_t target)
{
    // Check for bios image
    uint32_t offset = 0;
    uint32_t biosVerifyPktSize = bios_32k_size;

    phosphor::logging::log<phosphor::logging::level::INFO>("Verify Bios image..");

    // Open the file
    std::streampos fileSize;
    std::ifstream file(imagePath,
                       std::ios::in | std::ios::binary);

    if (!file.is_open())
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
        "Unable to open file");
        return -1;
    }
    file.seekg(0, std::ios::beg);

    phosphor::logging::log<phosphor::logging::level::INFO>(
    "Starting Bios image verification");
    while (offset < fileSize)
    {
        // Read the data
        std::vector<int> chksum(biosVerifyPktSize);
        file.read((char *)&chksum[0], biosVerifyPktSize);

        // Calculate checksum
        uint32_t tcksum = 0;
        for (int byte_index = 0; byte_index < chksum.size(); byte_index++)
        {
            tcksum += chksum[byte_index];
        }

        std::vector<std::uint8_t> calChksum((std::uint8_t *)&tcksum,
                                            (std::uint8_t *)&(tcksum) +
                                             sizeof(std::uint32_t));

       // Get the checksum value from firmware
       uint8_t retValue;
       std::vector<uint8_t> fwChksumData;

       retValue = getChksumFW(slotId, offset, biosVerifyPktSize,
                              fwChksumData, target);
       if (retValue != 0)
       {
           phosphor::logging::log<phosphor::logging::level::ERR>(
           "Failed to get the Checksum value!!");
           return -1;
       }

       for (uint8_t ind = 0; ind <= calChksum.size(); ind++)
       {
           // Compare both and see if they match or not
           if (fwChksumData[ind] != calChksum[ind])
           {
               std::string logMsg = "Checksum Failed! Offset: " +
                         std::to_string(offset) +
                         " Expected: " + std::to_string(calChksum[ind]) +
                         " Actual: " + std::to_string(fwChksumData[ind]);
               phosphor::logging::log<phosphor::logging::level::ERR>(logMsg.c_str());
               return -1;
           }
       }
       offset += biosVerifyPktSize;
    }
    phosphor::logging::log<phosphor::logging::level::ERR>(
    "Bios image verification Successful..");
    file.close();
    return 0;
}


/*
Function Name   : updateFirmwareTarget
Description     : Send data to respective target for FW udpate
Param: slotId   : Slot Id
Param: imagePath: Binary image path
Param: target: cmd Id to find the target (BIOS, CPLD, VR, ME)
*/
int updateFirmwareTarget(uint8_t slotId, const char *imagePath, uint8_t target)
{
    // Variable Declartion
    int count = 0x0;
    uint32_t offset = 0x0;
    uint32_t ipmbWriteMax  = ipmb_write_128b;

    // Set ME to recovery mode
    int ret_val = meRecovery(slotId, me_recv_cmd);
    if (ret_val != 0)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
        "Me set to recovery mode failed");
        return -1;
    }

    // Open the file
    std::streampos fileSize;
    std::ifstream file(imagePath,
                       std::ios::in | std::ios::binary | std::ios::ate);

    if (!file.is_open())
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
        "Unable to open file");
    }

    // Get its size
    fileSize = file.tellg();
    std::string logMsg = "Bin File Size: " + std::to_string(fileSize);
    phosphor::logging::log<phosphor::logging::level::INFO>(logMsg.c_str());

    // Check whether the image is valid
    if (fileSize <= 0)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
        "Invalid bin File");
        return -1;
    } else {
        phosphor::logging::log<phosphor::logging::level::INFO>(
        "Valid bin File");
    }
    file.seekg(0, std::ios::beg);
    int index = 1;

    phosphor::logging::log<phosphor::logging::level::INFO>(
    "Firmware write started");
    while (offset < fileSize)
    {

        // count details
        uint32_t count = ipmbWriteMax;
        uint8_t target_value = target;

        if ((target == update_bios) && ((offset + ipmbWriteMax) >= (index * bios_64k_size)))
        {
            count = (index * bios_64k_size) - offset;
            index++;
        }

        if ((target != update_bios) && ((offset+count) >= fileSize))
        {
            target_value = target_value | 0x80;
        }

        // Read the data
        std::vector<uint8_t> fileData(ipmbWriteMax);
        file.read((char *)&fileData[0], ipmbWriteMax);

        // Send data
        int ret = sendFirmwareUpdateData(slotId, fileData, offset, target_value);
        if (ret != 0)
        {
            std::string logMsg = "Firmware update Failed at offset: " +
                       std::to_string(offset);
            phosphor::logging::log<phosphor::logging::level::ERR>(logMsg.c_str());
            return -1;
        }

        // Update counter
        offset += count;
    }
    phosphor::logging::log<phosphor::logging::level::INFO>(
    "Firmware write Done");
    file.close();

    if (target == update_bios)
    {
        int ret = biosVerifyImage(imagePath, slotId, target);
        if (ret) {
            return -1;
        }
    }

    return 0;
}


int cpldUpdateFw(uint8_t slotId, const char *imagePath)
{
    int ret = updateFirmwareTarget(slotId, imagePath, update_cpld);
    if (ret != 0)
    {
        std::string logMsg = "CPLD update failed for slot#" +
                             std::to_string(slotId);
        phosphor::logging::log<phosphor::logging::level::ERR>(logMsg.c_str());
        return -1;
    }
    std::string logMsg = "CPLD update completed successfully for slot#" +
                         std::to_string(slotId);
    phosphor::logging::log<phosphor::logging::level::INFO>(logMsg.c_str());
    return 0;
}


int hostBiosUpdateFw(uint8_t slotId, const char *imagePath)
{
    int ret = updateFirmwareTarget(slotId, imagePath, update_bios);
    if (ret != 0)
    {
        std::string logMsg = "BIOS update failed for slot#" +
                             std::to_string(slotId);
        phosphor::logging::log<phosphor::logging::level::ERR>(logMsg.c_str());
        return -1;
    }
    std::string logMsg = "BIOS update completed successfully for slot#" +
                         std::to_string(slotId);
    phosphor::logging::log<phosphor::logging::level::INFO>(logMsg.c_str());
    return 0;
}


int updateFw(char *argv[], uint8_t slotId)
{
    const char *binFile = argv[1];
    // Check for the FW udpate
    if (strcmp(argv[3], "--update") != 0)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
        "Invalid Update command");
        print_help();
        return -1;
    }

    if (strcmp(argv[4], "bios") == 0)
    {
        int ret = hostBiosUpdateFw(slotId, binFile);
        if (ret != 0)
        {
            return -1;
        }

    } else if (strcmp(argv[4], "cpld") == 0) {
        int ret = cpldUpdateFw(slotId, binFile);
        if (ret != 0)
        {
            return -1;
        }

    } else {
        phosphor::logging::log<phosphor::logging::level::ERR>(
        "Invalid Update command");
        print_help();
        return -1;
    }

    return 0;
}

}// namespace end

int main(int argc, char *argv[])
{
    firmwareUpdate::conn =
        std::make_shared<sdbusplus::asio::connection>(firmwareUpdate::io);
    // Get the arguments
    uint8_t slotId;

    // Check for the host name
    if(strcmp(argv[2], "host1") == 0) {
        slotId = firmwareUpdate::host1;
    } else if (strcmp(argv[2], "host2") == 0) {
        slotId = firmwareUpdate::host2;
    } else if (strcmp(argv[2], "host3") == 0) {
        slotId = firmwareUpdate::host3;
    } else if (strcmp(argv[2], "host4") == 0) {
        slotId = firmwareUpdate::host4;
    } else {
        phosphor::logging::log<phosphor::logging::level::ERR>(
        "Invalid host number");
        firmwareUpdate::print_help();
        return -1;
    }

    // Update the FW
    int ret = firmwareUpdate::updateFw(argv, slotId);
    if (ret != 0)
    {
        std::string logMsg = "FW update failed for slot#" + std::to_string(slotId);
        phosphor::logging::log<phosphor::logging::level::ERR>(logMsg.c_str());
        return -1;
    }
    std::string logMsg = "FW update completed successfully for slot#" +
                         std::to_string(slotId);
    phosphor::logging::log<phosphor::logging::level::INFO>(logMsg.c_str());
    return 0;
}

