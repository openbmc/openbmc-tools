#include <iostream>
#include <boost/asio.hpp>
#include <sdbusplus/bus.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <chrono>

#include <cstdio>
#include <boost/array.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <iostream>

#include <filesystem>
#include <fstream>
#include <string_view>

bool IS_USER_BUS = false;
sd_bus* g_bus = nullptr;

static constexpr std::string_view DUMP_PATH = "/usr/share/www/dbus_capture.json";

namespace dbuscapture
{

std::thread* dbus_cap_thd = nullptr;
std::atomic<bool> is_capturing = false;
constexpr std::string_view DUMP_PATH = "/usr/share/www/dbus_capture1.json";


void writeToCaptureDump(nlohmann::json j) {
  std::fstream uidlFile(std::string(DUMP_PATH).data(), std::fstream::in | std::fstream::out | std::fstream::app);
  if (uidlFile.is_open())
  {
    uidlFile << std::setw(1) << j << "," << std::endl;
    uidlFile.close();
  }
}

void AcquireBus() {
  int r;
	r = sd_bus_new(&g_bus);
	if (r < 0)
  {
    throw sdbusplus::exception::SdBusError(-r, "sd_bus_new");
  }

	r = sd_bus_set_monitor(g_bus, true);
	if (r < 0)
  {
    throw sdbusplus::exception::SdBusError(-r, "sd_bus_set_monitor");
  }

	r = sd_bus_negotiate_creds(g_bus, true, _SD_BUS_CREDS_ALL);
	if (r < 0)
  {
    throw sdbusplus::exception::SdBusError(-r, "sd_bus_negotiate_creds");
  }

	r = sd_bus_negotiate_timestamp(g_bus, true);
	if (r < 0)
  {
    throw sdbusplus::exception::SdBusError(-r, "sd_bus_negotiate_timestamp");
  }

	r = sd_bus_negotiate_fds(g_bus, true);
	if (r < 0)
  {
    throw sdbusplus::exception::SdBusError(-r, "sd_bus_negotiate_fds");
  }

	r = sd_bus_set_bus_client(g_bus, true);
	if (r < 0)
  {
    throw sdbusplus::exception::SdBusError(-r, "sd_bus_set_bus_client");
  }
	
	if (IS_USER_BUS)
  {
		r = sd_bus_set_address(g_bus, "haha");
		if (r < 0)
    {
      throw sdbusplus::exception::SdBusError(-r, "sd_bus_set_address");
    }
	} else
  {
		r = sd_bus_set_address(g_bus, "unix:path=/run/dbus/system_bus_socket");
		if (r < 0)
    {
      throw sdbusplus::exception::SdBusError(-r, "sd_bus_set_address");
    }
	}

	r = sd_bus_start(g_bus);
	if (r < 0) {
    throw sdbusplus::exception::SdBusError(-r, "sd_bus_start");
  }
}

void BecomeDbusMonitor() {
  int r;
  sd_bus_message* message;
  r = sd_bus_message_new_method_call(g_bus, &message,
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus.Monitoring",
        "BecomeMonitor");
  if (r < 0)
  {
    throw sdbusplus::exception::SdBusError(-r, "sd_bus_message_new_method_call");
  }

  r = sd_bus_message_open_container(message, 'a', "s");
  if (r < 0)
  {
    throw sdbusplus::exception::SdBusError(-r, "sd_bus_message_open_container");
  }

  r = sd_bus_message_close_container(message);
  if (r < 0)
  {
    throw sdbusplus::exception::SdBusError(-r, "sd_bus_message_close_container");
  }

  uint32_t flags = 0;
  r = sd_bus_message_append_basic(message, 'u', &flags);
  if (r < 0)
  {
    throw sdbusplus::exception::SdBusError(-r, "sd_bus_message_append_basic");
  }

  sd_bus_error error = SD_BUS_ERROR_NULL;
  r = sd_bus_call(g_bus, message, 0, &error, nullptr);
  if (r < 0)
  {
    throw sdbusplus::exception::SdBusError(-r, "sd_bus_call");
  }

  const char* unique_name;
  r = sd_bus_get_unique_name(g_bus, &unique_name);
  if (r < 0)
  {
    throw sdbusplus::exception::SdBusError(-r, "sd_bus_get_unique_name");
  }
}

void Capture()
{
	AcquireBus();
	BecomeDbusMonitor();
	while (is_capturing) {
    struct sd_bus_message*msg = nullptr;
    nlohmann::json j;
    int r;
    
    sd_bus_process(g_bus, &msg);

    uint8_t type;
    r = sd_bus_message_get_type(msg, &type);
    if (r >= 0)
    {
        j["type"] = std::to_string(type);
    }

    uint64_t cookie;
    r = sd_bus_message_get_cookie(msg, &cookie);
    if (r >= 0)
    {
        j["cookie"] = std::to_string(cookie);
    }

    uint64_t reply_cookie;
    r = sd_bus_message_get_reply_cookie(msg, &reply_cookie);
    if (r >= 0)
    {
        j["reply_cookie"] = std::to_string(reply_cookie);
    }

    const char *path = sd_bus_message_get_path(msg);
    if (path)
    {
        j["path"] = path;
    }

    const char *interface = sd_bus_message_get_interface(msg);
    if (interface)
    {
        j["interface"] = interface;
    }

    const char *sender = sd_bus_message_get_sender(msg);
    if (sender)
    {
        j["sender"] = sender;
    }

    const char *destination = sd_bus_message_get_destination(msg);
    if (destination)
    {
        j["destination"] = destination;
    }

    const char *member = sd_bus_message_get_member(msg);
    if (member)
    {
        j["member"] = member;
    }

    const char *signature = sd_bus_message_get_signature(msg, true);
    if (signature)
    {
        j["signature"] = signature;
    }

    auto usec = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    j["time"] = std::to_string(usec);
    writeToCaptureDump(j);
    sd_bus_wait(g_bus, (uint64_t) -1);
  }
}
} // namespace dbuscapture

