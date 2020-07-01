// This file performs the file reading step
// Actual preprocessing is done in dbus_timeline_vis.js

function MyFloatMillisToBigIntUsec(x) {
  x = ('' + x).split('.');
  ret = BigInt(x[0]) * BigInt(1000);
  return ret;
}

// A DBus Pcap affects both the DBus view and the IPMI view
// so it's placed here instead of in {dbus,ipmi}_timeline_vis.js
//
// JSON format from dbus-pcap does not contain timestamps
// Text format from dbus-pcap does
// so we need both as input
function Preprocess_DBusPcap(data, timestamps) {
  // Also clear IPMI entries
  g_ipmi_parsed_entries = [];

  console.log('|data|=' + data.length + ', |timestamps|=' + timestamps.length);

  let ret = [];
  let in_flight = {};
  let in_flight_ipmi = {};

  for (let i = 0; i < data.length; i++) {
    const packet = data[i];

    // Fields we are interested in
    const fixed_header = packet[0];  // is an [Array(5), Array(6)]
    const payload = packet[1];
    const ty = fixed_header[0][1];
    let timestamp = timestamps[i];
    let timestamp_end = undefined;
    const IDX_TIMESTAMP_END = 8;

    let serial, path, member, iface, destination, sender;
    // Same as the format of the Dummy data set

    switch (ty) {
      case 4: {  // signal
        serial = fixed_header[0][5];
        path = fixed_header[1][0][1];
        iface = fixed_header[1][1][1];
        member = fixed_header[1][2][1];
        // signature = fixed_header[1][3][1];
        sender = fixed_header[1][4][1];
        destination = '<none>';
        timestamp_end = timestamp;
        let entry = [
          'sig', timestamp, serial, sender, destination, path, iface, member,
          timestamp_end, payload
        ];

        // Legacy IPMI interface uses signal for IPMI request
        if (iface == 'org.openbmc.HostIpmi' && member == 'ReceivedMessage') {
          console.log('Legacy IPMI interface, request');
        }

        ret.push(entry);
        break;
      }
      case 1: {  // method call
        serial = fixed_header[0][5];
        path = fixed_header[1][0][1];
        member = fixed_header[1][1][1];
        iface = fixed_header[1][2][1];
        destination = fixed_header[1][3][1];
        sender = fixed_header[1][4][1];
        let entry = [
          'mc', timestamp, serial, sender, destination, path, iface, member,
          timestamp_end, payload
        ];

        // Legacy IPMI interface uses method call for IPMI response
        if (iface == 'org.openbmc.HostIpmi' && member == 'sendMessge') {
          console.log('Legacy IPMI interface, response')
        } else if (
            iface == 'xyz.openbmc_project.Ipmi.Server' && member == 'execute') {
          let ipmi_entry = {
            netfn: payload[0],
            lun: payload[1],
            cmd: payload[2],
            request: payload[3],
            start_usec: MyFloatMillisToBigIntUsec(timestamp),
            end_usec: 0,
            response: []
          };
          in_flight_ipmi[serial] = (ipmi_entry);
        }


        ret.push(entry);
        in_flight[serial] = (entry);
        break;
      }
      case 2: {  // method reply
        let reply_serial = fixed_header[1][0][1];
        if (reply_serial in in_flight) {
          let x = in_flight[reply_serial];
          delete in_flight[reply_serial];
          x[IDX_TIMESTAMP_END] = timestamp;
        }

        if (reply_serial in in_flight_ipmi) {
          let x = in_flight_ipmi[reply_serial];
          delete in_flight_ipmi[reply_serial];
          const netfn = payload[0], cmd = payload[2];
          if (payload[3] != undefined) {
            x.response = payload[3];
          }
          x.end_usec = MyFloatMillisToBigIntUsec(timestamp);
          g_ipmi_parsed_entries.push(x);
        }
        break;
      }
    }
  }

  if (g_ipmi_parsed_entries.length > 0) UpdateLayout();
  return ret;
}
