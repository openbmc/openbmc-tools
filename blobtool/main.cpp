#include <stdio.h>
#include <systemd/sd-bus.h>
#include <vector>
#include <stdlib.h>
#include <string>
#include <unistd.h>
#include <assert.h>
#include <fstream>

// This tool reads a BLOB on the BMC.
// This will be particularly useful for testing blob-related stuff without a usable host.

sd_bus* g_bus = nullptr;
constexpr const char *APP_BANNER = "[Blob Tool for the BMC]";

bool g_verbose = false;
int g_chars_per_line = 32;

// IPMID works with both legacy and new interfaces.
// We will call it through the new interface in this program.
constexpr const char *IPMID_BUS_NAME    = "xyz.openbmc_project.Ipmi.Host";
constexpr const char *IPMID_OBJECT_PATH = "/xyz/openbmc_project/Ipmi";
constexpr const char *IPMID_INTERFACE   = "xyz.openbmc_project.Ipmi.Server";

void Terminate() {
	printf("%s program terminated.\n", APP_BANNER);
	exit(1);
}

// Stolen from "ipmi_utils.c"
// This implementation tracks the specification given at
// http://srecord.sourceforge.net/crc16-ccitt.html
uint16_t ipmi_gen_crc16(const void* data, size_t size) {
	const uint16_t kPoly = 0x1021;
	const uint16_t kLeftBit = 0x8000;
	const int kExtraRounds = 2;
	const uint8_t* bytes = (const uint8_t*)data;
	uint16_t crc = 0xFFFF;
	size_t i;
	size_t j;

	for (i = 0; i < size + kExtraRounds; ++i) {
		for (j = 0; j < 8; ++j) {
			bool xor_flag = (crc & kLeftBit) ? 1 : 0;
			crc <<= 1;
			// If this isn't an extra round and the current byte's j'th bit from the
			// left is set, increment the CRC.
			if (i < size && (bytes[i] & (1 << (7 - j)))) {
				crc++;
			}
			if (xor_flag) {
				crc ^= kPoly;
			}
		}
	}
	return crc;
}

uint16_t CharVectorToU16LE(char* ptr) {
	return (unsigned char)(ptr[0]) | ((unsigned char)(ptr[1]) << 8);
}

uint32_t CharVectorToU32LE(char* ptr) {
	return (unsigned char)(ptr[0]) |
		   ((unsigned char)(ptr[1]) << 8) |
		   ((unsigned char)(ptr[2]) << 16) |
		   ((unsigned char)(ptr[3]) << 24);
}

std::vector<char> U16LEToCharVector(uint16_t x) {
	std::vector<char> ret;
	ret.push_back(x & 0xff);
	ret.push_back((x >> 8) & 0xff);
	return ret;
}

std::vector<char> U32LEToCharVector(uint32_t x) {
	std::vector<char> ret;
	ret.push_back(x & 0xff);
	ret.push_back((x >> 8) & 0xff);
	ret.push_back((x >> 16) & 0xff);
	ret.push_back((x >> 24) & 0xff);
	return ret;
}