using namespace boost::asio;

class session
  : public boost::enable_shared_from_this<session>
{
public:
  session(boost::asio::io_service& io_service)
    : socket_(io_service),
      io_service_(io_service)
  {
  }

  local::stream_protocol::socket& socket()
  {
    return socket_;
  }

  void start()
  {
    socket_.async_read_some(boost::asio::buffer(data_),
        boost::bind(&session::handle_read,
          shared_from_this(),
          boost::asio::placeholders::error,
          boost::asio::placeholders::bytes_transferred));
  }

  void handle_read(const boost::system::error_code& error,
      size_t bytes_transferred)
  {
    if (!error)
    {
	  std::string command(data_.begin(), data_.end());
	  if (command.compare(0, 5, "start") == 0 && dbuscapture::dbus_cap_thd == nullptr)
	  {
		  dbuscapture::is_capturing = true;
		  dbuscapture::dbus_cap_thd = new std::thread(dbuscapture::Capture);
	  } else if (command.compare(0, 4, "stop") == 0 && dbuscapture::dbus_cap_thd != nullptr)
	  {
		dbuscapture::is_capturing = false;
	  	dbuscapture::dbus_cap_thd->join();
		dbuscapture::dbus_cap_thd = nullptr;
	  }
	  socket_.async_read_some(boost::asio::buffer(data_),
          boost::bind(&session::handle_read,
            shared_from_this(),
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));
    }
  }

  void handle_write(const boost::system::error_code& error)
  {
    if (!error)
    {
          socket_.async_read_some(boost::asio::buffer(data_),
          boost::bind(&session::handle_read,
            shared_from_this(),
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));
    }
  }

private:
  local::stream_protocol::socket socket_;
  boost::array<char, 8> data_;
  boost::asio::io_service& io_service_;
};

typedef boost::shared_ptr<session> session_ptr;

class server
{
public:
  server(boost::asio::io_service& io_service, const std::string& file)
    : io_service_(io_service),
      acceptor_(io_service, local::stream_protocol::endpoint(file))
  {
    session_ptr new_session(new session(io_service_));
    acceptor_.async_accept(new_session->socket(),
        boost::bind(&server::handle_accept, this, new_session,
          boost::asio::placeholders::error));
  }

  void handle_accept(session_ptr new_session,
      const boost::system::error_code& error)
  {
    if (!error)
    {
      new_session->start();
    }

    new_session.reset(new session(io_service_));
    acceptor_.async_accept(new_session->socket(),
        boost::bind(&server::handle_accept, this, new_session,
          boost::asio::placeholders::error));
  }

private:
  boost::asio::io_service& io_service_;
  local::stream_protocol::acceptor acceptor_;
};

int main(int argc, char* argv[])
{
try
  {
    if (argc != 2)
    {
      return 1;
    }
    boost::asio::io_service io_service;

    std::remove(argv[1]);
    server s(io_service, argv[1]);

    io_service.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
/*
#else // defined(BOOST_ASIO_HAS_LOCAL_SOCKETS)
# error Local sockets not available on this platform.
#endif // defined(BOOST_ASIO_HAS_LOCAL_SOCKETS)
*/
