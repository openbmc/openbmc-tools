// This file performs the file reading step
// Actual preprocessing is done in dbus_timeline_vis.js

function MyFloatMillisToBigIntUsec(x) {
  x = ('' + x).split('.');
  ret = BigInt(x[0]) * BigInt(1000);
  return ret;
}

function OpenDBusPcapFile(file_name) {
  // First try to parse using dbus-pcap
  var dbus_pcap_out1,
      dbus_pcap_out2;  // Normal format and JSON format output from dbus-pcap
  ShowBlocker('Running dbus-pcap ...');
  const dbus_pcap = spawn("python3", ["dbus-pcap", file_name, "--json"]);
  let stdout = "";
  let num_cr = "";
  const r = new RegExp("([0-9]+/[0-9]+) [0-9]+\.[0-9]+:.*");
  dbus_pcap.stdout.on("data", (data)=>{
    stdout = stdout + data;
    const s = data.toString();
    for (let i=0; i<s.length; i++) {
      if (s[i] == '\n' || s[i] == '\r') { num_cr ++; }
    }
    
    const res = r.exec(s);
    if (res != null && res.length > 1) {
      let x = res[1].split("/");
      let n0 = parseInt(x[0]), n1 = parseInt(x[1]);
      let pct = parseInt(n0 * 100 / n1);
      ShowBlocker('Running dbus-pcap: ' + res[1] + " (" + pct + "%)");
    }
  });
  
  dbus_pcap.on("close", (code)=> {
    {
      ShowBlocker('Processing dbus-pcap output ... ');
    }
    
    // Parse now
    let lines = stdout.split("\n");
    let timestamps1 = [], packets = [];
    for (let i=0; i<lines.length; i++) {
    
      const l = lines[i].trim();
      try {
        if (l.length > 0) {
          // Timestamp
          const l0 = l.substr(l.indexOf(" "), l.indexOf(':'));
          const ts_usec = parseFloat(l0) * 1000.0;
          if (!isNaN(ts_usec)) {
            timestamps1.push(ts_usec)
          } else {
            console.log('NaN! ' + l)
          }
          
          // JSON
          packets.push(JSON.parse(l.substr(l.indexOf(":")+1)));
        }
      } catch (x) {
        console.log(x)
      }
    }
    
    Timestamps_DBus = timestamps1;
    console.log("|timestamps1|=" + timestamps1.length + ", |packets|=" + packets.length);
    
    Data_DBus = packets.slice();
    OnGroupByConditionChanged_DBus();
    const v = dbus_timeline_view;
    let preproc = Preprocess_DBusPcap(packets, timestamps1);
    let grouped = Group_DBus(preproc, v.GroupBy);
    GenerateTimeLine_DBus(grouped);
    dbus_timeline_view.IsCanvasDirty = true;
    if (dbus_timeline_view.IsEmpty() == false ||
        ipmi_timeline_view.IsEmpty() == false) {
      dbus_timeline_view.CurrentFileName = file_name;
      ipmi_timeline_view.CurrentFileName = file_name;
      HideWelcomeScreen();
      ShowDBusTimeline();
      ShowIPMITimeline();
      ShowNavigation();
      UpdateFileNamesString();
    }
    HideBlocker();
  });
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
    
    if (fixed_header == undefined) { // for hacked dbus-pcap
      console.log(packet);
      continue;
    }
    
    const payload = packet[1];
    const ty = fixed_header[0][1];
    let timestamp = timestamps[i];
    let timestamp_end = undefined;
    const IDX_TIMESTAMP_END = 8;
    const IDX_MC_OUTCOME = 9; // Outcome of method call

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
          timestamp_end, payload, ""
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
          x[IDX_MC_OUTCOME] = "ok";
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
      
      case 3: { // error reply
        let reply_serial = fixed_header[1][0][1];
        if (reply_serial in in_flight) {
          let x = in_flight[reply_serial];
          delete in_flight[reply_serial];
          x[IDX_TIMESTAMP_END] = timestamp;
          x[IDX_MC_OUTCOME] = "error";
        }
      }
    }
  }

  if (g_ipmi_parsed_entries.length > 0) UpdateLayout();
  return ret;
}