class IpmiBlob {
public:
	struct BlobStat {
		uint16_t blob_state;
		uint32_t size;
		std::vector<char> metadata;
		void Print() {
			printf("Blob state:      %04X\n", blob_state);
			printf("Blob size:       %u\n", size);
			printf("Metadata length: %lu\n", metadata.size());
			if (metadata.empty() == false) {
				printf("Metadata       : ");
				for (char ch : metadata) printf("%c", ch);
				printf("\n");
			}
		}
	};
	// Returns true if successful, false if failed
	// Signature: yyyaya{sv}
	// Result:    (yyyyay)
	static bool SendIpmiBlobCMD(unsigned char blob_cmd, const std::vector<char>& blob_payload, bool has_crc, std::vector<char>* p_ret) {
		sd_bus_message* m = nullptr;
		sd_bus_error error = SD_BUS_ERROR_NULL;
		int r;

		std::vector<char> ipmi_payload = { 0xcf, 0xc2, 0x00 };
		ipmi_payload.push_back(blob_cmd);
		if (has_crc) {
			uint16_t crc = ipmi_gen_crc16((const void*)blob_payload.data(), blob_payload.size());
			std::vector<char> crc1 = U16LEToCharVector(crc);
			ipmi_payload.insert(ipmi_payload.end(), crc1.begin(), crc1.end());
		}
		ipmi_payload.insert(ipmi_payload.end(), blob_payload.begin(), blob_payload.end());

		if (g_verbose) {
			printf("Sending IPMI payload: ipmitool raw 46 128");
			for (int i=0; i<int(ipmi_payload.size()); i++) {
				printf(" 0x%02x", ipmi_payload[i]);
			}
			printf("\n");
		}

		r = sd_bus_message_new_method_call(g_bus, &m,
			IPMID_BUS_NAME,
			IPMID_OBJECT_PATH,
			IPMID_INTERFACE,
			"execute");
		if (r < 0) { printf("Failed to create method call\n"); return false; }
		r = sd_bus_message_append(m, "yyy", 46, 0, 128);
		if (r < 0) { printf("Failed to append bytes to method call\n"); return false; }
		r = sd_bus_message_append_array(m, 'y', (void*)ipmi_payload.data(), int(ipmi_payload.size()));
		if (r < 0) { printf("Failed to append byte array to method call\n"); return false; }
		r = sd_bus_message_open_container(m, 'a', "{sv}");
		if (r < 0) { printf("Failed to open container for method call\n"); return false; }
		r = sd_bus_message_close_container(m);
		if (r < 0) { printf("Failed to close container for method call\n"); return false; }

		sd_bus_message* response = nullptr;
		r = sd_bus_call(g_bus, m, 0, &error, &response);
		if (r < 0) { printf("Failed to make method call\n"); return 0; }

		// Same as executionEntry in ipmid-new.cpp
		char reqNetFn, lun, cmd, cc;
		char* data;
		size_t data_size;
		r = sd_bus_message_enter_container(response, SD_BUS_TYPE_STRUCT, "yyyyay");
		if (r < 0) { printf("Failed to enter response struct\n"); return false; }
		r = sd_bus_message_read(response, "yyyy", &reqNetFn, &lun, &cmd, &cc);
		if (r < 0) { printf("Failed to read reply: %d, %s\n", r, strerror(-r)); return false; }
		r = sd_bus_message_read_array(response, 'y', (const void**)&data, &data_size);

		if (cc != 0) {
			printf("The command (0x%02X) returned a non-zero CC: %d (0x%02X).\n", blob_cmd, cc, cc);
		}

		std::vector<char> ret;
		for (size_t i=0; i<data_size; i++) { ret.push_back(data[i]); }

		sd_bus_message_unref(m);
		sd_bus_message_unref(response);

		// Check CRC
		if (ret.size() > 5) {
			uint16_t crc0 = CharVectorToU16LE(ret.data() + 3);
			uint16_t crc1 = ipmi_gen_crc16((const void*)(ret.data() + 5), ret.size()-5);
			if (crc0 != crc1) {
				printf("CRC in response do not match contents\n");
				return false;
			}
		}
		*p_ret = ret;
		return true;
	}

	// Will abort when any error is encountered
	// Returns blob count
	
	// Legend:
	//
	//
	// +----------+-----+--------------+
	// | cf c2 00 | CRC | blob payload |
	// +----------+-----+--------------+
	// 
	// \______________ ________________/
	//                v
	//                
	//           IPMI Payload


	// BmcBlobCount, 0x00
	static unsigned GetBlobCount() {
		std::vector<char> ret; // ret is an IPMI payload
		bool ok = SendIpmiBlobCMD(0, std::vector<char>(), false, &ret);
		if (!ok) { printf("Could not send IPMI blob command for GetCount\n"); Terminate(); }
		unsigned blob_count = CharVectorToU32LE(ret.data() + 5);
		return blob_count;
	}

	// BmcBlobEnumerate, 0x01
	static std::string EnumerateBlob(uint32_t blob_idx) {
		std::vector<char> blob_payload = U32LEToCharVector(blob_idx);
		std::vector<char> ret; // ret is an IPMI payload
		bool ok = SendIpmiBlobCMD(1, blob_payload, true, &ret);
		if (!ok) { printf("Could not send IPMI blob command for Enumerate\n"); Terminate(); }
		std::string blob_id;
		blob_id.insert(blob_id.end(), ret.begin() + 5, ret.end());
		return blob_id;
	}

	// BmcBlobOpen, 0x02
	// Returns a session ID if successful
	static uint16_t OpenBlob(uint16_t flag, const std::string& blob_id) {
		std::vector<char> blob_payload = U16LEToCharVector(flag);
		blob_payload.insert(blob_payload.end(), blob_id.begin(), blob_id.end());
		blob_payload.push_back(0x00); // Null-terminate the string
		std::vector<char> ret;
		bool ok = SendIpmiBlobCMD(2, blob_payload, true, &ret);
		if (!ok) {
			printf("Could not send BlobOpen command for %s, flag=%X\n", blob_id.c_str(), flag);
			Terminate();
			abort();
		}
		else return CharVectorToU16LE(ret.data() + 5);
	}

	// BmcBlobRead, 0x03
	static std::vector<char> ReadBlob(uint16_t session_id, uint32_t offset, uint32_t requested_size) {
		std::vector<char> blob_payload;
		std::vector<char> x = U16LEToCharVector(session_id);
		blob_payload.insert(blob_payload.end(), x.begin(), x.end());
		x = U32LEToCharVector(offset);
		blob_payload.insert(blob_payload.end(), x.begin(), x.end());
		x = U32LEToCharVector(requested_size);
		blob_payload.insert(blob_payload.end(), x.begin(), x.end());
		std::vector<char> ret;
		bool ok = SendIpmiBlobCMD(3, blob_payload, true, &ret);
		if (!ok) {
			printf("Could not send BlobRead command for session 0x%X, offset=%u, requested_size=%u\n",
					session_id, offset, requested_size);
			Terminate();
			abort();
		} else {
			x.clear();
			x.insert(x.end(), ret.begin()+5, ret.end());
			return x;
		}
	}

	// BmcBlobClose, 0x06
	static void CloseBlob(uint16_t session_id) {
		std::vector<char> blob_payload = U16LEToCharVector(session_id);
		std::vector<char> ret;
		bool ok = SendIpmiBlobCMD(6, blob_payload, true, &ret);
		assert(ret.size() == 3); // Only IPMI OEN numbers, no CRC or Blob payload
		if (!ok) {
			printf("Could not send BlobClose command for seession 0x%X\n", session_id);
			Terminate();
		}
	}

	// BmcBlobStat, 0x08
	static struct BlobStat StatBlob(const std::string& blob_id) {
		std::vector<char> blob_payload;
		blob_payload.insert(blob_payload.end(), blob_id.begin(), blob_id.end());
		blob_payload.push_back(0x00); // null-terminate the string
		std::vector<char> ret;
		bool ok = SendIpmiBlobCMD(8, blob_payload, true, &ret);
		if (!ok) {
			printf("Could not send BlobStat for blob %s\n", blob_id.c_str());
			Terminate(); abort();
		} else {
			struct BlobStat stat;
			stat.blob_state = CharVectorToU16LE(ret.data() + 5);
			stat.size       = CharVectorToU32LE(ret.data() + 7);
			unsigned char metadata_len = (unsigned char)ret[11];
			for (unsigned char i=0; i<metadata_len; i++) {
				stat.metadata.push_back(ret[12 + i]);
			}
			return stat;
		}
	}

	IpmiBlob(const std::string& blob_id, uint16_t flag) {
		flag_ = flag;
		blob_id_ = blob_id;
	}

	void Open() {
		session_id_ = OpenBlob(flag_, blob_id_);
//		printf("Blob %s opened, session=%X\n", blob_id_.c_str(), session_id_);
	}

	std::vector<char> Read() {
		std::vector<char> ret;
		int req_size = 50;
		int offset = 0;

		// For Metrics and /skm/hss blobs, this is skipped
		if (blob_id_.find("/skm/hss/") == -1 &&
			blob_id_.find("/metric/snapshot") == -1) {
			IpmiBlob::BlobStat stat = StatBlob(blob_id_);
			if (stat.blob_state != 0x01) return ret; // 1: read
		}

		while (true) {
			std::vector<char> chunk = ReadBlob(session_id_, offset, req_size);
			if (chunk.size() < 1) break;
			offset += chunk.size();
			ret.insert(ret.end(), chunk.begin(), chunk.end());
		}
		return ret;
	}

	void Close() {
		if (session_id_ == 0) {
			printf("This blob does not appear to have a valid session yet so it probably can't be closed.\n");
		}
		CloseBlob(session_id_);
		printf("Blob %s closed.\n", blob_id_.c_str());
	}

protected:
	uint16_t flag_;
	uint16_t session_id_;
	std::string blob_id_;
};

void ListBlobs() {
	unsigned blob_count = IpmiBlob::GetBlobCount();
	printf("There are %u blobs in the system:\n", blob_count);
	printf("Index blob_id\n");
	for (unsigned i=0; i<blob_count; i++) {
		std::string blob_id = IpmiBlob::EnumerateBlob(i);
		printf("%5u %s\n", i, blob_id.c_str());
	}
}

std::vector<char> ReadBlob(const std::string& blob_id) {
	printf("Reading blob %s\n", blob_id.c_str());
	IpmiBlob blob(blob_id, 0x1); // read
	blob.Open();
	std::vector<char> content = blob.Read();
	printf("This blob is %lu bytes long. Its contents are:\n", content.size());
	const int ch_per_line = g_chars_per_line;

	// Hex dump style
	int idx = 0;
	while (idx < content.size()) {
		int ub = std::min(int(content.size()), idx + ch_per_line);
		int line_end = idx + ch_per_line;
		int num_blank = line_end - ub;
		printf("%5d | ", idx);
		for (int i=idx; i<ub; i++) { printf("%02X ", (0xFF & content[i])); }
		for (int i=0; i<num_blank; i++) { printf("   "); }
		printf("| ");
		for (int i=idx; i<ub; i++) { 
			char ch = content[i];
			if (ch >= 32 && ch <= 127) { printf("%c", content[i]);  }
			else { printf("."); }
		}
		for (int i=0; i<num_blank; i++) { printf(" "); }
		printf("\n");
		idx += ch_per_line;
	}

	blob.Close();
	return content;
}

void StatBlob(const std::string& blob_id){
	struct IpmiBlob::BlobStat stat = IpmiBlob::StatBlob(blob_id);
	stat.Print();
}

void PrintHelp(char** argv) {
	printf("----------[Blob Tool for the BMC]----------\n");
	printf("Usage:\n");
	printf("%s read BLOB_NAME: read the content of a blob, from the beginning to the end (open, stat, read, close)\n", argv[0]);
	printf("%s save BLOB_NAME OUTPUT_FILE: same as above, plus save the file to OUTPUT_FILE\n", argv[0]);
	printf("%s stat BLOB_NAME: query the stat of a blob\n", argv[0]);
	printf("-------------------------------------------\n");
}

int main(int argc, char** argv) {
	char* x = getenv("VERBOSE");
	if (x && std::atoi(x) != 0) { g_verbose = true; }

	x = getenv("CHARS_PER_LINE");
	if (x) {
		g_chars_per_line = std::atoi(x);
		if (g_chars_per_line < 1) { g_chars_per_line = 1; }
	}

	int r = sd_bus_open_system(&g_bus);
	if (r < 0) { printf("Failed to connect to the system bus: %s\n", strerror(-r)); return 0; }
	printf("%s system DBus opened.\n", APP_BANNER);

	sd_bus_message* m = nullptr;
	sd_bus_error error = SD_BUS_ERROR_NULL;

	if (argc > 1) {
		std::string cmd(argv[1]);
		if (cmd == "list") { ListBlobs(); }
		else if (cmd == "help") { PrintHelp(argv); }
		else if (cmd == "read") {
			if (argc > 2) { ReadBlob(std::string(argv[2]));
			} else { printf("Please enter a blob ID to read its contents.\n"); }
		} else if (cmd == "save") {
			if (argc > 3) {
				std::vector<char> content = ReadBlob(std::string(argv[2]));
				std::fstream outfile;
				outfile.open(argv[3], std::ios::trunc | std::ios::binary);
				outfile.write(content.data(), content.size());
				if (!outfile.good()) {
					printf("Could not save file for some reason\n");
				}
				outfile.close();
				printf("Wrote %lu bytes to %s\n", content.size(), argv[3]);
			} else { printf("Please enter a blob ID and an output file name to save the blob's contents to the file.\n"); }
		} else if (cmd == "stat") {
			if (argc > 2) { StatBlob(std::string(argv[2]));
			} else { printf("Please enter a blob ID to get its stat.\n"); }
		}
	} else {
		ListBlobs();
	}
	return 0;
}
